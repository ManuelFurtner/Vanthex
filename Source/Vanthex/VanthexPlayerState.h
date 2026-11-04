#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "VanthexTypes.h"
#include "VanthexEconomyTypes.h"
#include "VanthexPlayerState.generated.h"

UCLASS()
class AVanthexPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	void SetTeam(EVanthexTeam NewTeam, int32 NewSlot);
	void SetAlive(bool bNewAlive);

	EVanthexTeam GetTeam() const { return Team; }
	int32 GetSpawnSlot() const { return SpawnSlot; }
	bool IsAlive() const { return bAlive; }
	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }
	int32 GetAssists() const { return Assists; }
	void RecordKill() { if(HasAuthority()) { ++Kills; ForceNetUpdate(); } }
	void RecordDeath() { if(HasAuthority()) { ++Deaths; ForceNetUpdate(); } }
	void RecordAssist() { if(HasAuthority()) { ++Assists; ForceNetUpdate(); } }
	const FVanthexEconomyState& GetEconomy() const { return Economy; }
	void ResetEconomyForMatch();
	void CopyEconomyFrom(const AVanthexPlayerState* PreviousOwner);
	bool TryPurchase(FName ItemId, uint32 RequestId);
	const FString& GetLastPurchaseError() const { return LastPurchaseError; }
	void AwardCredits(int32 Amount);
	void HandleEconomyDeath();
 bool ConsumeGadget(int32 Slot,FName Expected);
	float GetArmorPoints() const { return Economy.ArmorPoints; }
	float ApplyArmorDamage(float Damage, bool Head, bool Melee);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated) int32 Kills = 0;
	UPROPERTY(Replicated) int32 Deaths = 0;
	UPROPERTY(Replicated) int32 Assists = 0;
	UPROPERTY(Replicated)
	FVanthexEconomyState Economy;
	// Reliable requests are ordered per owner. A high-water mark rejects replay with bounded memory.
	uint32 LastPurchaseRequest = 0;
	FString LastPurchaseError;
	UPROPERTY(Replicated)
	EVanthexTeam Team = EVanthexTeam::None;

	UPROPERTY(Replicated)
	int32 SpawnSlot = 0;

	UPROPERTY(Replicated)
	bool bAlive = true;
};
