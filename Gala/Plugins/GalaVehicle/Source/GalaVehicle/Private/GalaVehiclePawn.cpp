#include "GalaVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GalaSeedWorldActor.h"
#include "GalaTelemetryBus.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

static constexpr float M_TO_CM = 100.f;
static constexpr float SUBSTEP_HZ = 360.f;   // CAR_FEEL.md: physics substep ≥ 360 Hz

AGalaVehiclePawn::AGalaVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	Chassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Chassis;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Chassis->SetStaticMesh(Cube.Object);
		Chassis->SetRelativeScale3D(FVector(5.4f, 2.0f, 1.1f));   // ~late-70s sedan box
	}

	// First person: seated cockpit reference frame (the sim-rig view). Left seat, eye height.
	DriverCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DriverCamera"));
	DriverCamera->SetupAttachment(Chassis);
	DriverCamera->SetRelativeLocation(FVector(-20.f, -42.f, 55.f));
	DriverCamera->SetFieldOfView(90.f);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Chassis);
	CameraArm->TargetArmLength = 1100.f;
	CameraArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	CameraArm->SocketOffset = FVector(0, 0, 320.f);
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 5.f;
	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(CameraArm);
	ChaseCamera->SetFieldOfView(62.f);
	ChaseCamera->SetActive(false);

	OverheadCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverheadCamera"));
	OverheadCamera->SetupAttachment(Chassis);
	OverheadCamera->SetRelativeLocationAndRotation(FVector(0, 0, 250000.f), FRotator(-90.f, 0.f, 0.f));
	OverheadCamera->SetFieldOfView(80.f);
	OverheadCamera->SetActive(false);
}

void AGalaVehiclePawn::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVThrottle", EKeys::W, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVThrottle", EKeys::Up, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVThrottle", EKeys::S, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVThrottle", EKeys::Down, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVSteer", EKeys::D, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVSteer", EKeys::Right, 1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVSteer", EKeys::A, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVSteer", EKeys::Left, -1.f));
	UPlayerInput::AddEngineDefinedAxisMapping(FInputAxisKeyMapping("GVHandbrake", EKeys::SpaceBar, 1.f));
	UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("GVReset", EKeys::R));
	UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("GVCam", EKeys::C));
	UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("GVOverhead", EKeys::V));

	Input->BindAxis("GVThrottle", this, &AGalaVehiclePawn::OnThrottle);
	Input->BindAxis("GVSteer", this, &AGalaVehiclePawn::OnSteer);
	Input->BindAxis("GVHandbrake", this, &AGalaVehiclePawn::OnHandbrake);
	Input->BindAction("GVReset", IE_Pressed, this, &AGalaVehiclePawn::OnReset);
	Input->BindAction("GVCam", IE_Pressed, this, &AGalaVehiclePawn::OnToggleCamera);
	Input->BindAction("GVOverhead", IE_Pressed, this, &AGalaVehiclePawn::OnToggleOverhead);
}

void AGalaVehiclePawn::PlaceAtStart()
{
	if (!SeedWorld) { return; }
	const FTransform T = SeedWorld->GetSpineStart();
	Sim = FGalaVehicleState{};
	Sim.PosX = T.GetLocation().X / M_TO_CM;
	Sim.PosY = T.GetLocation().Y / M_TO_CM;
	Sim.Yaw = FMath::DegreesToRadians(T.Rotator().Yaw);
	bPlaced = true;
}

void AGalaVehiclePawn::OnReset() { bPlaced = false; }

void AGalaVehiclePawn::OnToggleCamera()
{
	const bool bDriver = DriverCamera->IsActive();
	DriverCamera->SetActive(!bDriver);
	ChaseCamera->SetActive(bDriver);
	OverheadCamera->SetActive(false);
}

void AGalaVehiclePawn::OnToggleOverhead()
{
	const bool bOn = OverheadCamera->IsActive();
	OverheadCamera->SetActive(!bOn);
	DriverCamera->SetActive(bOn);
	ChaseCamera->SetActive(false);
}

void AGalaVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!SeedWorld)
	{
		TActorIterator<AGalaSeedWorldActor> It(GetWorld());
		if (It) { SeedWorld = *It; }
	}
	if (SeedWorld && SeedWorld->bSeedLoaded && !bPlaced) { PlaceAtStart(); }
	if (!bPlaced) { return; }

	// Steering: map input → front road-wheel angle, softened and speed-sensitive (a fast wheel at
	// speed is twitchy and unlike a heavy 70s box). The wheel/keys move a modelled rim; the rack
	// torque we compute is what a DD base would fight back with.
	const float SpeedMS = FMath::Abs(Sim.VelX);
	const float MaxSteer = FMath::DegreesToRadians(35.f) / (1.f + SpeedMS * 0.06f);
	const float SteerTarget = SteerRawIn * MaxSteer;
	SteerAngleRad = FMath::FInterpTo(SteerAngleRad, SteerTarget, DeltaSeconds, 6.f);

	FGalaVehicleInput In;
	In.Throttle = ThrottleIn;
	In.Brake = BrakeIn;
	In.SteerAngleRad = SteerAngleRad;
	In.Handbrake = HandbrakeIn;

	// Fixed-rate substeps, decoupled from render fps (clamped so a hitch can't spiral).
	const float H = 1.f / SUBSTEP_HZ;
	SubstepAccum = FMath::Min(SubstepAccum + DeltaSeconds, 0.1f);
	int32 Steps = 0;
	while (SubstepAccum >= H && Steps < 48) { Sim.Step(Params, In, H); SubstepAccum -= H; ++Steps; }

	// Pose: planar sim → world, terrain Z under the CG, body leans with the forces (feel cue).
	const double Xcm = Sim.PosX * M_TO_CM, Ycm = Sim.PosY * M_TO_CM;
	const float Zcm = SeedWorld->TerrainZAt(Xcm, Ycm) + 60.f;
	SetActorLocationAndRotation(FVector(Xcm, Ycm, Zcm),
	                            FRotator(0.f, FMath::RadiansToDegrees(Sim.Yaw), 0.f));
	// pitch nose-up under accel / down under brake; roll into the corner — small, just readable
	const float PitchDeg = FMath::Clamp(-Sim.AccelX * 0.12f, -4.f, 4.f);
	const float RollDeg  = FMath::Clamp(Sim.AccelY * 0.10f, -6.f, 6.f);
	Chassis->SetRelativeRotation(FRotator(PitchDeg, 0.f, RollDeg));

	// Publish to the bus — the real forces, so every downstream visual reads honest driving.
	if (UGalaTelemetryBus* Bus = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGalaTelemetryBus>() : nullptr)
	{
		FGalaTelemetry T;
		T.SpeedMS = SpeedMS;
		T.LongAccelMS2 = Sim.AccelX;
		T.LateralG = Sim.AccelY / Params.Gravity;
		T.YawRateRadS = Sim.YawRate;
		T.SlipAngleFrontDeg = FMath::RadiansToDegrees(Sim.SlipFrontRad);
		T.SlipAngleRearDeg = FMath::RadiansToDegrees(Sim.SlipRearRad);
		T.bAtLimit = Sim.bAtLimit;
		T.Throttle = ThrottleIn;
		T.Brake = BrakeIn;
		T.Handbrake = HandbrakeIn;
		T.SteeringAngleDeg = FMath::RadiansToDegrees(SteerAngleRad);
		T.RackTorqueNm = Sim.RackTorqueNm;
		T.RPM = Sim.RPM;
		T.Position = GetActorLocation();
		Bus->Publish(T);
		Bus->TickIntensity(DeltaSeconds);
	}
}
