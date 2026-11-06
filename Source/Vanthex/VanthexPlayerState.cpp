#include "VanthexPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexGameState.h"
#include "HealthComponent.h"
#include "Engine/World.h"

void AVanthexPlayerState::SetTeam(EVanthexTeam NewTeam, int32 NewSlot)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Team != EVanthexTeam::None && NewTeam != Team)
	{
		const auto* Match = GetWorld() ? GetWorld()->GetGameState<AVanthexGameState>() : nullptr;
		if (Match && Match->GetPhase() != EVanthexRoundPhase::Waiting && Match->GetPhase() != EVanthexRoundPhase::MatchEnded) return;
	}
	Team = NewTeam;
	SpawnSlot = NewSlot;
}

void AVanthexPlayerState::SetAlive(bool bNewAlive)
{
	if (!HasAuthority())
	{
		return;
	}

	bAlive = bNewAlive;
}

void AVanthexPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVanthexPlayerState, RespawnTime);
	DOREPLIFETIME(AVanthexPlayerState, Team);
	DOREPLIFETIME(AVanthexPlayerState, SpawnSlot);
	DOREPLIFETIME(AVanthexPlayerState, bAlive);
	DOREPLIFETIME(AVanthexPlayerState, Kills);
	DOREPLIFETIME(AVanthexPlayerState, Deaths);
	DOREPLIFETIME(AVanthexPlayerState, Assists);
	DOREPLIFETIME(AVanthexPlayerState, FlagCaptures);
	DOREPLIFETIME(AVanthexPlayerState, FlagReturns);
	DOREPLIFETIME(AVanthexPlayerState, CorePasses);
	DOREPLIFETIME(AVanthexPlayerState, CoreIntercepts);
	DOREPLIFETIME_CONDITION(AVanthexPlayerState, Economy, COND_OwnerOnly);
}

void AVanthexPlayerState::ResetEconomyForMatch()
{
	if (!HasAuthority()) return;
	Economy = FVanthexEconomyState();
	Kills = Deaths = Assists = 0;
	FlagCaptures = FlagReturns = CorePasses = CoreIntercepts = 0;
	LastPurchaseRequest = 0;
	LastPurchaseError.Reset();
	ForceNetUpdate();
}

void AVanthexPlayerState::CopyEconomyFrom(const AVanthexPlayerState* PreviousOwner)
{
	if (!HasAuthority() || !PreviousOwner) return;
	Economy = PreviousOwner->Economy;
 RespawnTime=PreviousOwner->RespawnTime;
	Kills = PreviousOwner->Kills; Deaths = PreviousOwner->Deaths; Assists = PreviousOwner->Assists;
	FlagCaptures = PreviousOwner->FlagCaptures;
	FlagReturns = PreviousOwner->FlagReturns;
	CorePasses = PreviousOwner->CorePasses;
	CoreIntercepts = PreviousOwner->CoreIntercepts;
	LastPurchaseRequest = 0;
	LastPurchaseError.Reset();
	ForceNetUpdate();
}

bool AVanthexPlayerState::TryPurchase(FName ItemId, uint32 RequestId)
{
	LastPurchaseError.Reset();
	auto Reject = [this](const TCHAR* Reason) { LastPurchaseError = Reason; return false; };
	if (!HasAuthority()) return Reject(TEXT("Kaeufe werden vom Server bestaetigt."));
	if (RequestId == 0 || RequestId <= LastPurchaseRequest) return Reject(TEXT("Doppelte oder veraltete Kaufanfrage."));
	LastPurchaseRequest = RequestId;
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AVanthexGameMode>() : nullptr;
	if (!Mode || !Mode->IsBuyPhase()) return Reject(TEXT("Kaeufe sind nur in der Kaufphase moeglich."));
	const auto* Character = Cast<AVanthexCharacter>(GetPawn());
	if (!bAlive || !Character || !Character->GetController() || Character->GetPlayerState<AVanthexPlayerState>() != this ||
		!Character->GetHealthComponent() || Character->GetHealthComponent()->IsDead() ||
		(Team != EVanthexTeam::Alpha && Team != EVanthexTeam::Bravo))
		return Reject(TEXT("Nur ein lebender Spieler im Team kann einkaufen."));
	if (!VanthexEconomy::TryApplyPurchase(Economy, ItemId, &LastPurchaseError)) return false;
	ForceNetUpdate();
	return true;
}

void AVanthexPlayerState::AwardCredits(int32 Amount)
{
	if (!HasAuthority()) return;
	VanthexEconomy::AddCredits(Economy, Amount);
	ForceNetUpdate();
}

void AVanthexPlayerState::HandleEconomyDeath()
{
	if (!HasAuthority()) return;
	VanthexEconomy::ResetOnDeath(Economy);
	ForceNetUpdate();
}

bool AVanthexPlayerState::ConsumeGadget(int32 Slot,FName Expected)
{
 if(!HasAuthority()||!Economy.Gadgets.IsValidIndex(Slot)||Economy.Gadgets[Slot]!=Expected)return false;
 Economy.Gadgets.RemoveAt(Slot);ForceNetUpdate();return true;
}

float AVanthexPlayerState::ApplyArmorDamage(float Damage, bool Head, bool Melee)
{
	if (!HasAuthority()) return Damage;
	const float Remaining = VanthexEconomy::ApplyArmorDamage(Economy, Damage, Head, Melee);
	ForceNetUpdate();
	return Remaining;
}

void AVanthexPlayerState::GiveCTFLoadout()
{
 if(!HasAuthority()) return;
 Economy.PrimaryWeapon=TEXT("M4"); Economy.Sidearm=TEXT("Pistol");
 Economy.ArmorTier=EVanthexArmorTier::Light; Economy.ArmorPoints=VanthexEconomy::MaximumArmor(Economy.ArmorTier);
 Economy.Gadgets.Reset(); ForceNetUpdate();
}
