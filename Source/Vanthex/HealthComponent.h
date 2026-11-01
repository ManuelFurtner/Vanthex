#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexDamage.h"
#include "HealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FVanthexDeathSignature, AActor* /*DeadActor*/);

UCLASS(ClassGroup = (Vanthex), meta = (BlueprintSpawnableComponent))
class UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UHealthComponent();

	/** Only entry that changes health. Later this is the server function. */
	void ApplyDamage(const FVanthexDamageRequest& Request);

	/** Round start. Restores max health and clears the death flag. */
	void ResetHealth();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDead() const { return bDead; }
	float GetLastDamage() const { return LastDamage; }
	bool WasLastHitHeadshot() const { return bLastHitWasHeadshot; }

	FVanthexDeathSignature OnDeath;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	float Health = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	bool bDead = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	float LastDamage = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	bool bLastHitWasHeadshot = false;
};
