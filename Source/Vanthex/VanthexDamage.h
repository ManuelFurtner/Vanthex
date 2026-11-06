#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class AController;

enum class EVanthexDamageKind : uint8 { Environment, Bullet, Melee, Explosion };

/** Single damage submission. Hitscan fills it; HealthComponent is the only applier. */
struct FVanthexDamageRequest
{
	AActor* InstigatorActor = nullptr;
	AController* InstigatorController = nullptr;
	float Damage = 0.f;
	EVanthexDamageKind DamageKind = EVanthexDamageKind::Environment;
	FName WeaponId;
	bool bHeadshot = false;
	FHitResult Hit;
	/** Firearms aggregate hit feedback across all pellets in the shot. */
	bool bDeferHitMarker = false;
};
