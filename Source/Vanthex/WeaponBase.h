#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

UCLASS(Abstract)
class AWeaponBase : public AActor
{
	GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	AWeaponBase();

	virtual void StartFire();
	virtual void StopFire();
	virtual void StartReload();
	void CancelReload();

	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }
	int32 GetMagazineSize() const { return MagazineSize; }
	bool IsReloading() const { return bReloading; }
	bool IsTriggerHeld() const { return bTriggerHeld; }
	float GetBodyDamage() const { return BodyDamage; }
	float GetHeadshotMultiplier() const { return HeadshotMultiplier; }
	float GetReloadDuration() const { return ReloadDuration; }
	float GetFireInterval() const { return FireInterval; }

	/** Single magazine write. Fire, reload, and the M1 self-test all go through this. */
	void SetMagazineAmmo(int32 NewAmmo);

	virtual void ResetForRound();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishReload();

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USceneComponent> WeaponRoot;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	int32 MagazineSize = 30;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Weapon")
	int32 AmmoInMagazine = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.01"))
	float FireInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float ReloadDuration = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float BodyDamage = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 2.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Weapon")
	bool bReloading = false;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	bool bTriggerHeld = false;

	FTimerHandle ReloadTimer;
};
