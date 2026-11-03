#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexBuildWeaponAudioCommandlet.generated.h"

UCLASS()
class UVanthexBuildWeaponAudioCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexBuildWeaponAudioCommandlet();
 virtual int32 Main(const FString& Params) override;
};
