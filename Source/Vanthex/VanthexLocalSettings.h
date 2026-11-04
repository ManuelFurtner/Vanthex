#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VanthexCrosshair.h"
#include "VanthexLocalSettings.generated.h"

class UWorld;

/** Local preferences only. Never replicated or used for combat decisions. */
UCLASS(Config=GameUserSettings)
class UVanthexLocalSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config) float MasterVolume = .8f;
	UPROPERTY(Config) float CombatVolume = .85f;
	UPROPERTY(Config) float UIVolume = .55f;
	UPROPERTY(Config) float RoomVolume = .65f;
 UPROPERTY(Config) float VoiceVolume = .8f;
 UPROPERTY(Config) bool bVoiceEnabled = true;
 UPROPERTY(Config) float CrosshairThickness = 1.5f;
 UPROPERTY(Config) float CrosshairLength = 8.f;
 UPROPERTY(Config) float CrosshairGap = 0.f;
 UPROPERTY(Config) FLinearColor CrosshairColor = FLinearColor::White;
 FVanthexCrosshairStyle GetCrosshairStyle() const;
 void ResetCrosshair();
	UPROPERTY(Config) FString LastAddress = TEXT("127.0.0.1:7777");
	static UVanthexLocalSettings* Get();
	void ApplyAudio(UWorld* World);
	void Persist();
	static float SafeVolume(float Value);
	static bool ValidateAddress(const FString& Input, FString& Address, FString& Error);
};
