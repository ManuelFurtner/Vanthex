#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "VanthexTypes.h"
#include "VanthexPlayerState.generated.h"

UCLASS()
class AVanthexPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	void SetTeam(EVanthexTeam NewTeam, int32 NewSlot);
	void SetAlive(bool bNewAlive);

	EVanthexTeam GetTeam() const { return Team; }
	int32 GetSpawnSlot() const { return SpawnSlot; }
	bool IsAlive() const { return bAlive; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	EVanthexTeam Team = EVanthexTeam::None;

	UPROPERTY(Replicated)
	int32 SpawnSlot = 0;

	UPROPERTY(Replicated)
	bool bAlive = true;
};
