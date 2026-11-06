#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexEquipmentComponent.generated.h"

class AVanthexCharacter;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FVanthexDroppedWeapon;

UENUM()
enum class EVanthexWeaponSlot : uint8 { Rifle, Knife, Pistol };

/** One sampled pose drives first-person equipment, world equipment and hand IK. */
struct FVanthexEquipmentPresentation
{
	EVanthexWeaponSlot VisibleSlot = EVanthexWeaponSlot::Rifle;
	float Lower = 0.f;
	float Swing = 0.f;
};

// One replicated record keeps selection and its animation clock consistent.
USTRUCT()
struct FVanthexEquipmentState
{
	GENERATED_BODY()
	UPROPERTY() EVanthexWeaponSlot Previous = EVanthexWeaponSlot::Rifle;
	UPROPERTY() EVanthexWeaponSlot Selected = EVanthexWeaponSlot::Rifle;
	UPROPERTY() float SwitchStart = -100.f;
	UPROPERTY() float SwitchDuration = 0.55f;
	UPROPERTY() float SwingStart = -100.f;
	UPROPERTY() float SwingDuration = 0.6f;
};

/** Selection, holster/draw presentation and server-owned melee. */
UCLASS(ClassGroup=(Vanthex), meta=(BlueprintSpawnableComponent))
class UVanthexEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UVanthexEquipmentComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SelectRifle();
 void SelectPistol();
	void SelectKnife();
	void CycleWeapon(int32 Direction);
 void SelectRoundWeapon();
 void SyncLoadout(FName PrimaryId, FName SidearmId, bool bRefillAmmo);
 bool HasSlot(EVanthexWeaponSlot Slot) const;
 FName GetSlotWeaponId(EVanthexWeaponSlot Slot) const;
 bool DropSelectedWeapon();
 void DropWeaponsOnDeath();
 bool TakeDroppedWeapon(const FVanthexDroppedWeapon& Data);
	void Attack();
	void ResetEquipment();
	bool CanUseRifle() const;
	bool IsSwitching() const;
	bool IsSwinging() const;
	EVanthexWeaponSlot GetSelectedSlot() const { return State.Selected; }
	FVanthexEquipmentPresentation GetPresentation() const;
	USceneComponent* GetWorldKnifeRoot() const { return WorldKnifeRoot; }
 USceneComponent* GetFirstPersonKnifeRoot() const { return KnifeRoot; }

	UPROPERTY(EditDefaultsOnly, Category="Equipment", meta=(ClampMin="0.1")) float SwitchDuration = 0.55f;
	UPROPERTY(EditDefaultsOnly, Category="Knife", meta=(ClampMin="0.1")) float SwingDuration = 0.6f;
	UPROPERTY(EditDefaultsOnly, Category="Knife", meta=(ClampMin="0.01")) float StrikeDelay = 0.14f;
	UPROPERTY(EditDefaultsOnly, Category="Knife", meta=(ClampMin="1")) float KnifeRange = 160.f;
	UPROPERTY(EditDefaultsOnly, Category="Knife", meta=(ClampMin="0")) float KnifeDamage = 50.f;
	// Optional authored replacement, local +X along blade, pivot at grip.
	UPROPERTY(EditDefaultsOnly, Category="Knife") TObjectPtr<UStaticMesh> KnifeMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(Server, Reliable) void ServerSelect(EVanthexWeaponSlot Slot);
	UFUNCTION(Server, Reliable) void ServerCycleWeapon(int32 Direction);
	UFUNCTION(Server, Reliable) void ServerAttack();
	UPROPERTY(Replicated) FVanthexEquipmentState State;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> KnifeRoot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> WorldKnifeRoot;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> WorldKnifeParts;
	void Select(EVanthexWeaponSlot Slot);
	void ResolveKnifeStrike();
	void UpdatePresentation();
	void BuildKnife();
	bool CanAct() const;
	bool CanSelect() const;
	float ServerTime() const;
	AVanthexCharacter* Character() const;
	FTimerHandle StrikeTimer;
 FTimerHandle SwitchTimer;
 struct FSlotAmmo { FName Id; int32 Magazine=0; int32 Reserve=0; };
 FSlotAmmo PrimaryAmmo, SidearmAmmo;
 void CaptureAmmo();
 void ApplySelectedWeapon();
 void CommitSwitch();
 uint32 AudioActionSequence=0, SwitchActionId=0;
 void PublishCarriedWeapons();
 void FinishInventoryTransfer(EVanthexWeaponSlot Slot);
 bool bDeathWeaponsDropped = false;
 float NextTransferTime = 0.f;
};
