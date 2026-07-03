// AGalaProtoPawn — M0 grey-box drive. Deliberately NOT the real car: an arcade-weighty stand-in
// (lazy torque, speed-faded steering, soft settle) so the seed world is drivable on the Mac the
// day the editor opens. The real substepped tire model + rack-torque FFB replace this in
// GalaVehicle per CAR_FEEL.md — nothing here is load-bearing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GalaProtoPawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class AGalaSeedWorldActor;

UCLASS()
class GALA_API AGalaProtoPawn : public APawn
{
	GENERATED_BODY()

public:
	AGalaProtoPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

private:
	void OnThrottle(float V) { ThrottleIn = V; }
	void OnSteer(float V) { SteerIn = V; }
	void OnHandbrake(float V) { HandbrakeIn = V; }
	void OnReset();
	void OnToggleCamera();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> DriverCamera;   // default: the seat
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;   // debug chase
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY() TObjectPtr<AGalaSeedWorldActor> SeedWorld;

	float ThrottleIn = 0.f, SteerIn = 0.f, HandbrakeIn = 0.f;
	float SpeedMS = 0.f, HeadingRad = 0.f, SteerState = 0.f;
	double PosX = 0, PosY = 0, PosZ = 0;   // UE cm, integrated kinematically
	bool bPlaced = false;
};
