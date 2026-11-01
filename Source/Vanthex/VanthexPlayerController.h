#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VanthexBulletAudioComponent.h"
#include "VanthexPlayerController.generated.h"

UCLASS()
class AVanthexPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVanthexPlayerController();
 UVanthexBulletAudioComponent* GetBulletAudio() const { return BulletAudio; }
 UFUNCTION(Client, Unreliable) void ClientBulletAudio(const FVanthexBulletAudioPacket& Packet);

	/** Local test restart. Later this is a server round restart. */
	UFUNCTION(Exec)
	void RestartTest();
 bool IsSessionMenuOpen() const { return SessionMenu.IsValid(); }
 UFUNCTION(Client, Unreliable) void ClientConfirmHit(bool bHeadshot);
 float HitMarkerUntil=0.f;
 bool bHitMarkerHead=false;
 UFUNCTION(Exec) void HostGame();
 UFUNCTION(Exec) void JoinGame(const FString& Address);
 UFUNCTION(Exec) void LeaveGame();

protected:
 UPROPERTY(VisibleAnywhere, Category="Audio") TObjectPtr<UVanthexBulletAudioComponent> BulletAudio;
	virtual void BeginPlay() override;
 virtual void SetupInputComponent() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
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
 TSharedPtr<class SEditableTextBox> AddressBox;
};
