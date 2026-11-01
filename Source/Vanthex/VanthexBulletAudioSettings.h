#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VanthexBulletAudioSettings.generated.h"

class UVanthexBulletAudioProfile;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vanthex Bullet Audio"))
class UVanthexBulletAudioSettings : public UDeveloperSettings
{
 GENERATED_BODY()
public:
 UPROPERTY(Config, EditAnywhere, Category="Audio") TSoftObjectPtr<UVanthexBulletAudioProfile> Profile;
 virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
