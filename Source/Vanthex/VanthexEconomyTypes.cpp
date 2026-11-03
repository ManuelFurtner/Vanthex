#include "VanthexEconomyTypes.h"

const TArray<FVanthexEconomyItem>& VanthexEconomy::GetCatalog()
{
	using Type = EVanthexEconomyItemType;
	static const TArray<FVanthexEconomyItem> Catalog = {
		{TEXT("Pistol"), TEXT("Standardpistole"), 0, Type::Sidearm},
		{TEXT("P250"), TEXT("P250"), 350, Type::Sidearm},
		{TEXT("MP5"), TEXT("MP5"), 1300, Type::PrimaryWeapon},
		{TEXT("MP7"), TEXT("MP7"), 1900, Type::PrimaryWeapon},
		{TEXT("M4"), TEXT("M4"), 2500, Type::PrimaryWeapon},
		{TEXT("AK"), TEXT("AK"), 2700, Type::PrimaryWeapon},
		{TEXT("AUG"), TEXT("AUG"), 2900, Type::PrimaryWeapon},
		{TEXT("DMR"), TEXT("DMR"), 3200, Type::PrimaryWeapon},
		{TEXT("Shotgun"), TEXT("Shotgun"), 1600, Type::PrimaryWeapon},
		{TEXT("ArmorLight"), TEXT("Leichte Ruestung"), 400, Type::Armor, EVanthexArmorTier::Light},
		{TEXT("ArmorHeavy"), TEXT("Schwere Ruestung"), 750, Type::Armor, EVanthexArmorTier::Heavy},
		{TEXT("Flash"), TEXT("Flashbang"), 200, Type::Gadget},
		{TEXT("Smoke"), TEXT("Smoke"), 300, Type::Gadget},
		{TEXT("Frag"), TEXT("Frag"), 350, Type::Gadget},
		{TEXT("EMP"), TEXT("EMP"), 350, Type::Gadget},
		{TEXT("Sensor"), TEXT("Sensor"), 250, Type::Gadget}
	};
	return Catalog;
}

const FVanthexEconomyItem* VanthexEconomy::FindItem(FName Id)
{
	return GetCatalog().FindByPredicate([Id](const FVanthexEconomyItem& Item) { return Item.Id == Id; });
}

float VanthexEconomy::MaximumArmor(EVanthexArmorTier Tier)
{
	return Tier == EVanthexArmorTier::Heavy ? 80.f : Tier == EVanthexArmorTier::Light ? 40.f : 0.f;
}

float VanthexEconomy::ArmorAbsorption(EVanthexArmorTier Tier)
{
	return Tier == EVanthexArmorTier::Heavy ? .4f : Tier == EVanthexArmorTier::Light ? .25f : 0.f;
}

bool VanthexEconomy::TryApplyPurchase(FVanthexEconomyState& State, FName ItemId, FString* OutError)
{
	if (OutError) OutError->Reset();
	auto Reject = [OutError](const TCHAR* Reason) { if (OutError) *OutError = Reason; return false; };
	const FVanthexEconomyItem* Item = FindItem(ItemId);
	if (!Item) return Reject(TEXT("Unbekannter Gegenstand."));
	if (State.Credits < Item->Price) return Reject(TEXT("Nicht genug Credits."));
	FVanthexEconomyState Next = State;
	switch (Item->Type)
	{
	case EVanthexEconomyItemType::PrimaryWeapon:
		if (Next.PrimaryWeapon == Item->Id) return Reject(TEXT("Diese Primaerwaffe ist bereits ausgeruestet."));
		Next.PrimaryWeapon = Item->Id;
		break;
	case EVanthexEconomyItemType::Sidearm:
		if (Next.Sidearm == Item->Id) return Reject(TEXT("Diese Pistole ist bereits ausgeruestet."));
		Next.Sidearm = Item->Id;
		break;
	case EVanthexEconomyItemType::Armor:
		if (Next.ArmorTier == EVanthexArmorTier::Heavy && Item->ArmorTier == EVanthexArmorTier::Light)
			return Reject(TEXT("Schwere Ruestung kann nicht durch leichte ersetzt werden."));
		if (Next.ArmorTier == Item->ArmorTier && Next.ArmorPoints >= MaximumArmor(Item->ArmorTier))
			return Reject(TEXT("Diese Ruestung ist bereits vollstaendig."));
		Next.ArmorTier = Item->ArmorTier;
		Next.ArmorPoints = MaximumArmor(Item->ArmorTier);
		break;
	case EVanthexEconomyItemType::Gadget:
		if (Next.Gadgets.Num() >= MaximumGadgets) return Reject(TEXT("Beide Gadget-Plaetze sind belegt."));
		Next.Gadgets.Add(Item->Id);
		break;
	default:
		return Reject(TEXT("Dieser Gegenstand kann nicht gekauft werden."));
	}
	Next.Credits -= Item->Price;
	State = MoveTemp(Next);
	return true;
}

void VanthexEconomy::AddCredits(FVanthexEconomyState& State, int32 Amount)
{
	if (Amount <= 0) return;
	State.Credits = int32(FMath::Clamp<int64>(int64(State.Credits) + int64(Amount), 0, MaximumCredits));
}

void VanthexEconomy::ResetOnDeath(FVanthexEconomyState& State)
{
	State.PrimaryWeapon = NAME_None;
	State.Sidearm = TEXT("Pistol");
	State.ArmorTier = EVanthexArmorTier::None;
	State.ArmorPoints = 0.f;
	State.Gadgets.Reset();
}

float VanthexEconomy::ApplyArmorDamage(FVanthexEconomyState& State, float Damage, bool Head, bool Melee)
{
	if (!FMath::IsFinite(Damage) || Damage <= 0.f) return 0.f;
	if (Head || Melee) return Damage;
	const float Points = FMath::IsFinite(State.ArmorPoints) ? FMath::Clamp(State.ArmorPoints, 0.f, MaximumArmor(State.ArmorTier)) : 0.f;
	const float Absorbed = FMath::Min(Points, Damage * ArmorAbsorption(State.ArmorTier));
	State.ArmorPoints = Points - Absorbed;
	return Damage - Absorbed;
}
