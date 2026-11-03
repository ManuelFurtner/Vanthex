#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "HitscanWeapon.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AHitscanWeapon : public AWeaponBase
{
	GENERATED_BODY()

public:
	AHitscanWeapon();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void StartFire() override;
	virtual void StopFire() override;
	virtual void StartReload() override;

	/** One shot. Input auto-fire and the self-test both call this. */
	void FireShot();
 UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FRotator ViewRotation, FHitResult Hit);
 UFUNCTION(NetMulticast, Unreliable) void MulticastPelletImpacts(const TArray<FHitResult>& Hits);

	virtual void ResetForRound() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishReload() override;
 virtual void OnWeaponProfileChanged() override;
 void ContinueAutomaticFire();
 void ScheduleAutomaticFire();

	bool IsHeadHit(const FHitResult& Hit) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> RiflePivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FirearmParts;

	float Recoil = 0.f;
	float MuzzleTime = 0.f;

	void PresentImpact(const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> ViewMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float TraceRange = 15000.f;

	/** Cosmetic bullet configuration; subsonic weapons disable the crack independently of damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Bullet Audio")
	bool bSupersonicBullet = true;

	void PresentShotFeedback(const FRotator& ViewRotation);

	FTimerHandle FireTimer;
	float NextFireTime = 0.f;
 float LastShotTime = -100.f;
 uint32 ShotSequence = 0;
 int32 BurstIndex = 0;
};
