#include "GalaGameMode.h"

#include "EngineUtils.h"
#include "GalaSeedWorldActor.h"
#include "GalaVehiclePawn.h"

AGalaGameMode::AGalaGameMode()
{
	// The real substepped car (GalaVehicle). The arcade GalaProtoPawn stays in the tree as a
	// fallback but is no longer the default.
	DefaultPawnClass = AGalaVehiclePawn::StaticClass();
}

void AGalaGameMode::BeginPlay()
{
	Super::BeginPlay();
	TActorIterator<AGalaSeedWorldActor> It(GetWorld());
	if (!It)  // none placed in the level -> spawn one
	{
		GetWorld()->SpawnActor<AGalaSeedWorldActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}
