#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "HitscanWeapon.generated.h"

class AVanthexPlayerController;
class USceneComponent;
struct FCollisionQueryParams;
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Cosmetic prediction for the owning client. Zero means there is no predicted burst. */
	uint32 StartPredictedFire();
	void StopPredictedFire();
	void StartTaggedFire(uint32 Burst);
	virtual void StopFire() override;
	virtual void StartReload() override;

	/** One shot. Input auto-fire and the self-test both call this. */
	void FireShot();
 USceneComponent* GetFirstPersonPivot() const { return RiflePivot; }
 UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FRotator ViewRotation, FHitResult Hit);
 UFUNCTION(NetMulticast, Unreliable) void MulticastPelletImpacts(const TArray<FHitResult>& Hits);

	virtual void ResetForRound() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishReload() override;
 virtual void OnWeaponProfileChanged() override;
 void ContinueAutomaticFire();
 void ScheduleAutomaticFire();
 void PredictShot();
 void ClearServerPrediction();
 AVanthexPlayerController* GetPredictionController() const;
 FVector TracePellet(FHitResult& Hit, const FCollisionQueryParams& Params, const FVector& Start,
  const FRotator& View, uint32 Sequence, int32 Pellet, float MovementForSpread) const;

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
 UPROPERTY(Replicated)
 uint32 ShotSequence = 0;

 // Per-press tokens are independent of the weapon's deterministic spread sequence.
 uint32 ServerPredictionBurst = 0;
 uint32 ServerPredictionShot = 0;
 TWeakObjectPtr<AVanthexPlayerController> ServerPredictionOwner;
 FTimerHandle PredictionTimer;
 uint32 PredictionBurst = 0;
 uint32 PredictionShot = 0;
 uint32 PredictionSequence = 0;
 int32 PredictionAmmoRemaining = 0;
 float NextPredictionFireTime = 0.f;
 FName PredictionWeaponId;
 TWeakObjectPtr<AVanthexPlayerController> PredictionOwner;
 int32 BurstIndex = 0;
};
