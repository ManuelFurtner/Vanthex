#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VanthexHUD.generated.h"

UCLASS()
class AVanthexHUD : public AHUD
{
	GENERATED_BODY()

public:
	AVanthexHUD();
 virtual void DrawHUD() override;
 UPROPERTY() TObjectPtr<UFont> DisplayFont;
 float PreviousHealth=100.f;
 float DamageFlashUntil=0.f;
 void DrawTacticalHUD();
};
