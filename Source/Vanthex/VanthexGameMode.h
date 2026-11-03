#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VanthexTypes.h"
#include "VanthexGameMode.generated.h"

class APlayerStart;
class AVanthexCharacter;
class AVanthexCore;
class AVanthexPlayerState;
class AVanthexTestArena;
class AVanthexTestTarget;
struct FVanthexDamageRequest;

enum class EVanthexSelfTestStage : uint8
{
	WaitForWorld,
	Sprint,
	CrouchWait,
	UncrouchWait,
	Move,
	JumpWait,
	BodyShot,
	HeadShot,
	ReloadWait,
	PlayerDeath,
	Restart,
	RestartWait,
	Team,
	FriendlyFireAim,
	FriendlyFire,
	EnemyAim,
	EnemyDamage,
	Timeout,
	TimeoutWait,
	Elimination,
	RoundReset,
	Score,
	CoreTests,
	Match,
	Equipment,
	Finish
};

enum class EVanthexCoreTest : uint8
{
	Spawn,
	PickupAlpha,
	SingleCarrier,
	ManualDrop,
	WaitDrop,
	PickupBravo,
	DropOnDeath,
	Throw,
	TeammateCatch,
	Intercept,
	Settle,
	CancelLeave,
	CancelDrop,
	CancelDeath,
	ExtractionA,
	RoundReset,
	ExtractionB,
	F5Reset,
	M1Regression,
	M1RegressionFire,
	M2Regression,
	Done
};

UCLASS()
class AVanthexGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVanthexGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void Logout(AController* Exiting) override;
	/** Transfers a departing human's live roster pawn/account before UE destroys it. */
	bool HandOverPlayerSlot(AController* Exiting);
 void RestartTest();
	void NotifyCombatantDied(AVanthexCharacter* Combatant);

	bool IsRoundActive() const { return Phase == EVanthexRoundPhase::RoundActive; }
	bool IsBuyPhase() const { return Phase == EVanthexRoundPhase::BuyPhase; }
	bool UsesLegacyCombatFixture() const;
	void RecordDamage(AVanthexCharacter* Victim, const FVanthexDamageRequest& Request, float AppliedHP, bool bKilled);
	void ClearDamageHistory(AVanthexCharacter* Victim);
	void NotifyCoreCaught(uint32 ThrowId, AVanthexCharacter* Thrower, AVanthexCharacter* Catcher, const FVector& Origin, const FVector& CatchPosition);
	float ShortestExtractionRoute(const FVector& Position) const;
	friend void VanthexRunEconomyIntegrationTest(AVanthexGameMode* Mode);
	friend void VanthexRunWeaponProfileTest(AVanthexGameMode* Mode);
	friend void VanthexRunCoreEconomyTest(AVanthexGameMode* Mode);
	friend void VanthexRunEconomyShots(AVanthexGameMode* Mode);
	bool CanDamage(const AActor* InstigatorActor, const AActor* TargetActor) const;
	AVanthexCore* GetCore() const { return Core; }
	void RefreshCoreReadout();

	static constexpr int32 MaxPlayersPerTeam = 3;
	static constexpr int32 RoundsToWin = 7;

protected:
	virtual void BeginPlay() override;

	void PerformSessionReset();
	void EnsureRoster();
	void SpawnBot(EVanthexTeam Team, int32 Slot);
	void RegisterCombatant(AVanthexCharacter* Combatant);
	void RestartMatch();
	void BeginRound();
	void ActivateRound();
	void TickBuyPhase();
	void BuyForBots();
	void SettleRoundEconomy(EVanthexTeam Winner, bool bExtraction);
	int32 EconomySlot(const AVanthexPlayerState* State) const;
	AVanthexPlayerState* EconomyStateForSlot(int32 Key) const;
	void TickRoundClock();
	void ResolveTimeout();
	void EndRound(EVanthexTeam Winner);
	void EnterIntermission();
	void EnterMatchEnded();
	void ResetCombatants();
	void PublishMatch();
	int32 CountAlive(EVanthexTeam Team) const;
	AVanthexCharacter* FindCombatant(EVanthexTeam Team, int32 Slot) const;
	AVanthexPlayerState* GetVanthexPlayerState(const AActor* Actor) const;
	void EvaluateElimination();
	void UpdateExtraction(float DeltaSeconds);
	void AdvanceCoreSelfTest();

	UFUNCTION()
	void OnRoundFlow();

	void AdvanceSelfTest();
	void AdvanceEquipmentSelfTest();
	int32 EquipmentTestStep = 0;
	float EquipmentTestDeadline = 0.f;
	UPROPERTY() TObjectPtr<AActor> EquipmentTestWall;
	void AdvanceVisualShots();
 void AdvanceMapShots();
 void AdvanceGridV3Shots();
 void AdvanceGridV3GameTest();
	void SelfTestExpect(bool bCondition, const TCHAR* Name, const FString& Detail);
	void FinishSelfTest();
	void AimAt(const FVector& Point) const;
	AVanthexCharacter* GetTestCharacter() const;
	void KillTeam(EVanthexTeam Team);
	bool AreCombatantsReady() const;

	UPROPERTY()
	TObjectPtr<AVanthexTestArena> Arena;

	UPROPERTY()
	TObjectPtr<AVanthexTestTarget> TestTarget;

	UPROPERTY()
	TObjectPtr<AVanthexCore> Core;

	UPROPERTY()
	TArray<TObjectPtr<AVanthexCharacter>> Combatants;

	EVanthexRoundPhase Phase = EVanthexRoundPhase::Waiting;
	EVanthexTeam RoundWinner = EVanthexTeam::None;
	EVanthexTeam MatchWinner = EVanthexTeam::None;
	int32 AlphaScore = 0;
	int32 BravoScore = 0;
	int32 RoundIndex = 0;
	float TimeRemaining = 0.f;
	float RoundDuration = 180.f;
	float BuyDuration = 25.f;
	int32 AlphaLossStreak = 0, BravoLossStreak = 0;
	int32 LastSettledRound = -1;
	struct FDamageCredit { int32 Attacker = -1; float HP = 0.f; double Time = 0.; };
	TMap<TWeakObjectPtr<AVanthexCharacter>, TArray<FDamageCredit>> DamageHistory;
	TSet<TWeakObjectPtr<AVanthexCharacter>> RewardedDeaths;
	TSet<uint32> ProcessedThrows;
	TMap<int32, int32> PaidPasses;
	TSet<int32> PaidPassPairs;
	TSet<int32> PaidIntercepts;
	int32 AlphaPasses = 0, BravoPasses = 0;
	float RoundStartDelay = 0.25f;
	float RoundEndDelay = 1.f;
	float IntermissionDuration = 2.f;
	float ExtractionDuration = 5.f;
	float ExtractionElapsed = 0.f;
	EVanthexExtractionId ActiveExtraction = EVanthexExtractionId::None;
	bool bAllowFriendlyFire = false;
	bool bRosterReady = false;
	bool bMatchStarted = false;
	uint8 FlowStep = 0;

	bool bSelfTest = false;
	bool bSelfTestFailed = false;
	bool bRestartPending = false;
	EVanthexSelfTestStage SelfTestStage = EVanthexSelfTestStage::WaitForWorld;
	int32 SelfTestTicks = 0;
	int32 TimeoutRoundIndex = 0;
	int32 CoreScoreBefore = 0;
	int32 CoreTestMark = 0;
	EVanthexCoreTest CoreTest = EVanthexCoreTest::Spawn;
	float MoveStartTime = 0.f;
	FTimerHandle SelfTestTimer;
	FTimerHandle VisualShotTimer;
	int32 VisualShotIndex = 0;
	uint32 VisualActionMask = 0;
	bool bVisualThrowReleased = false;
	bool bVisualExtractArmed = false;
	FTimerHandle FlowTimer;
	FVector MoveStart = FVector::ZeroVector;
	TArray<FString> SelfTestLines;
};
