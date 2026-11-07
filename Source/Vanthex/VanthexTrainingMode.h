#pragma once

#include "VanthexGameMode.h"
#include "VanthexTestArena.h"
#include "VanthexTrainingMode.generated.h"

/** Runtime geometry and targets for the local range. */
UCLASS()
class AVanthexTrainingArena : public AVanthexTestArena
{
 GENERATED_BODY()

public:
 void BuildRange();

protected:
 virtual void Tick(float DeltaSeconds) override;

private:
 AVanthexTestTarget* AddTarget(const FVector& Position);
 void RespawnTarget(AActor* TargetActor);

 UPROPERTY()
 TObjectPtr<AVanthexTestTarget> MovingTarget;
};

/** Uses the normal weapon and Core rules with one local player and no bots. */
UCLASS()
class AVanthexTrainingMode : public AVanthexGameMode
{
 GENERATED_BODY()

public:
 AVanthexTrainingMode();
 virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
 virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
 virtual void BeginPlay() override;

private:
 friend void VanthexRunTrainingRangeTest(AVanthexTrainingMode*);
};
