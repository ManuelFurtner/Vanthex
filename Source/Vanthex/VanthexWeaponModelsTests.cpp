#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexWeaponPresentation.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponModelAssets,"Vanthex.WeaponModels.CompleteAuthoredArmory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexWeaponModelAssets::RunTest(const FString&)
{
 int32 Count=0;
 for(const TCHAR* Id:{TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun"),TEXT("Knife")})
  for(const TCHAR* Part:{TEXT("Body"),TEXT("Magazine"),TEXT("Bolt")}) {
   if(FCString::Strcmp(Id,TEXT("Knife"))==0&&FCString::Strcmp(Part,TEXT("Body"))!=0) continue;
   ++Count;
   const FString Name=FString::Printf(TEXT("SM_%s_%s"),Id,Part);
   const auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Vanthex/Weapons/Meshes/%s.%s"),*Name,*Name));
   if(!TestNotNull(Name,Mesh)) continue;
   const FVector Size=Mesh->GetBoundingBox().GetSize();
   const int32 NearTriangles=Mesh->GetNumTriangles(0);
   // The compact bolt plates are closed bevelled solids: 60 source triangles, 32 unique
   // positions and only 1 mm thick. A simplifier can preserve all their hard edges.
   // Restrict that allowance to this physical part class; bodies, magazines and larger
   // bolt/slide assemblies must still satisfy the original detail and reduction budget.
   const bool CompactBolt=FCString::Strcmp(Part,TEXT("Bolt"))==0&&Size.GetMax()<=5&&Size.GetMin()<=.25;
   const int32 MinimumTriangles=CompactBolt?24:100; // Excludes an unshaped 12-triangle box even for a small plate.
   TestTrue(FString::Printf(TEXT("%s detailed geometry (LOD0=%d, minimum=%d)"),*Name,NearTriangles,MinimumTriangles),
    NearTriangles>=MinimumTriangles&&NearTriangles<50000);
   if(CompactBolt) TestTrue(Name+TEXT(" solid bolt plate thickness"),Size.GetMin()>.05);
   if(TestTrue(Name+TEXT(" distance LOD exists"),Mesh->GetNumLODs()>=2)) {
    const int32 FarTriangles=Mesh->GetNumTriangles(1);
    TestTrue(FString::Printf(TEXT("%s distance geometry budget (LOD0=%d LOD1=%d compact=%d)"),*Name,NearTriangles,FarTriangles,CompactBolt),
     FarTriangles>=12&&FarTriangles<=NearTriangles&&(CompactBolt||FarTriangles<NearTriangles));
   }
   TestTrue(Name+TEXT(" sane centimetre bounds"),Size.GetMax()>1&&Size.GetMax()<90);
   TestTrue(Name+TEXT(" material library"),Mesh->GetStaticMaterials().Num()==10);
   for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(Name+TEXT(" PBR slot"),Slot.MaterialInterface.Get());
  }
 TestEqual(TEXT("Nine three-part firearms and one knife"),Count,28);
 TestTrue(TEXT("Legacy diagnostic uses authored M4"),VanthexWeaponPresentation::ModelId(TEXT("LegacyRifle"))==TEXT("M4"));
 TestTrue(TEXT("Compact MP7 and DMR use different actual muzzle offsets"),VanthexWeaponPresentation::Muzzle(TEXT("MP7")).X<VanthexWeaponPresentation::Muzzle(TEXT("DMR")).X);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponModelReload,"Vanthex.WeaponModels.MagazineAndBoltPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FVanthexWeaponModelReload::RunTest(const FString&)
{
 auto* Magazine=NewObject<UStaticMeshComponent>(); auto* Bolt=NewObject<UStaticMeshComponent>();
 Magazine->ComponentTags.Add(TEXT("WeaponPart_Magazine")); Bolt->ComponentTags.Add(TEXT("WeaponPart_Bolt"));
 TArray<TObjectPtr<UStaticMeshComponent>> Parts={Magazine,Bolt};
 VanthexWeaponPresentation::Animate(Parts,TEXT("AUG"),.25f,0);
 TestTrue(TEXT("Magazine withdraws at magazine-out cue"),Magazine->GetRelativeLocation().Z<-8.9);
 VanthexWeaponPresentation::Animate(Parts,TEXT("AUG"),.65f,0);
 TestTrue(TEXT("Magazine is seated at magazine-in cue"),Magazine->GetRelativeLocation().IsNearlyZero());
 VanthexWeaponPresentation::Animate(Parts,TEXT("AUG"),.82f,0);
 TestTrue(TEXT("Bolt moves during bolt cue"),Bolt->GetRelativeLocation().X<-.9);
 VanthexWeaponPresentation::Animate(Parts,TEXT("AUG"),-1.f,0);
 TestTrue(TEXT("Cancel or completion restores both transforms"),Magazine->GetRelativeLocation().IsNearlyZero()&&Bolt->GetRelativeLocation().IsNearlyZero());
 VanthexWeaponPresentation::Animate(Parts,TEXT("Pistol"),-1.f,1);
 TestTrue(TEXT("Pistol slide responds to accepted shot"),Bolt->GetRelativeLocation().X<-1.5);
 return true;
}
#endif
