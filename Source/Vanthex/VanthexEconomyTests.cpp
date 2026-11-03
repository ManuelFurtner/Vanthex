#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VanthexEconomyTypes.h"
#include "VanthexDamage.h"
#include <limits>

namespace
{
constexpr EAutomationTestFlags EconomyTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

bool SameInventory(const FVanthexEconomyState& A, const FVanthexEconomyState& B)
{
	return A.Credits == B.Credits && A.PrimaryWeapon == B.PrimaryWeapon && A.Sidearm == B.Sidearm &&
		A.ArmorTier == B.ArmorTier && A.ArmorPoints == B.ArmorPoints && A.Gadgets == B.Gadgets;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEconomyCatalog, "Vanthex.Economy.CatalogAndDefaults", EconomyTestFlags)
bool FVanthexEconomyCatalog::RunTest(const FString& Parameters)
{
	using namespace VanthexEconomy;
	const FVanthexEconomyState State;
	TestEqual(TEXT("New match starts with 800 credits"), State.Credits, 800);
	TestTrue(TEXT("No primary, free pistol, no armor or gadgets"), State.PrimaryWeapon.IsNone() && State.Sidearm == FName(TEXT("Pistol")) &&
		State.ArmorTier == EVanthexArmorTier::None && State.ArmorPoints == 0.f && State.Gadgets.IsEmpty());
	const TPair<const TCHAR*, int32> Prices[] = {
		{TEXT("Pistol"), 0}, {TEXT("P250"), 350}, {TEXT("MP5"), 1300}, {TEXT("MP7"), 1900},
		{TEXT("M4"), 2500}, {TEXT("AK"), 2700}, {TEXT("AUG"), 2900}, {TEXT("DMR"), 3200},
		{TEXT("Shotgun"), 1600}, {TEXT("ArmorLight"), 400}, {TEXT("ArmorHeavy"), 750},
		{TEXT("Flash"), 200}, {TEXT("Smoke"), 300}, {TEXT("Frag"), 350}, {TEXT("EMP"), 350}, {TEXT("Sensor"), 250}
	};
	TestEqual(TEXT("Catalog contains all sixteen items"), GetCatalog().Num(), 16);
	TSet<FName> Ids;
	for (const FVanthexEconomyItem& Item : GetCatalog())
	{
		TestFalse(TEXT("Item IDs are unique"), Ids.Contains(Item.Id));
		TestFalse(TEXT("Item has a display name"), Item.DisplayName.IsEmpty());
		Ids.Add(Item.Id);
	}
	for (const auto& Expected : Prices)
	{
		const auto* Item = FindItem(FName(Expected.Key));
		TestNotNull(FString::Printf(TEXT("Catalog has %s"), Expected.Key), Item);
		if (Item) TestEqual(FString::Printf(TEXT("Exact price of %s"), Expected.Key), Item->Price, Expected.Value);
	}
	TestTrue(TEXT("P250 occupies sidearm slot"), FindItem(TEXT("P250"))->Type == EVanthexEconomyItemType::Sidearm);
	TestTrue(TEXT("AUG occupies primary slot"), FindItem(TEXT("AUG"))->Type == EVanthexEconomyItemType::PrimaryWeapon);
	TestTrue(TEXT("Heavy armor carries its tier"), FindItem(TEXT("ArmorHeavy"))->ArmorTier == EVanthexArmorTier::Heavy);
	TestTrue(TEXT("EMP occupies gadget slot only"), FindItem(TEXT("EMP"))->Type == EVanthexEconomyItemType::Gadget);
	TestNull(TEXT("Unknown item cannot resolve to a fallback purchase"), FindItem(TEXT("InventedWeapon")));
	TestTrue(TEXT("Unspecified damage defaults to environment"), FVanthexDamageRequest().DamageKind == EVanthexDamageKind::Environment);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEconomyPurchases, "Vanthex.Economy.AtomicPurchases", EconomyTestFlags)
bool FVanthexEconomyPurchases::RunTest(const FString& Parameters)
{
	using namespace VanthexEconomy;
	FVanthexEconomyState State;
	State.Credits = 349;
	FVanthexEconomyState Before = State;
	FString Error;
	TestFalse(TEXT("One credit short is rejected"), TryApplyPurchase(State, TEXT("P250"), &Error));
	TestFalse(TEXT("Rejected purchase supplies reason"), Error.IsEmpty());
	TestTrue(TEXT("Insufficient funds leaves complete state unchanged"), SameInventory(State, Before));
	TestFalse(TEXT("Unknown ID is rejected"), TryApplyPurchase(State, TEXT("ArbitraryClientId"), &Error));
	TestTrue(TEXT("Unknown ID leaves complete state unchanged"), SameInventory(State, Before));
	State.Credits = 350;
	TestTrue(TEXT("Exact funds buys P250"), TryApplyPurchase(State, TEXT("P250"), &Error));
	TestTrue(TEXT("Successful purchase clears prior error"), Error.IsEmpty());
	TestTrue(TEXT("P250 costs exactly 350 and replaces pistol"), State.Credits == 0 && State.Sidearm == FName(TEXT("P250")));
	State.Credits = 7000;
	TestTrue(TEXT("Buy first primary"), TryApplyPurchase(State, TEXT("M4")));
	TestEqual(TEXT("M4 debits exact price"), State.Credits, 4500);
	TestTrue(TEXT("Buy replacement primary"), TryApplyPurchase(State, TEXT("AUG")));
	TestTrue(TEXT("Replacement pays full price without refund; sidearm stays"), State.Credits == 1600 &&
		State.PrimaryWeapon == FName(TEXT("AUG")) && State.Sidearm == FName(TEXT("P250")));
	State.Credits = 7000;
	Before = State;
	TestFalse(TEXT("Redundant weapon purchase is rejected"), TryApplyPurchase(State, TEXT("AUG")));
	TestTrue(TEXT("Redundant weapon purchase is atomic"), SameInventory(State, Before));
	TestTrue(TEXT("First Flash fits"), TryApplyPurchase(State, TEXT("Flash")));
	TestTrue(TEXT("Second identical Flash fits"), TryApplyPurchase(State, TEXT("Flash")));
	TestTrue(TEXT("Identical gadgets occupy separate slots and charge twice"), State.Gadgets.Num() == 2 &&
		State.Gadgets[0] == FName(TEXT("Flash")) && State.Gadgets[1] == FName(TEXT("Flash")) && State.Credits == 6600);
	Before = State;
	TestFalse(TEXT("Third gadget is rejected"), TryApplyPurchase(State, TEXT("Smoke")));
	TestTrue(TEXT("Full gadget inventory rejection does not debit"), SameInventory(State, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEconomyArmorPurchase, "Vanthex.Economy.ArmorPurchaseRules", EconomyTestFlags)
bool FVanthexEconomyArmorPurchase::RunTest(const FString& Parameters)
{
	using namespace VanthexEconomy;
	FVanthexEconomyState State;
	State.Credits = 7000;
	TestTrue(TEXT("Light armor purchase"), TryApplyPurchase(State, TEXT("ArmorLight")));
	TestTrue(TEXT("Light armor has 40 points and costs 400"), State.ArmorTier == EVanthexArmorTier::Light && State.ArmorPoints == 40.f && State.Credits == 6600);
	FVanthexEconomyState Before = State;
	TestFalse(TEXT("Full same-tier armor cannot be bought again"), TryApplyPurchase(State, TEXT("ArmorLight")));
	TestTrue(TEXT("Full armor rejection is atomic"), SameInventory(State, Before));
	State.ArmorPoints = 39.5f;
	TestTrue(TEXT("Damaged light armor can be replaced"), TryApplyPurchase(State, TEXT("ArmorLight")));
	TestTrue(TEXT("Even slight damage refills for full 400 price"), State.ArmorPoints == 40.f && State.Credits == 6200);
	TestTrue(TEXT("Light to heavy upgrade allowed"), TryApplyPurchase(State, TEXT("ArmorHeavy")));
	TestTrue(TEXT("Upgrade replaces points and costs full 750"), State.ArmorTier == EVanthexArmorTier::Heavy && State.ArmorPoints == 80.f && State.Credits == 5450);
	Before = State;
	TestFalse(TEXT("Full heavy armor replacement rejected"), TryApplyPurchase(State, TEXT("ArmorHeavy")));
	TestTrue(TEXT("Full heavy rejection is atomic"), SameInventory(State, Before));
	State.ArmorPoints = 0.f;
	Before = State;
	TestFalse(TEXT("Even exhausted heavy armor cannot downgrade"), TryApplyPurchase(State, TEXT("ArmorLight")));
	TestTrue(TEXT("Downgrade rejection is atomic"), SameInventory(State, Before));
	TestTrue(TEXT("Exhausted heavy armor can be replaced"), TryApplyPurchase(State, TEXT("ArmorHeavy")));
	TestTrue(TEXT("Heavy replacement refills for full 750"), State.ArmorPoints == 80.f && State.Credits == 4700);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEconomyArmorDamage, "Vanthex.Economy.ArmorDamageBoundaries", EconomyTestFlags)
bool FVanthexEconomyArmorDamage::RunTest(const FString& Parameters)
{
	using namespace VanthexEconomy;
	FVanthexEconomyState State;
	auto Check = [this, &State](const TCHAR* Label, EVanthexArmorTier Tier, float Points, float Damage, bool Head, bool Melee, float HP, float Remaining)
	{
		State.ArmorTier = Tier;
		State.ArmorPoints = Points;
		const float Applied = ApplyArmorDamage(State, Damage, Head, Melee);
		TestTrue(FString::Printf(TEXT("%s HP damage"), Label), FMath::IsNearlyEqual(Applied, HP));
		TestTrue(FString::Printf(TEXT("%s remaining armor"), Label), FMath::IsNearlyEqual(State.ArmorPoints, Remaining));
	};
	Check(TEXT("40 body versus light"), EVanthexArmorTier::Light, 40.f, 40.f, false, false, 30.f, 30.f);
	Check(TEXT("40 body versus heavy"), EVanthexArmorTier::Heavy, 80.f, 40.f, false, false, 24.f, 64.f);
	Check(TEXT("40 body versus five points"), EVanthexArmorTier::Heavy, 5.f, 40.f, false, false, 35.f, 0.f);
	Check(TEXT("80 head bypasses heavy"), EVanthexArmorTier::Heavy, 80.f, 80.f, true, false, 80.f, 80.f);
	Check(TEXT("50 knife bypasses heavy"), EVanthexArmorTier::Heavy, 80.f, 50.f, false, true, 50.f, 80.f);
	Check(TEXT("Fractions are preserved"), EVanthexArmorTier::Light, 40.f, 1.5f, false, false, 1.125f, 39.625f);
	Check(TEXT("Exhausted armor absorbs nothing"), EVanthexArmorTier::Heavy, 0.f, 40.f, false, false, 40.f, 0.f);
	Check(TEXT("No armor absorbs nothing"), EVanthexArmorTier::None, 0.f, 40.f, false, false, 40.f, 0.f);
	State.ArmorTier = EVanthexArmorTier::Heavy;
	State.ArmorPoints = 80.f;
	for (float Invalid : {0.f, -40.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		TestEqual(TEXT("Invalid damage does not heal or damage"), ApplyArmorDamage(State, Invalid, false, false), 0.f);
		TestEqual(TEXT("Invalid damage leaves armor unchanged"), State.ArmorPoints, 80.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEconomyLifecycle, "Vanthex.Economy.CreditCapAndDeath", EconomyTestFlags)
bool FVanthexEconomyLifecycle::RunTest(const FString& Parameters)
{
	using namespace VanthexEconomy;
	FVanthexEconomyState State;
	AddCredits(State, 200);
	TestEqual(TEXT("Ordinary credit award is exact"), State.Credits, 1000);
	AddCredits(State, 6100);
	TestEqual(TEXT("Credit cap discards overflow"), State.Credits, 7000);
	State.Credits = 6999;
	AddCredits(State, MAX_int32);
	TestEqual(TEXT("Large awards cannot integer-overflow the cap"), State.Credits, 7000);
	AddCredits(State, -100);
	AddCredits(State, 0);
	TestEqual(TEXT("Zero and negative awards cannot debit credits"), State.Credits, 7000);
	State.PrimaryWeapon = TEXT("AUG");
	State.Sidearm = TEXT("P250");
	State.ArmorTier = EVanthexArmorTier::Heavy;
	State.ArmorPoints = 15.75f;
	State.Gadgets = {TEXT("EMP"), TEXT("Smoke")};
	ResetOnDeath(State);
	TestEqual(TEXT("Death retains earned credits"), State.Credits, 7000);
	TestTrue(TEXT("Death loses purchased equipment and restores free pistol"), State.PrimaryWeapon.IsNone() &&
		State.Sidearm == FName(TEXT("Pistol")) && State.ArmorTier == EVanthexArmorTier::None && State.ArmorPoints == 0.f && State.Gadgets.IsEmpty());
	ResetOnDeath(State);
	TestEqual(TEXT("Repeated death handling cannot grant start credits"), State.Credits, 7000);
	State = FVanthexEconomyState();
	TestEqual(TEXT("A new match, unlike death, resets to 800"), State.Credits, StartingCredits);
	return true;
}

#endif
