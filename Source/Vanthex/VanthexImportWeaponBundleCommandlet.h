#pragma once
#include "Commandlets/Commandlet.h"
#include "VanthexImportWeaponBundleCommandlet.generated.h"

/** Derives presentation meshes from a locally installed licensed FPS Weapon Bundle. */
UCLASS()
class UVanthexImportWeaponBundleCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UVanthexImportWeaponBundleCommandlet();
 virtual int32 Main(const FString& Params) override;
};
