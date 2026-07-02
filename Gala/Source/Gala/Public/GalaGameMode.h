// AGalaGameMode — grey-box bootstrap: ensures a seed world exists and puts the player in the
// proto pawn at 5591 Gala Ave. Works in any level, including a brand-new Empty Level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GalaGameMode.generated.h"

UCLASS()
class GALA_API AGalaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGalaGameMode();

	virtual void BeginPlay() override;
};
