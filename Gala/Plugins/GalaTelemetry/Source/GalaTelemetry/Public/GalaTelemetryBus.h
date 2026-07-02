// GalaTelemetry — the telemetry bus. The single source of truth for "how the car is being driven."
// Every visual/audio system reads FGalaTelemetry (and the derived intensity) from this subsystem;
// nothing downstream reads raw physics state. This is the literal implementation of the
// "telemetry is the paint" pillar — see ARCHITECTURE.md#telemetry-bus.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GalaTelemetryBus.generated.h"

/** One frame of driving truth. Published by GalaVehicle each frame (accumulated across physics
 *  substeps); consumed by GalaPaint / GalaWorld / audio. Channel meanings: ARCHITECTURE.md. */
USTRUCT(BlueprintType)
struct GALATELEMETRY_API FGalaTelemetry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gala") float SpeedMS = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float LongAccelMS2 = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float LateralG = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float YawRateRadS = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float SlipAngleFrontDeg = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float SlipAngleRearDeg = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") bool bAtLimit = false;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float Throttle = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float Brake = 0.f;      // load-cell: force-derived
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float Clutch = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float Handbrake = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float SteeringAngleDeg = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float RackTorqueNm = 0.f;   // the FFB signal
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float RPM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float PowertrainLoad = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TArray<float> SuspensionTravel;  // FL FR RL RR
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float DistanceFromAnchorM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FVector Position = FVector::ZeroVector;
};

/** Game-instance-wide bus. GalaVehicle publishes; everyone else samples. Also owns *intensity*,
 *  the master mood value (0 serene .. 1 churning) with hysteresis + slew limiting, and the
 *  sanctum override (proximity pulls intensity toward reverent-calm; the sanctum wins). */
UCLASS()
class GALATELEMETRY_API UGalaTelemetryBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gala")
	void Publish(const FGalaTelemetry& Frame) { Latest = Frame; }

	UFUNCTION(BlueprintPure, Category = "Gala")
	const FGalaTelemetry& GetLatest() const { return Latest; }

	UFUNCTION(BlueprintPure, Category = "Gala")
	float GetIntensity() const { return Intensity; }

	/** Set each frame by GalaWorld from sanctum proximity (0 = far, 1 = inside). */
	UFUNCTION(BlueprintCallable, Category = "Gala")
	void SetSanctumProximity(float InProximity) { SanctumProximity = FMath::Clamp(InProximity, 0.f, 1.f); }

	/** Advance the eased intensity. Call once per frame (GalaPaint's driver does this).
	 *  M1 acceptance: smooth, no flicker, visibly spans serene<->churning. */
	UFUNCTION(BlueprintCallable, Category = "Gala")
	void TickIntensity(float DeltaSeconds)
	{
		const float Raw = FMath::Clamp(
			  0.55f * FMath::Min(Latest.SpeedMS / 38.f, 1.f)
			+ 0.25f * FMath::Min(FMath::Abs(Latest.LateralG) / 1.2f, 1.f)
			+ 0.15f * (Latest.bAtLimit ? 1.f : 0.f)
			+ 0.15f * Latest.Handbrake
			+ 0.10f * Latest.Brake, 0.f, 1.f);
		const float Target = Raw * (1.f - 0.85f * SanctumProximity);   // the sanctum wins
		const float Rate = (Target > Intensity) ? RiseRate : FallRate; // hysteresis: eases up slower
		Intensity = FMath::FInterpConstantTo(Intensity, Target, DeltaSeconds, Rate);
	}

private:
	FGalaTelemetry Latest;
	float Intensity = 0.f;
	float SanctumProximity = 0.f;
	float RiseRate = 0.35f;   // per-second slew limits — tune in M1, never let it flicker
	float FallRate = 0.55f;
};
