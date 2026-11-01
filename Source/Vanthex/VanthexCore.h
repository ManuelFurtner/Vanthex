#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexTypes.h"
#include "VanthexCore.generated.h"

class AVanthexCharacter;
class UStaticMeshComponent;

UCLASS()
class AVanthexCore : public AActor
{
	GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	AVanthexCore();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	void SetHome(const FVector& InHome);
	void ResetToSpawn();
	bool TryPickup(AVanthexCharacter* Character);
	void Drop();
	void Throw();
	void DropFromDeath();
	void SetExtracting(bool bExtracting);
	void MarkExtracted();

	EVanthexCoreState GetCoreState() const { return State; }
	AVanthexCharacter* GetCarrier() const { return Carrier; }
	EVanthexTeam GetCarrierTeam() const;
	FVector GetHome() const { return Home; }
	FVector GetVelocity() const { return Velocity; }
	bool IsAtHome() const;
	float GetThrowSpeed() const { return ThrowSpeed; }
	float GetPickupRadius() const { return PickupRadius; }
	float GetCatchRadius() const { return CatchRadius; }

private:
	void AttachToCarrier(AVanthexCharacter* Character);
	void Release(const FVector& WorldLocation, const FVector& NewVelocity, bool bAllowCatch);
	void EnterAtRest();
	void TryCatch();
	void RecoverIfOutOfBounds(float DeltaSeconds);
	void BuildPresentation();
	void UpdatePresentation(float DeltaSeconds);
	bool IsLiving(const AVanthexCharacter* Character) const;
	bool IsRoundActive() const;

	UPROPERTY(VisibleAnywhere, Category = "Core")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EnergyChamber;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EnergyRing;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TrailMeshes;

	TArray<FVector> TrailPoints;
	float PresentationTime = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Core")
	EVanthexCoreState State = EVanthexCoreState::AtRest;

	UPROPERTY(Replicated)
	TObjectPtr<AVanthexCharacter> Carrier;

	UPROPERTY()
	TObjectPtr<AVanthexCharacter> Thrower;

	UPROPERTY()
	TObjectPtr<AVanthexCharacter> PickupLock;

	FVector Home = FVector(220.f, 0.f, 50.f);
	FVector Velocity = FVector::ZeroVector;
	FVector ReleaseOrigin = FVector::ZeroVector;
	float ThrowTime = -1000.f;
	float PickupUnlockTime = -1000.f;
	float OutOfBoundsTime = 0.f;
	float PickupRadius = 200.f;
	float CatchRadius = 180.f;
	float ThrowSpeed = 2000.f;
	float DropIgnoreTime = 0.45f;
	bool bCatchEnabled = false;
};
