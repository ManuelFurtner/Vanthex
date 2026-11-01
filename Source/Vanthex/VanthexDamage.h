#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class AController;

/** Single damage submission. Hitscan fills it; HealthComponent is the only applier. */
struct FVanthexDamageRequest
{
	AActor* InstigatorActor = nullptr;
	AController* InstigatorController = nullptr;
	float Damage = 0.f;
	bool bHeadshot = false;
	FHitResult Hit;
};
