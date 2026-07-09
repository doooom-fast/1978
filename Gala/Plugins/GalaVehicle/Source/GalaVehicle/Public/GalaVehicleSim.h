// GalaVehicleSim — the car's physics, as pure math (no UObject, no engine tick). A planar
// single-track ("bicycle") dynamic model with a communicative tire slip curve, longitudinal load
// transfer, and — the point of the whole exercise — a computed steering-rack torque that becomes
// force feedback on the Moza (CAR_FEEL.md). Units are SI: metres, m/s, radians, Newtons, kg.
//
// Deliberately OURS, not Chaos: the tire forces and the self-aligning torque are the FFB signal,
// so we own them. Deferred (documented, not hidden): per-wheel left/right load transfer (this is
// front/rear only), a friction circle coupling long+lat grip, gearbox/clutch. The seams below are
// the load-bearing ones — the rack-torque path especially.

#pragma once

#include "CoreMinimal.h"

/** Tunable car — a late-70s land yacht: heavy, soft, big lazy torque, skinny forgiving tyres. */
struct GALAVEHICLE_API FGalaVehicleParams
{
	// mass / geometry
	float Mass = 1800.f;          // kg — body-on-frame heft
	float YawInertia = 3200.f;    // kg·m²
	float DistCgToFront = 1.30f;  // m (a) — front-engine → CG forward → a < b (front-heavy)
	float DistCgToRear = 1.70f;   // m (b)
	float CgHeight = 0.55f;       // m — sets longitudinal load transfer
	float Gravity = 9.81f;

	// tyre (simplified Pacejka: Fy = μ·Fz·sin(C·atan(B·α))). Period rubber = lower peak, wide
	// forgiving shoulder → the limit is easy to read through the wheel and slides are progressive.
	float TyrePeakMu = 0.95f;
	float TyreB = 7.5f;           // stiffness (lower = wider, lazier peak)
	float TyreC = 1.45f;          // shape
	float SlipAtPeakRad = 0.157f; // ~9° — where grip peaks AND pneumatic trail has collapsed

	// self-aligning torque → rack torque (the FFB signal)
	float PneumaticTrail0 = 0.11f; // m at low slip; collapses to ~0 by the grip peak → wheel lightens
	float MechanicalTrail = 0.03f; // m caster — a steady centring the trail collapse can't remove
	float SteeringRatio = 15.0f;   // kingpin torque ÷ ratio = torque at the rim

	// powertrain (lazy carburetted V8: fat low-mid, tapering top)
	float DriveForceLowV = 9000.f; // N available near idle/low speed
	float DriveForceTaper = 0.020f;// 1/(1+taper·v²) rolloff with speed
	float BrakeForceMax = 15000.f; // N at brake=1 (load-cell force curve applied upstream)
	float EngineBrakeForce = 900.f;// N at closed throttle
	float AeroDrag = 2.2f;         // N/(m/s)² → ~3.5 kN at 40 m/s
	float RollResistCoef = 0.015f;

	// handbrake: locks the rear → kills rear grip for deliberate, painterly slides
	float HandbrakeForce = 9000.f;
	float HandbrakeGripCut = 0.75f;// fraction of rear lateral grip removed when fully pulled

	float IdleRPM = 700.f;
	float RedlineRPM = 5200.f;
	float RPMPerMS = 150.f;        // crude single-gear rev mapping (gearbox is later)
};

/** One frame of driver command. Steer is the FRONT ROAD-WHEEL ANGLE in radians (the pawn maps the
 *  wheel/keys → this, applying steering ratio + speed sensitivity). Brake is force-fraction. */
struct GALAVEHICLE_API FGalaVehicleInput
{
	float Throttle = 0.f;   // 0..1
	float Brake = 0.f;      // 0..1 (already a force via the load-cell curve)
	float SteerAngleRad = 0.f;
	float Handbrake = 0.f;  // 0..1
};

/** Integrable state + the outputs the pawn reads for telemetry, camera lean, and audio. */
struct GALAVEHICLE_API FGalaVehicleState
{
	// dynamic state (body frame: X forward, Y left)
	float VelX = 0.f, VelY = 0.f;   // m/s
	float YawRate = 0.f;            // rad/s
	// world pose (planar; the pawn adds terrain Z)
	double PosX = 0.0, PosY = 0.0;  // m
	float Yaw = 0.f;                // rad (0 = +X world)

	// outputs (recomputed each step)
	float AccelX = 0.f, AccelY = 0.f;  // body-frame accelerations (load transfer + camera lean)
	float SlipFrontRad = 0.f, SlipRearRad = 0.f;
	float LoadFrontN = 0.f, LoadRearN = 0.f;
	float RackTorqueNm = 0.f;          // at the rim — normalise to the base on FFB output
	bool bAtLimit = false;
	float RPM = 700.f;

	/** Advance one fixed substep. Call at ≥360 Hz from the pawn's accumulator. */
	void Step(const FGalaVehicleParams& P, const FGalaVehicleInput& In, float Dt);
};
