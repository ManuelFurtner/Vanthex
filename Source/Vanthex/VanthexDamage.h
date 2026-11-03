#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class AController;

enum class EVanthexDamageKind : uint8 { Environment, Bullet, Melee };

/** Single damage submission. Hitscan fills it; HealthComponent is the only applier. */
struct FVanthexDamageRequest
{
	AActor* InstigatorActor = nullptr;
	AController* InstigatorController = nullptr;
	float Damage = 0.f;
	EVanthexDamageKind DamageKind = EVanthexDamageKind::Environment;
	bool bHeadshot = false;
	FHitResult Hit;
};
