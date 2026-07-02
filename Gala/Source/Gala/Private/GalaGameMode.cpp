#include "GalaGameMode.h"

#include "EngineUtils.h"
#include "GalaProtoPawn.h"
#include "GalaSeedWorldActor.h"

AGalaGameMode::AGalaGameMode()
{
	DefaultPawnClass = AGalaProtoPawn::StaticClass();
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
