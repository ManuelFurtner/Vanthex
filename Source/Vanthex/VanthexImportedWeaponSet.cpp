#include "VanthexImportedWeaponSet.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

const FVanthexImportedWeaponModel* UVanthexImportedWeaponSet::Find(FName WeaponId)
{
 // The public project is self-contained. The optional pack is never a hard startup dependency.
 static bool Attempted=false;
 static TStrongObjectPtr<UVanthexImportedWeaponSet> Set;
 if(!Attempted) {
  Attempted=true;
  if(!FParse::Param(FCommandLine::Get(),TEXT("VanthexOwnWeaponModels"))&&
     FPackageName::DoesPackageExist(TEXT("/Game/Vanthex/Imported/Weapons/DA_WeaponBundle"))) {
   Set.Reset(LoadObject<UVanthexImportedWeaponSet>(nullptr,
    TEXT("/Game/Vanthex/Imported/Weapons/DA_WeaponBundle.DA_WeaponBundle")));
  }
 }
 const FName Id=WeaponId==TEXT("LegacyRifle")?FName(TEXT("M4")):WeaponId;
 if(Set.IsValid()) for(const auto& Model:Set->Models)
  if(Model.WeaponId==Id&&Model.Body&&(Id==TEXT("Knife")||Model.Magazine)&&
     !Model.Muzzle.ContainsNaN()&&!Model.SightPoint.ContainsNaN()) return &Model;
 return nullptr;
}
