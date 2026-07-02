// GalaSeedImport — typed view of the seed bundle + the ONE ENU->UE conversion boundary.
//
// The bundle frame is ENU meters: X=east, Y=up, Z=north (right-handed, Y-up).
// UE is Z-up, left-handed, centimeters. The mapping — chosen once, never re-flipped downstream:
//
//     UE.X (forward) = ENU.Z * 100   (north)
//     UE.Y (right)   = ENU.X * 100   (east)
//     UE.Z (up)      = ENU.Y * 100   (up)
//
// North-east-up right-handed maps onto UE's left-handed X-north/Y-east/Z-up cleanly (no mirror).
// See PIPELINE_BRIDGE.md ("Axis note").

#pragma once

#include "CoreMinimal.h"
#include "GalaSeed.generated.h"

FORCEINLINE FVector GalaEnuToUE(double EnuX, double EnuY, double EnuZ)
{
	return FVector(EnuZ * 100.0, EnuX * 100.0, EnuY * 100.0);
}

USTRUCT(BlueprintType)
struct GALASEEDIMPORT_API FGalaSeedRoad
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Gala") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FString RoadClass;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float WidthM = 9.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float LengthM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TArray<FVector> Points;  // UE cm
};

USTRUCT(BlueprintType)
struct GALASEEDIMPORT_API FGalaSeedLandmark
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FString Label;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") bool bIsRegion = false;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TArray<FVector2D> OutlineXY;  // UE cm (X=north, Y=east); Z drapes on terrain
};

USTRUCT(BlueprintType)
struct GALASEEDIMPORT_API FGalaSeedHeightfield
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Gala") int32 Nx = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") int32 Ny = 0;      // row 0 = north
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float SpacingM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float MinM = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") float MaxM = 0.f;
	TArray<uint16> Samples;                                            // row-major, normalized
};

USTRUCT(BlueprintType)
struct GALASEEDIMPORT_API FGalaSeed
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FString GitSha;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") double OriginLon = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") double OriginLat = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") double OriginElevM = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TArray<FGalaSeedRoad> Roads;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TArray<FVector> Spine;  // home -> school, UE cm
	UPROPERTY(BlueprintReadOnly, Category = "Gala") TMap<FString, FGalaSeedLandmark> Landmarks;
	UPROPERTY(BlueprintReadOnly, Category = "Gala") FGalaSeedHeightfield Heightfield;
};

/** Loads seed/gala_seed.json + heightfield.r16 into the typed structs above. Pure data — actor,
 *  spline, and landscape creation from this is the M0 in-editor task (see ROADMAP.md#m0). */
class GALASEEDIMPORT_API FGalaSeedLoader
{
public:
	/** SeedDir e.g. <repo>/seed. Returns false (with error log) on any contract violation. */
	static bool Load(const FString& SeedDir, FGalaSeed& Out);
};
