// AGalaSeedWorldActor — builds a drivable grey-box world from the committed seed bundle at
// runtime: terrain from the heightfield, every network street as a road ribbon, landmark
// markers, and enough light/sky to read as an autumn afternoon. Drop one into an empty level
// (or let AGalaGameMode spawn it) and press Play. This is M0 scaffolding — the real landscape,
// materials, and 1978 dressing replace it piece by piece without changing the data source.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GalaSeed.h"
#include "GalaSeedWorldActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class GALASEEDIMPORT_API AGalaSeedWorldActor : public AActor
{
	GENERATED_BODY()

public:
	AGalaSeedWorldActor();

	virtual void BeginPlay() override;

	/** Where the drive starts: spine[0] (the 5591 Gala Ave end), facing along the spine. */
	UFUNCTION(BlueprintPure, Category = "Gala")
	FTransform GetSpineStart() const;

	/** Terrain height (UE cm, world Z) at a world XY — bilinear over the seed heightfield. */
	float TerrainZAt(double WorldX, double WorldY) const;

	UPROPERTY(BlueprintReadOnly, Category = "Gala")
	bool bSeedLoaded = false;

	FGalaSeed Seed;

private:
	void BuildTerrain();
	void BuildRoads();
	void BuildLandmarks();
	void BuildLightAndSky();

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> RoadMesh;

	// heightfield extent in UE cm (X = north, Y = east), derived from bbox + origin
	double HfNorthMax = 0, HfNorthMin = 0, HfEastMin = 0, HfEastMax = 0;
};
