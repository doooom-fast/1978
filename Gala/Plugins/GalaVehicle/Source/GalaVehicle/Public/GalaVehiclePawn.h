// AGalaVehiclePawn — drives FGalaVehicleSim: fixed-rate substepping decoupled from render, terrain
// following, first-person cockpit camera, and publishing to the telemetry bus. This replaces the
// arcade proto pawn. FFB output (rack torque → Moza) is the Windows step; here the rack torque is
// computed and published, and the body leans with the forces so the feel is legible even flat.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GalaVehicleSim.h"
#include "GalaVehiclePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class AGalaSeedWorldActor;

UCLASS()
class GALAVEHICLE_API AGalaVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AGalaVehiclePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

	// Plain C++ (not a UPROPERTY — FGalaVehicleParams is a math struct, not reflected). When we
	// tune against the Moza, promote this to a USTRUCT / data asset for live editing.
	FGalaVehicleParams Params;

private:
	void OnThrottle(float V) { ThrottleIn = FMath::Max(0.f, V); BrakeIn = FMath::Max(0.f, -V); }
	void OnSteer(float V) { SteerRawIn = V; }
	void OnHandbrake(float V) { HandbrakeIn = V; }
	void OnReset();
	void OnToggleCamera();
	void OnToggleOverhead();

	void PlaceAtStart();

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Chassis;   // gets pitch/roll lean
	UPROPERTY() TObjectPtr<UCameraComponent> DriverCamera;
	UPROPERTY() TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY() TObjectPtr<UCameraComponent> ChaseCamera;
	UPROPERTY() TObjectPtr<UCameraComponent> OverheadCamera;

	UPROPERTY() TObjectPtr<AGalaSeedWorldActor> SeedWorld;

	FGalaVehicleState Sim;
	float ThrottleIn = 0.f, BrakeIn = 0.f, SteerRawIn = 0.f, HandbrakeIn = 0.f;
	float SteerAngleRad = 0.f;   // smoothed road-wheel angle
	float SubstepAccum = 0.f;
	bool bPlaced = false;
};
