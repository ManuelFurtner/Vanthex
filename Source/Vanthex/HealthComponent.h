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

	/** Server-only entry for validated damage and actual-HP economy events. */
	void ApplyDamage(const FVanthexDamageRequest& Request);

	/** Round start. Restores max health and clears the death flag. */
	void ResetHealth();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDead() const { return bDead; }
	float GetLastDamage() const { return LastDamage; }
	float GetLastAppliedHPDamage() const { return LastAppliedHPDamage; }
	bool WasLastHitHeadshot() const { return bLastHitWasHeadshot; }

	FVanthexDeathSignature OnDeath;

protected:
	/** Keep corpse collision in sync on the server and when death/reset replicates. */
	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	float Health = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead, VisibleAnywhere, Category = "Health")
	bool bDead = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	float LastDamage = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	float LastAppliedHPDamage = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
	bool bLastHitWasHeadshot = false;
};
