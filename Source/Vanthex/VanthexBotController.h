#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "VanthexBotController.generated.h"

/** Fills a missing roster slot with server-authoritative navigation and combat. */
UCLASS()
class AVanthexBotController : public AController
{
	GENERATED_BODY()

public:
	AVanthexBotController();
 virtual void Tick(float DeltaSeconds) override;
private:
 TArray<FVector> Route;
 float NextRouteTime = 0.f;
 float TargetVisibleSince = 0.f;
 float NextCombatShot = 0.f;
 float NextGadgetUse=0.f;
 float BurstEnds=0.f;
 void TurnToward(const FVector& Point,float DeltaSeconds);
 void MoveSafely(const FVector& Direction,float Strength);
 TWeakObjectPtr<APawn> CombatTarget;
 void PlanRoute(const FVector& Goal);
};
