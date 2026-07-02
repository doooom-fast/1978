#include "GalaProtoPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GalaSeedWorldActor.h"
#include "GalaTelemetryBus.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AGalaProtoPawn::AGalaProtoPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(270.f, 100.f, 55.f));   // ~5.4 x 2.0 x 1.1 m, X forward
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Body;

	// visible shell: engine cube scaled to the body — burnt-orange 70s slab (grey-box)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		UStaticMeshComponent* Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
		Shell->SetStaticMesh(Cube.Object);
		Shell->SetupAttachment(Body);
		Shell->SetRelativeScale3D(FVector(5.4f, 2.0f, 1.1f));
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Body);
	CameraArm->TargetArmLength = 1100.f;
	CameraArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	CameraArm->SocketOffset = FVector(0, 0, 320.f);
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 5.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	Camera->SetFieldOfView(62.f);
}

void AGalaProtoPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	// Engine-defined mappings: pure C++, no input assets needed for the grey-box.
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaThrottle", EKeys::W, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaThrottle", EKeys::Up, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaThrottle", EKeys::S, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaThrottle", EKeys::Down, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaSteer", EKeys::A, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaSteer", EKeys::Left, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaSteer", EKeys::D, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaSteer", EKeys::Right, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GalaHandbrake", EKeys::SpaceBar, 1.f));
	UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("GalaReset", EKeys::R));

	Input->BindAxis("GalaThrottle", this, &AGalaProtoPawn::OnThrottle);
	Input->BindAxis("GalaSteer", this, &AGalaProtoPawn::OnSteer);
	Input->BindAxis("GalaHandbrake", this, &AGalaProtoPawn::OnHandbrake);
	Input->BindAction("GalaReset", IE_Pressed, this, &AGalaProtoPawn::OnReset);
}

void AGalaProtoPawn::OnReset()
{
	bPlaced = false;
	SpeedMS = 0.f;
}

void AGalaProtoPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);

	if (!SeedWorld)
	{
		TActorIterator<AGalaSeedWorldActor> It(GetWorld());
		if (It) { SeedWorld = *It; }
	}
	if (SeedWorld && SeedWorld->bSeedLoaded && !bPlaced)
	{
		const FTransform Start = SeedWorld->GetSpineStart();
		PosX = Start.GetLocation().X; PosY = Start.GetLocation().Y; PosZ = Start.GetLocation().Z;
		HeadingRad = FMath::DegreesToRadians(Start.Rotator().Yaw);
		bPlaced = true;
	}
	if (!bPlaced) { return; }

	// weighty arcade: big lazy torque, drag, speed-faded steering, handbrake loosens the tail
	const float Fwd = FMath::Max(0.f, ThrottleIn), Brk = FMath::Max(0.f, -ThrottleIn);
	const float Accel = Fwd * (SpeedMS < 2.f ? 6.5f : 14.5f / (1.f + SpeedMS * 0.08f))
	                  - Brk * (SpeedMS > 0.4f ? 9.5f : -4.5f);
	SpeedMS += Accel * Dt;
	SpeedMS -= SpeedMS * (0.045f + HandbrakeIn * 0.9f) * Dt + FMath::Sign(SpeedMS) * 0.25f * Dt;
	SpeedMS = FMath::Clamp(SpeedMS, -8.f, 46.f);
	const float SteerTarget = SteerIn * (0.55f / (1.f + FMath::Abs(SpeedMS) * 0.075f));
	SteerState = FMath::FInterpTo(SteerState, SteerTarget, Dt, 7.f);
	HeadingRad += SteerState * SpeedMS * 0.30f * Dt * (HandbrakeIn > 0.5f ? 1.6f : 1.f);

	PosX += FMath::Cos(HeadingRad) * SpeedMS * 100.f * Dt;
	PosY += FMath::Sin(HeadingRad) * SpeedMS * 100.f * Dt;

	// settle onto whatever is below (road ribbon or terrain), soft like long-travel suspension
	FHitResult Hit;
	const FVector From(PosX, PosY, PosZ + 800.0), To(PosX, PosY, PosZ - 4000.0);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility))
	{
		PosZ = FMath::FInterpTo(PosZ, Hit.Location.Z + 55.f, Dt, 8.f);
	}

	SetActorLocationAndRotation(FVector(PosX, PosY, PosZ),
	                            FRotator(0.f, FMath::RadiansToDegrees(HeadingRad), 0.f));

	// publish to the bus — even the grey-box speaks the real contract
	if (UGalaTelemetryBus* Bus = GetGameInstance()->GetSubsystem<UGalaTelemetryBus>())
	{
		FGalaTelemetry T;
		T.SpeedMS = FMath::Abs(SpeedMS);
		T.Throttle = Fwd;
		T.Brake = Brk;
		T.Handbrake = HandbrakeIn;
		T.SteeringAngleDeg = FMath::RadiansToDegrees(SteerState);
		T.YawRateRadS = SteerState * SpeedMS * 0.30f;
		T.Position = GetActorLocation();
		Bus->Publish(T);
		Bus->TickIntensity(Dt);
	}
}
