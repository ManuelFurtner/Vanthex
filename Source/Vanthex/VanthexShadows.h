#pragma once
#include "CoreMinimal.h"

class UWorld;
class UPointLightComponent;

namespace VanthexShadows
{
 void ConfigureKeyLight(UPointLightComponent* Light);
 /** Also upgrades saved GRID V4 maps; does not add lights or change their brightness. */
 int32 ApplyGridV4(UWorld* World);
}
