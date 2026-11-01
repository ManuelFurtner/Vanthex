#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VanthexHUD.generated.h"

UCLASS()
class AVanthexHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
 float PreviousHealth=100.f;
 float DamageFlashUntil=0.f;
};
