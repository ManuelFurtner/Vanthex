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
	int32 GetFlagCaptures() const { return FlagCaptures; }
	int32 GetFlagReturns() const { return FlagReturns; }
	int32 GetCorePasses() const { return CorePasses; }
	int32 GetCoreIntercepts() const { return CoreIntercepts; }
	void RecordKill() { if(HasAuthority()) { ++Kills; ForceNetUpdate(); } }
	void RecordDeath() { if(HasAuthority()) { ++Deaths; ForceNetUpdate(); } }
	void RecordAssist() { if(HasAuthority()) { ++Assists; ForceNetUpdate(); } }
	void RecordFlagCapture() { if(HasAuthority()) { ++FlagCaptures; ForceNetUpdate(); } }
	void RecordFlagReturn() { if(HasAuthority()) { ++FlagReturns; ForceNetUpdate(); } }
	void RecordCorePass() { if(HasAuthority()) { ++CorePasses; ForceNetUpdate(); } }
	void RecordCoreIntercept() { if(HasAuthority()) { ++CoreIntercepts; ForceNetUpdate(); } }
	const FVanthexEconomyState& GetEconomy() const { return Economy; }
	void ResetEconomyForMatch();
 void GiveCTFLoadout();
 void SetRespawnTime(float Time) { if(HasAuthority()) { RespawnTime=Time; ForceNetUpdate(); } }
 float GetRespawnTime() const { return RespawnTime; }
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
 UPROPERTY(Replicated) float RespawnTime=0.f;
	UPROPERTY(Replicated) int32 Kills = 0;
	UPROPERTY(Replicated) int32 Deaths = 0;
	UPROPERTY(Replicated) int32 Assists = 0;
	UPROPERTY(Replicated) int32 FlagCaptures = 0;
	UPROPERTY(Replicated) int32 FlagReturns = 0;
	UPROPERTY(Replicated) int32 CorePasses = 0;
	UPROPERTY(Replicated) int32 CoreIntercepts = 0;
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
