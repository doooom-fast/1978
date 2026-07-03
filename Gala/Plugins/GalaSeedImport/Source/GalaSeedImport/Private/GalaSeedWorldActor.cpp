#include "GalaSeedWorldActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGalaWorld, Log, All);

namespace
{
	// Grey-box palette: autumn, not final art. Terrain olive->rust by height, asphalt near-black.
	FLinearColor TerrainColor(float H01)
	{
		const FLinearColor Olive(0.16f, 0.14f, 0.06f), Rust(0.30f, 0.16f, 0.07f), Gold(0.42f, 0.30f, 0.12f);
		return H01 < 0.5f ? FMath::Lerp(Olive, Rust, H01 * 2.f) : FMath::Lerp(Rust, Gold, H01 * 2.f - 1.f);
	}
}

AGalaSeedWorldActor::AGalaSeedWorldActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	TerrainMesh->SetupAttachment(RootComponent);
	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Roads"));
	RoadMesh->SetupAttachment(RootComponent);

	// The default surface material ignores vertex color (renders as grey grid). The engine's
	// debug vertex-color material shows the baked palette — unlit, but readable. Grey-box only.
	static ConstructorHelpers::FObjectFinder<UMaterial> VertexColorMat(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial"));
	if (VertexColorMat.Succeeded())
	{
		TerrainMesh->SetMaterial(0, VertexColorMat.Object);
		RoadMesh->SetMaterial(0, VertexColorMat.Object);
	}
}

void AGalaSeedWorldActor::BeginPlay()
{
	Super::BeginPlay();

	// Gala/ lives inside the repo; the bundle is committed at <repo>/seed.
	const FString SeedDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("../seed")));
	if (!FGalaSeedLoader::Load(SeedDir, Seed))
	{
		UE_LOG(LogGalaWorld, Error, TEXT("seed bundle failed to load from %s"), *SeedDir);
		return;
	}
	bSeedLoaded = true;

	BuildTerrain();
	BuildRoads();
	BuildLandmarks();
	BuildLightAndSky();

	// -GalaAutoShot: dump what the player sees to Saved/Screenshots a few seconds after the
	// world builds — the headless visual-verification hook for agent sessions.
	if (FParse::Param(FCommandLine::Get(), TEXT("GalaAutoShot")))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, []()
		{
			FScreenshotRequest::RequestScreenshot(TEXT("gala_autoshot.png"), false, false);
		}, 10.f, false);
	}
}

void AGalaSeedWorldActor::BuildTerrain()
{
	const FGalaSeedHeightfield& HF = Seed.Heightfield;
	// bbox (lon/lat) -> UE cm around the projection origin. Simple meters-per-degree here is
	// within centimeters of the pipeline's projection at this extent; roads carry their own
	// exact Y, terrain is the backdrop they sit on.
	// (bbox order in the bundle: south, west, north, east.)
	const double MLat = 111132.0, MLon = 111132.0 * FMath::Cos(FMath::DegreesToRadians(Seed.OriginLat));
	HfEastMin = (HF.West - Seed.OriginLon) * MLon * 100.0;
	HfEastMax = (HF.East - Seed.OriginLon) * MLon * 100.0;
	HfNorthMax = (HF.North - Seed.OriginLat) * MLat * 100.0;   // row 0 = north
	HfNorthMin = (HF.South - Seed.OriginLat) * MLat * 100.0;

	TArray<FVector> V; V.Reserve(HF.Nx * HF.Ny);
	TArray<FLinearColor> C; C.Reserve(HF.Nx * HF.Ny);
	TArray<int32> I; I.Reserve((HF.Nx - 1) * (HF.Ny - 1) * 6);
	const float Span = FMath::Max(1.f, HF.MaxM - HF.MinM);
	for (int32 j = 0; j < HF.Ny; ++j)
	{
		for (int32 i = 0; i < HF.Nx; ++i)
		{
			const float Hm = HF.MinM + HF.Samples[j * HF.Nx + i] * Span / 65535.f;
			const double X = HfNorthMax - j * (HfNorthMax - HfNorthMin) / (HF.Ny - 1);  // north -> +X
			const double Y = HfEastMin + i * (HfEastMax - HfEastMin) / (HF.Nx - 1);     // east  -> +Y
			V.Add(FVector(X, Y, (Hm - Seed.OriginElevM) * 100.0));
			C.Add(TerrainColor((Hm - HF.MinM) / Span));
		}
	}
	for (int32 j = 0; j < HF.Ny - 1; ++j)
	{
		for (int32 i = 0; i < HF.Nx - 1; ++i)
		{
			const int32 A = j * HF.Nx + i, B = A + 1, D = A + HF.Nx, E2 = D + 1;
			// Double-sided (both windings): grey-box terrain is visible from any angle, so a
			// winding mistake can never hide the ground. Unlit vertex-color material, so the
			// backfaces cost nothing but robustness. Revisit with proper normals at M1 look dev.
			I.Append({A, D, B,  A, B, D,   B, D, E2,  B, E2, D});
		}
	}
	TerrainMesh->CreateMeshSection_LinearColor(0, V, I, {}, {}, C, {}, /*collision*/ true);
	UE_LOG(LogGalaWorld, Log, TEXT("terrain: %dx%d grid"), HF.Nx, HF.Ny);
}

void AGalaSeedWorldActor::BuildRoads()
{
	TArray<FVector> V;
	TArray<int32> I;
	TArray<FLinearColor> C;
	// Roads are DRAPED on the terrain surface (sampled per vertex), not placed at their own
	// centerline elevation. Two consequences that fix the "floating lasagna": every street sits
	// ON the ground, and streets meeting at a junction sample the SAME terrain point, so they
	// meet at the same height instead of stacking. Real per-road grade returns with the proper
	// road mesh in M1; for the grey-box, drape-and-connect is what reads as a neighborhood.
	constexpr float LiftCm = 12.f;   // just clear of z-fighting with the ground
	const FLinearColor Asphalt(0.05f, 0.045f, 0.04f);
	for (const FGalaSeedRoad& R : Seed.Roads)
	{
		if (R.Points.Num() < 2) { continue; }
		const float HalfW = FMath::Max(2.5f, R.WidthM * 0.5f) * 100.f;
		const int32 Base = V.Num();
		for (int32 i = 0; i < R.Points.Num(); ++i)
		{
			const FVector& P = R.Points[i];
			const FVector& A = R.Points[FMath::Max(0, i - 1)];
			const FVector& B = R.Points[FMath::Min(R.Points.Num() - 1, i + 1)];
			FVector Dir = B - A; Dir.Z = 0;
			Dir = Dir.GetSafeNormal(1.e-3, FVector::ForwardVector);
			const FVector Side(-Dir.Y * HalfW, Dir.X * HalfW, 0);
			const FVector L = P + Side, Rt = P - Side;
			V.Add(FVector(L.X, L.Y, TerrainZAt(L.X, L.Y) + LiftCm));
			V.Add(FVector(Rt.X, Rt.Y, TerrainZAt(Rt.X, Rt.Y) + LiftCm));
			C.Add(Asphalt); C.Add(Asphalt);
			if (i > 0)
			{
				const int32 Q = Base + (i - 1) * 2;   // Q=Lprev Q+1=Rprev Q+2=Lcur Q+3=Rcur
				// double-sided quad — winding-proof (see BuildTerrain)
				I.Append({Q, Q + 2, Q + 1,  Q, Q + 1, Q + 2,
				          Q + 1, Q + 2, Q + 3,  Q + 1, Q + 3, Q + 2});
			}
		}
	}
	RoadMesh->CreateMeshSection_LinearColor(0, V, I, {}, {}, C, {}, /*collision*/ true);
	UE_LOG(LogGalaWorld, Log, TEXT("roads: %d edges, %d verts (draped on terrain)"),
	       Seed.Roads.Num(), V.Num());
}

void AGalaSeedWorldActor::BuildLandmarks()
{
	for (const auto& Pair : Seed.Landmarks)
	{
		const FGalaSeedLandmark& Lm = Pair.Value;
		if (Lm.OutlineXY.Num() == 0) { continue; }
		FVector2D Centroid(0, 0);
		for (const FVector2D& P : Lm.OutlineXY) { Centroid += P; }
		Centroid /= Lm.OutlineXY.Num();
		UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
		Text->SetupAttachment(RootComponent);
		Text->SetText(FText::FromString(Lm.Label));
		Text->SetWorldSize(900.f);
		Text->SetTextRenderColor(FColor(243, 228, 200));
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetWorldLocation(FVector(Centroid.X, Centroid.Y,
		                               TerrainZAt(Centroid.X, Centroid.Y) + 2500.f));
		Text->RegisterComponent();
	}
}

void AGalaSeedWorldActor::BuildLightAndSky()
{
	// Low autumn sun from the southwest, warm; enough sky to read the world. Placeholder for
	// the real art-directed sky (VISION.md) — grey-box only.
	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this);
	Sun->SetupAttachment(RootComponent);
	Sun->SetIntensity(6.f);
	Sun->SetLightColor(FLinearColor(1.f, 0.82f, 0.58f));
	Sun->SetWorldRotation(FRotator(-16.f, 40.f, 0.f));   // long shadows
	Sun->SetAtmosphereSunLight(true);
	Sun->RegisterComponent();

	USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(this);
	Atmo->SetupAttachment(RootComponent);
	Atmo->RegisterComponent();

	USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(this);
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetIntensity(1.2f);
	SkyLight->bRealTimeCapture = true;
	SkyLight->RegisterComponent();
}

FTransform AGalaSeedWorldActor::GetSpineStart() const
{
	if (Seed.Spine.Num() < 2) { return FTransform::Identity; }
	const FVector A = Seed.Spine[0], B = Seed.Spine[FMath::Min(8, Seed.Spine.Num() - 1)];
	const FRotator Facing = (B - A).GetSafeNormal().Rotation();
	return FTransform(FRotator(0, Facing.Yaw, 0), A + FVector(0, 0, 120.f));
}

float AGalaSeedWorldActor::TerrainZAt(double WorldX, double WorldY) const
{
	const FGalaSeedHeightfield& HF = Seed.Heightfield;
	if (HF.Nx < 2 || HF.Ny < 2) { return 0.f; }
	const double U = (WorldY - HfEastMin) / (HfEastMax - HfEastMin) * (HF.Nx - 1);
	const double W = (HfNorthMax - WorldX) / (HfNorthMax - HfNorthMin) * (HF.Ny - 1);
	const int32 i = FMath::Clamp((int32)U, 0, HF.Nx - 2);
	const int32 j = FMath::Clamp((int32)W, 0, HF.Ny - 2);
	const double Fu = FMath::Clamp(U - i, 0.0, 1.0), Fw = FMath::Clamp(W - j, 0.0, 1.0);
	const float Span = FMath::Max(1.f, HF.MaxM - HF.MinM);
	auto H = [&](int32 a, int32 b) { return HF.MinM + HF.Samples[b * HF.Nx + a] * Span / 65535.f; };
	const double Hm = (H(i, j) * (1 - Fu) + H(i + 1, j) * Fu) * (1 - Fw)
	                + (H(i, j + 1) * (1 - Fu) + H(i + 1, j + 1) * Fu) * Fw;
	return (Hm - Seed.OriginElevM) * 100.0;
}
