#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VanthexWeaponAudioProfile.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

UENUM(BlueprintType)
enum class EVanthexWeaponAudioEvent : uint8
{
 Shot, Mechanic, ReloadStart, ReloadMagazineOut, ReloadMagazineIn, ReloadBolt, ReloadEnd, ReloadCancel,
 DryFire, Draw, Holster, KnifeSwing, EquipmentMove, PurchaseAccepted, PurchaseRejected, BuyPhaseStart, RoundStart
};

USTRUCT(BlueprintType)
struct FVanthexWeaponSoundSet
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TArray<TObjectPtr<USoundBase>> Variants;
 UPROPERTY(EditAnywhere) float Volume = .4f;
};

USTRUCT(BlueprintType)
struct FVanthexWeaponSoundBank
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) TMap<EVanthexWeaponAudioEvent, FVanthexWeaponSoundSet> Events;
};

/** Original mono sources; hard references ensure all selected weapon variants cook. */
UCLASS(BlueprintType)
class UVanthexWeaponAudioProfile : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere) TMap<FName, FVanthexWeaponSoundBank> Weapons;
 UPROPERTY(EditAnywhere) TMap<EVanthexWeaponAudioEvent, FVanthexWeaponSoundSet> UI;
 /** One mixed foot+gear action, identical across all armor tiers and durability values. */
 UPROPERTY(EditAnywhere) FVanthexWeaponSoundSet Movement;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> RoomTail;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundAttenuation> Attenuation;
 UPROPERTY(EditAnywhere) TObjectPtr<USoundConcurrency> Concurrency;
 UPROPERTY(EditAnywhere) int32 MaxVoices = 24;
 UPROPERTY(EditAnywhere) int32 MaxPacketsPerSecond = 128;
};
