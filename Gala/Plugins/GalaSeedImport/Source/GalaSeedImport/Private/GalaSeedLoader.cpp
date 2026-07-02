// FGalaSeedLoader — parse the committed seed bundle. Contract: PIPELINE_BRIDGE.md.
// Any deviation from the contract is a hard, logged failure — never silently improvise geometry.

#include "GalaSeed.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGalaSeed, Log, All);

static bool ParsePoints(const TArray<TSharedPtr<FJsonValue>>& In, TArray<FVector>& Out)
{
	Out.Reserve(In.Num());
	for (const TSharedPtr<FJsonValue>& V : In)
	{
		const TArray<TSharedPtr<FJsonValue>>* Xyz = nullptr;
		if (!V->TryGetArray(Xyz) || Xyz->Num() != 3)
		{
			return false;
		}
		Out.Add(GalaEnuToUE((*Xyz)[0]->AsNumber(), (*Xyz)[1]->AsNumber(), (*Xyz)[2]->AsNumber()));
	}
	return true;
}

bool FGalaSeedLoader::Load(const FString& SeedDir, FGalaSeed& Out)
{
	FString Raw;
	const FString JsonPath = FPaths::Combine(SeedDir, TEXT("gala_seed.json"));
	if (!FFileHelper::LoadFileToString(Raw, *JsonPath))
	{
		UE_LOG(LogGalaSeed, Error, TEXT("missing %s"), *JsonPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogGalaSeed, Error, TEXT("gala_seed.json did not parse"));
		return false;
	}

	const TSharedPtr<FJsonObject> Origin = Root->GetObjectField(TEXT("origin"));
	Out.OriginLon = Origin->GetNumberField(TEXT("lon"));
	Out.OriginLat = Origin->GetNumberField(TEXT("lat"));
	Out.OriginElevM = Origin->GetNumberField(TEXT("elev_m"));
	Out.GitSha = Root->GetObjectField(TEXT("provenance"))->GetStringField(TEXT("git_sha"));

	for (const TSharedPtr<FJsonValue>& RV : Root->GetArrayField(TEXT("roads")))
	{
		const TSharedPtr<FJsonObject> R = RV->AsObject();
		FGalaSeedRoad Road;
		Road.Id = (int32)R->GetNumberField(TEXT("id"));
		R->TryGetStringField(TEXT("name"), Road.Name);
		R->TryGetStringField(TEXT("road_class"), Road.RoadClass);
		double W = 9.0;
		if (R->TryGetNumberField(TEXT("width_m"), W)) { Road.WidthM = (float)W; }
		Road.LengthM = (float)R->GetNumberField(TEXT("length_m"));
		if (!ParsePoints(R->GetArrayField(TEXT("points_xyz_m")), Road.Points))
		{
			UE_LOG(LogGalaSeed, Error, TEXT("road %d: malformed points"), Road.Id);
			return false;
		}
		Out.Roads.Add(MoveTemp(Road));
	}

	if (!ParsePoints(Root->GetObjectField(TEXT("spine"))->GetArrayField(TEXT("points_xyz_m")), Out.Spine))
	{
		UE_LOG(LogGalaSeed, Error, TEXT("spine: malformed points"));
		return false;
	}

	for (const auto& Pair : Root->GetObjectField(TEXT("landmarks"))->Values)
	{
		const TSharedPtr<FJsonObject> L = Pair.Value->AsObject();
		FGalaSeedLandmark Lm;
		Lm.Label = L->GetStringField(TEXT("label"));
		Lm.bIsRegion = L->GetStringField(TEXT("kind")) == TEXT("region");
		for (const TSharedPtr<FJsonValue>& PV : L->GetArrayField(TEXT("points_xz_m")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Xz = PV->AsArray();
			// ENU (x=east, z=north) meters -> UE (X=north, Y=east) cm; height drapes on terrain.
			Lm.OutlineXY.Add(FVector2D(Xz[1]->AsNumber() * 100.0, Xz[0]->AsNumber() * 100.0));
		}
		// UE 5.8: FJsonObject keys are TSharedString<char16_t>, not FString — convert explicitly
		Out.Landmarks.Add(FString(Pair.Key.Len(), *Pair.Key), MoveTemp(Lm));
	}

	const TSharedPtr<FJsonObject> HF = Root->GetObjectField(TEXT("heightfield"));
	Out.Heightfield.Nx = (int32)HF->GetNumberField(TEXT("nx"));
	Out.Heightfield.Ny = (int32)HF->GetNumberField(TEXT("ny"));
	Out.Heightfield.SpacingM = (float)HF->GetNumberField(TEXT("spacing_m"));
	Out.Heightfield.MinM = (float)HF->GetNumberField(TEXT("min_m"));
	Out.Heightfield.MaxM = (float)HF->GetNumberField(TEXT("max_m"));
	const TArray<TSharedPtr<FJsonValue>>& Bbox = HF->GetArrayField(TEXT("bbox_swne"));
	if (Bbox.Num() != 4)
	{
		UE_LOG(LogGalaSeed, Error, TEXT("heightfield bbox_swne must have 4 entries"));
		return false;
	}
	Out.Heightfield.South = Bbox[0]->AsNumber();
	Out.Heightfield.West = Bbox[1]->AsNumber();
	Out.Heightfield.North = Bbox[2]->AsNumber();
	Out.Heightfield.East = Bbox[3]->AsNumber();

	TArray<uint8> Bytes;
	const FString R16Path = FPaths::Combine(SeedDir, HF->GetStringField(TEXT("file")));
	if (!FFileHelper::LoadFileToArray(Bytes, *R16Path))
	{
		UE_LOG(LogGalaSeed, Error, TEXT("missing %s"), *R16Path);
		return false;
	}
	const int64 Expected = (int64)Out.Heightfield.Nx * Out.Heightfield.Ny * 2;
	if (Bytes.Num() != Expected)
	{
		UE_LOG(LogGalaSeed, Error, TEXT("heightfield.r16 is %d bytes, contract says %lld"),
		       Bytes.Num(), Expected);
		return false;
	}
	Out.Heightfield.Samples.SetNumUninitialized(Out.Heightfield.Nx * Out.Heightfield.Ny);
	FMemory::Memcpy(Out.Heightfield.Samples.GetData(), Bytes.GetData(), Bytes.Num());  // uint16 LE

	UE_LOG(LogGalaSeed, Log, TEXT("seed loaded: %d roads, spine %d pts, %d landmarks, grid %dx%d (sha %s)"),
	       Out.Roads.Num(), Out.Spine.Num(), Out.Landmarks.Num(),
	       Out.Heightfield.Nx, Out.Heightfield.Ny, *Out.GitSha.Left(12));
	return true;
}

#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, GalaSeedImport);
