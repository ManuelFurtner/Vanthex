#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "VanthexBotController.generated.h"

/** Stands in for a missing roster slot. No combat AI. */
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
 TWeakObjectPtr<APawn> CombatTarget;
 void PlanRoute(const FVector& Goal);
};
