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
 bool IsScoreboardOpen() const { return bScoreboardHeld; }
 void ShowScoreboard() { bScoreboardHeld=true; }
 void HideScoreboard() { bScoreboardHeld=false; }
 UFUNCTION(Client, Unreliable) void ClientConfirmHit(bool bHeadshot);
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
 void ToggleSessionMenu();
 virtual void PlayerTick(float DeltaSeconds) override;
 float ProbeStart = -1.f;
 bool bProbeReported = false;
 bool bProbeFired = false;
 bool bProbeStopped = false;
 bool bProbeDamaged = false;
 FVector ProbeOrigin;
 void CloseSessionMenu();
 TSharedPtr<class SWidget> SessionMenu;
};
