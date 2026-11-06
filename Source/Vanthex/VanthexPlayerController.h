#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexPlayerController.generated.h"

class UVanthexBuyMenuComponent;
class UVanthexCommunicationComponent;

UCLASS()
class AVanthexPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVanthexPlayerController();
 UVanthexCommunicationComponent* GetCommunication() const { return Communication; }
 UVanthexBulletAudioComponent* GetBulletAudio() const { return BulletAudio; }
 UFUNCTION(Client, Unreliable) void ClientBulletAudio(const FVanthexBulletAudioPacket& Packet);
 UVanthexWeaponAudioComponent* GetWeaponAudio() const { return WeaponAudio; }
 UVanthexBuyMenuComponent* GetBuyMenu() const { return BuyMenu; }
 UFUNCTION(Client, Unreliable) void ClientWeaponAudio(const FVanthexWeaponAudioPacket& Packet);

	/** Local test restart. Later this is a server round restart. */
	UFUNCTION(Exec)
	void RestartTest();
 bool IsSessionMenuOpen() const { return SessionMenu.IsValid(); }
 bool IsFrontEnd() const;
 void StartGame(bool bCaptureTheFlag, bool bListen = false);
 bool IsScoreboardOpen() const { return bScoreboardHeld; }
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
	virtual void BeginPlay() override;
 virtual void SetupInputComponent() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void PawnLeavingGame() override;
 virtual void OnRep_Pawn() override;
 void ToggleSessionMenu();
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
 bool CanShowDamageFeedback() const;
 uint32 LastIssuedHitBurst = 0;
 uint32 InvalidatedThroughHitBurst = 0;
 uint32 ActiveHitBurst = 0;
 uint32 LastResolvedHitShot = 0;
 uint32 PendingHitShot = 0;
 float PredictedHitUntil = 0.f;
 int32 HitFeedbackRound = INDEX_NONE;
};
