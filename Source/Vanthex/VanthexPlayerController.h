#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexPlayerController.generated.h"

class UVanthexBuyMenuComponent;
class UVanthexModerationComponent;
class UVanthexCommunicationComponent;
class AVanthexCharacter;

UCLASS()
class AVanthexPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVanthexPlayerController();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 AVanthexCharacter* GetSpectatedTeammate() const;
 UVanthexCommunicationComponent* GetCommunication() const { return Communication; }
 UVanthexBulletAudioComponent* GetBulletAudio() const { return BulletAudio; }
 UFUNCTION(Client, Unreliable) void ClientBulletAudio(const FVanthexBulletAudioPacket& Packet);
 UVanthexWeaponAudioComponent* GetWeaponAudio() const { return WeaponAudio; }
 UVanthexBuyMenuComponent* GetBuyMenu() const { return BuyMenu; }
 UVanthexModerationComponent* GetModeration() const { return Moderation; }
 UFUNCTION(Client, Unreliable) void ClientWeaponAudio(const FVanthexWeaponAudioPacket& Packet);

	/** Local test restart. Later this is a server round restart. */
	UFUNCTION(Exec)
	void RestartTest();
 bool IsSessionMenuOpen() const { return SessionMenu.IsValid(); }
 bool IsFrontEnd() const;
 void StartGame(bool bCaptureTheFlag, bool bListen = false);
 void StartTraining();
 bool IsScoreboardOpen() const { return bScoreboardHeld; }
 const FVector* GetTeamPingLocation() const;
 void ShowScoreboard() { bScoreboardHeld=true; }
 void HideScoreboard() { bScoreboardHeld=false; }
 // Only the owning client displays provisional hits. The server reports each shot outcome.
 uint32 BeginHitPrediction();
 bool CanPredictDamageFeedback() const;
 bool CanPredictShot(uint32 Burst, uint32 Shot) const;
 void ShowPredictedHit(uint32 Burst, uint32 Shot);
 bool IsShowingPredictedHit() const;
 bool IsShowingConfirmedHit() const;
 void ResetDamageFeedback();
 UFUNCTION(Client, Unreliable) void ClientResolvePredictedShot(uint32 Burst, uint32 Shot, bool bHit, bool bHeadshot);
 UFUNCTION(Client, Unreliable) void ClientConfirmHit(bool bHeadshot);
 virtual void SetPawn(APawn* InPawn) override;
 float HitMarkerUntil=0.f;
 bool bHitMarkerHead=false;
 UFUNCTION(Exec) void HostGame();
 UFUNCTION(Exec) void JoinGame(const FString& Address);
 UFUNCTION(Exec) void LeaveGame();

protected:
 UPROPERTY(VisibleAnywhere, Category="Communication") TObjectPtr<UVanthexCommunicationComponent> Communication;
 bool bScoreboardHeld=false;
 TSharedPtr<class IInputProcessor> ScoreboardKeyRelease;
 UPROPERTY(VisibleAnywhere, Category="Audio") TObjectPtr<UVanthexBulletAudioComponent> BulletAudio;
 UPROPERTY(VisibleAnywhere, Category="Audio") TObjectPtr<UVanthexWeaponAudioComponent> WeaponAudio;
 UPROPERTY(VisibleAnywhere, Category="Economy") TObjectPtr<UVanthexBuyMenuComponent> BuyMenu;
 UPROPERTY(VisibleAnywhere, Category="Moderation") TObjectPtr<UVanthexModerationComponent> Moderation;
	virtual void BeginPlay() override;
 virtual void SetupInputComponent() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void PawnLeavingGame() override;
 virtual void OnRep_Pawn() override;
 void ToggleSessionMenu();
 virtual void Tick(float DeltaSeconds) override;
 virtual void PlayerTick(float DeltaSeconds) override;
 float ProbeStart = -1.f;
 bool bProbeReported = false;
 bool bProbeFired = false;
 bool bProbeStopped = false;
 bool bProbeDamaged = false;
 FVector ProbeOrigin;
 void OpenSessionMenu();
 void CloseSessionMenu();
 void RemoveSessionMenu();
 void OpenSessionMap(const TCHAR* MapPath, bool bListen);
 TSharedPtr<class SWidget> SessionMenu;

private:
 UPROPERTY(Replicated) TObjectPtr<AVanthexCharacter> SpectatedTeammate;
 bool IsOwnCharacterDead() const;
 bool IsLivingTeammate(const AVanthexCharacter* Candidate) const;
 AVanthexCharacter* FindSpectateTarget(const AVanthexCharacter* Current, int32 Direction) const;
 void SetSpectateTarget(AVanthexCharacter* Target);
 void SpectatePrevious();
 void SpectateNext();
 void RequestSpectateCycle(int32 Direction);
 UFUNCTION(Server, Reliable) void ServerCycleSpectate(int32 Direction);
 void RequestTeamPing();
 UFUNCTION(Server, Unreliable) void ServerPlaceTeamPing();
 UFUNCTION(Client, Unreliable) void ClientPlaceTeamPing(FVector_NetQuantize Location, int32 RoundIndex);
 FVector TeamPingLocation = FVector::ZeroVector;
 double NextTeamPingAt = 0.0;
 double TeamPingUntil = 0.0;
 int32 TeamPingRound = INDEX_NONE;
 bool CanShowDamageFeedback() const;
 uint32 LastIssuedHitBurst = 0;
 uint32 InvalidatedThroughHitBurst = 0;
 uint32 ActiveHitBurst = 0;
 uint32 LastResolvedHitShot = 0;
 uint32 PendingHitShot = 0;
 float PredictedHitUntil = 0.f;
 int32 HitFeedbackRound = INDEX_NONE;
};
