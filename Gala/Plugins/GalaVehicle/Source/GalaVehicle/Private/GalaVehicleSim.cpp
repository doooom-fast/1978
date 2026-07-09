#include "GalaVehicleSim.h"

// Simplified Pacejka lateral force: Fy = μ·Fz·sin(C·atan(B·α)). Rises, peaks near SlipAtPeak,
// falls off past it — the shape a driver reads as "the limit."
static float TyreFy(const FGalaVehicleParams& P, float SlipRad, float FzN, float GripScale)
{
	const float Mu = P.TyrePeakMu * GripScale;
	return Mu * FzN * FMath::Sin(P.TyreC * FMath::Atan(P.TyreB * SlipRad));
}

void FGalaVehicleState::Step(const FGalaVehicleParams& P, const FGalaVehicleInput& In, float Dt)
{
	const float L = P.DistCgToFront + P.DistCgToRear;
	const float g = P.Gravity;
	const float delta = In.SteerAngleRad;

	// Forward speed used to normalise slip angles; guarded so it can't blow up near standstill.
	const float VxAbs = FMath::Abs(VelX);
	const float VxGuard = FMath::Max(VxAbs, 1.5f);
	// Below walking pace, lateral tyre dynamics are meaningless (and numerically explosive), so
	// fade them in. The car still drives off from rest; it just doesn't "corner" at 0.3 m/s.
	const float SpeedBlend = FMath::Clamp((VxAbs - 0.6f) / 2.0f, 0.f, 1.f);

	// --- slip angles (front/rear), body frame X fwd, Y left --------------------------------------
	SlipFrontRad = (FMath::Atan2(VelY + P.DistCgToFront * YawRate, VxGuard) - delta) * SpeedBlend;
	SlipRearRad  =  FMath::Atan2(VelY - P.DistCgToRear  * YawRate, VxGuard) * SpeedBlend;

	// --- vertical loads with longitudinal transfer (uses last step's AccelX) ---------------------
	const float StaticF = P.Mass * g * P.DistCgToRear / L;   // front carries the rear lever arm
	const float StaticR = P.Mass * g * P.DistCgToFront / L;
	const float Transfer = P.Mass * AccelX * P.CgHeight / L; // +accel → load shifts rearward
	LoadFrontN = FMath::Max(0.f, StaticF - Transfer);
	LoadRearN  = FMath::Max(0.f, StaticR + Transfer);

	// --- lateral tyre forces ---------------------------------------------------------------------
	const float HbGrip = 1.f - P.HandbrakeGripCut * In.Handbrake;   // handbrake unsticks the rear
	const float FyF = -TyreFy(P, SlipFrontRad, LoadFrontN, 1.f);
	const float FyR = -TyreFy(P, SlipRearRad,  LoadRearN,  HbGrip);

	// --- longitudinal force (rear drive; brakes both; drag/roll/engine-brake oppose motion) ------
	const float SgnV = (VelX >= 0.f) ? 1.f : -1.f;
	const float Drive = In.Throttle * P.DriveForceLowV / (1.f + P.DriveForceTaper * VelX * VelX);
	const float Brake = In.Brake * P.BrakeForceMax + In.Handbrake * P.HandbrakeForce;
	const float Drag  = P.AeroDrag * VelX * VxAbs;
	const float Roll  = (VxAbs > 0.05f) ? P.RollResistCoef * P.Mass * g * SgnV : 0.f;
	const float EngBrake = (1.f - In.Throttle) * P.EngineBrakeForce * FMath::Clamp(VxAbs / 4.f, 0.f, 1.f) * SgnV;
	float Fx = Drive - Drag - Roll - EngBrake - Brake * SgnV;
	// Don't let brakes drive the car backwards within a single stop.
	if (VxAbs < 0.3f && In.Throttle < 0.05f) { Fx = FMath::Min(Fx, 0.f) * (VelX > 0 ? 1.f : -1.f); }

	// --- equations of motion (single-track) ------------------------------------------------------
	const float sinD = FMath::Sin(delta), cosD = FMath::Cos(delta);
	const float VxDot = (Fx - FyF * sinD) / P.Mass + VelY * YawRate;
	const float VyDot = (FyF * cosD + FyR) / P.Mass - VelX * YawRate;
	const float RDot  = (P.DistCgToFront * FyF * cosD - P.DistCgToRear * FyR) / P.YawInertia;

	// semi-implicit Euler
	VelX += VxDot * Dt;
	VelY += VyDot * Dt;
	YawRate += RDot * Dt;

	// bring a near-stopped car fully to rest (kills residual creep/jitter)
	if (FMath::Abs(VelX) < 0.15f && In.Throttle < 0.05f) { VelX *= 0.5f; VelY *= 0.5f; YawRate *= 0.5f; }

	AccelX = VxDot - VelY * YawRate;   // stored for next step's load transfer + camera pitch
	AccelY = VyDot + VelX * YawRate;   // lateral accel → camera roll, "lateral G"

	// --- integrate world pose --------------------------------------------------------------------
	const float c = FMath::Cos(Yaw), s = FMath::Sin(Yaw);
	PosX += (VelX * c - VelY * s) * Dt;
	PosY += (VelX * s + VelY * c) * Dt;
	Yaw += YawRate * Dt;

	// --- self-aligning torque → rack torque (THE FFB SIGNAL) -------------------------------------
	// Pneumatic trail collapses toward zero as slip approaches the grip peak: the wheel goes light
	// exactly when the front tyres are about to let go. That lightening is the signal a sim driver
	// catches a slide by. Mechanical (caster) trail remains for a steady centre.
	const float TrailFrac = FMath::Clamp(1.f - FMath::Abs(SlipFrontRad) / P.SlipAtPeakRad, 0.f, 1.f);
	const float Trail = P.PneumaticTrail0 * TrailFrac + P.MechanicalTrail;
	const float KingpinTorque = FyF * Trail;                 // N·m about the kingpin (front lumped)
	RackTorqueNm = -KingpinTorque / P.SteeringRatio;         // at the rim; FFB layer scales to base

	bAtLimit = FMath::Abs(SlipFrontRad) > P.SlipAtPeakRad || FMath::Abs(SlipRearRad) > P.SlipAtPeakRad;
	RPM = FMath::Clamp(P.IdleRPM + VxAbs * P.RPMPerMS, P.IdleRPM, P.RedlineRPM);
}
