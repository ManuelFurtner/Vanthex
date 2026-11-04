#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VanthexGameInstance.generated.h"

UCLASS()
class UVanthexGameInstance : public UGameInstance
{
 GENERATED_BODY()
public:
 virtual void Init() override;
 virtual void Shutdown() override;
private:
 void PrepareLoadingScreen(const FString& MapName);
 FDelegateHandle PreLoadHandle;
};
