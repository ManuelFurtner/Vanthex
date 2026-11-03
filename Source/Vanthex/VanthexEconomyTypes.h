#pragma once

#include "CoreMinimal.h"
#include "VanthexEconomyTypes.generated.h"

UENUM()
enum class EVanthexArmorTier : uint8 { None, Light, Heavy };

UENUM()
enum class EVanthexEconomyItemType : uint8 { PrimaryWeapon, Sidearm, Armor, Gadget };

struct FVanthexEconomyItem
{
	FName Id;
	FString DisplayName;
	int32 Price = 0;
	EVanthexEconomyItemType Type = EVanthexEconomyItemType::PrimaryWeapon;
	EVanthexArmorTier ArmorTier = EVanthexArmorTier::None;
};

/** Private roster inventory. Only its owning connection receives this state. */
USTRUCT()
struct FVanthexEconomyState
{
	GENERATED_BODY()
	UPROPERTY() int32 Credits = 800;
	UPROPERTY() FName PrimaryWeapon = NAME_None;
	UPROPERTY() FName Sidearm = TEXT("Pistol");
	UPROPERTY() EVanthexArmorTier ArmorTier = EVanthexArmorTier::None;
	UPROPERTY() float ArmorPoints = 0.f;
	UPROPERTY() TArray<FName> Gadgets;
};

namespace VanthexEconomy
{
	constexpr int32 StartingCredits = 800;
	constexpr int32 MaximumCredits = 7000;
	constexpr int32 MaximumGadgets = 2;
	const TArray<FVanthexEconomyItem>& GetCatalog();
	const FVanthexEconomyItem* FindItem(FName Id);
	float MaximumArmor(EVanthexArmorTier Tier);
	float ArmorAbsorption(EVanthexArmorTier Tier);
	/** Validates against a copy and commits only when every inventory/price rule passes. */
	bool TryApplyPurchase(FVanthexEconomyState& State, FName ItemId, FString* OutError = nullptr);
	void AddCredits(FVanthexEconomyState& State, int32 Amount);
	void ResetOnDeath(FVanthexEconomyState& State);
	/** Returns HP damage after absorption and deducts exactly the absorbed armor points. */
	float ApplyArmorDamage(FVanthexEconomyState& State, float Damage, bool Head, bool Melee);
}
