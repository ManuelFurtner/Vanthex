#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexInspectWeaponBundleCommandlet.generated.h"

/** Read-only inventory. Reports and optional geometry snapshots go to Saved only. */
UCLASS()
class UVanthexInspectWeaponBundleCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexInspectWeaponBundleCommandlet();
 virtual int32 Main(const FString& Params) override;
};
