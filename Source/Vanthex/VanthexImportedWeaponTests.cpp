#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexImportedWeaponSet.h"
#include "VanthexWeaponPresentation.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexOptionalWeaponBundle,"Vanthex.WeaponModels.OptionalLicensedBundle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexOptionalWeaponBundle::RunTest(const FString&)
{
 int32 Count=0;
 for(const FName Id:{FName(TEXT("M4")),FName(TEXT("AK")),FName(TEXT("Knife"))}) {
  const auto* Model=UVanthexImportedWeaponSet::Find(Id);
  if(!Model) continue;
  ++Count; const FString Label=Id.ToString();
  TestTrue(Label+TEXT(" licensed geometry remains separate from public fallback"),Model->Body->GetPathName().StartsWith(TEXT("/Game/Vanthex/Imported/Weapons/")));
  TestTrue(Label+TEXT(" detailed source mesh"),Model->Body->GetNumTriangles(0)>1000);
  TestTrue(Label+TEXT(" distance LOD"),Model->Body->GetNumLODs()>1&&Model->Body->GetNumTriangles(1)<Model->Body->GetNumTriangles(0));
  TestTrue(Label+TEXT(" normalized centimetre bounds"),Model->Body->GetBoundingBox().GetSize().X>20&&Model->Body->GetBoundingBox().GetSize().X<60);
  TestTrue(Label+TEXT(" actual muzzle matches front end"),FMath::Abs(Model->Muzzle.X-Model->Body->GetBoundingBox().Max.X)<.5);
  for(const auto& Slot:Model->Body->GetStaticMaterials())
   TestTrue(Label+TEXT(" preserves source PBR materials"),Slot.MaterialInterface&&Slot.MaterialInterface->GetPathName().StartsWith(TEXT("/Game/FPS_Weapon_Bundle/")));
  TestTrue(Label+TEXT(" runtime muzzle follows selected model"),VanthexWeaponPresentation::Muzzle(Id).Equals(Model->Muzzle,.001));
  if(Id!=TEXT("Knife")) {
   TestNotNull(Label+TEXT(" detachable source magazine"),Model->Magazine.Get());
   if(Model->Magazine) TestTrue(Label+TEXT(" magazine is distinct geometry"),Model->Magazine->GetNumTriangles(0)<Model->Body->GetNumTriangles(0));
   const auto Aim=VanthexWeaponPresentation::Aim(Id);
   const FVector ViewSight=Aim.Rotation.RotateVector(Model->SightPoint)+Aim.CameraRelativeOrigin;
   TestTrue(Label+TEXT(" real source sight centered on camera axis"),FMath::Abs(ViewSight.Y)<.01&&FMath::Abs(ViewSight.Z)<.01);
   TestTrue(Label+TEXT(" native iron sights selected"),Aim.Sight==EVanthexSight::Iron);
  }
 }
 TestTrue(TEXT("Optional set is absent or contains all three matching models"),Count==0||Count==3);
 TestNull(TEXT("Bundle does not mislabel unrelated platform as MP7"),UVanthexImportedWeaponSet::Find(TEXT("MP7")));
 TestNull(TEXT("Bundle does not replace AUG with unrelated platform"),UVanthexImportedWeaponSet::Find(TEXT("AUG")));
 if(Count==0) AddInfo(TEXT("Licensed pack absent or VanthexOwnWeaponModels selected; complete authored armory remains the fallback."));
 return true;
}
#endif
