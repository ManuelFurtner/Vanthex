#include "CoreMinimal.h"
#if WITH_EDITOR
#include "VanthexTestArena.h"
#include "VanthexGameMode.h"
#include "VanthexCore.h"
#include "VanthexFlag.h"
#include "Materials/MaterialInstanceConstant.h"
#include "VanthexShiftDevice.h"
#include "VanthexExtractionZone.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

bool VanthexBakeFoundry(bool bGridV3 = false, bool bGridV4 = false, bool bCTF = false)
{
 const FString LevelName=bCTF?TEXT("CrossroadsCTF"):bGridV4?TEXT("GridV4"):bGridV3?TEXT("GridV3"):TEXT("Foundry");
 const FString Folder=bCTF?TEXT("CROSSROADS CTF"):bGridV4?TEXT("GRID V4 AEGIS"):bGridV3?TEXT("GRID V3"):TEXT("FOUNDRY");
 auto* Package=CreatePackage(*(TEXT("/Game/Vanthex/Maps/")+LevelName));
 UWorld::InitializationValues IV; IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* World=UWorld::CreateWorld(EWorldType::Editor,false,FName(*LevelName),Package,true,ERHIFeatureLevel::SM6,&IV);
 if(!World) return false;
 World->SetFlags(RF_Public|RF_Standalone);
 World->GetWorldSettings()->DefaultGameMode=AVanthexGameMode::StaticClass();
 auto* Arena=World->SpawnActor<AVanthexTestArena>(); Arena->Build();
 TArray<UStaticMeshComponent*> Parts; Arena->GetComponents(Parts);
 int32 Count=0;
 TMap<UMaterialInterface*,UMaterialInstanceConstant*> SavedCTFMaterials;
 auto CopyMaterials=[&](UStaticMeshComponent* From,UStaticMeshComponent* To) {
  for(int32 I=0;I<From->GetNumMaterials();++I) if(auto* M=From->GetMaterial(I))
   if(bCTF) {
    UMaterialInstanceConstant*& Saved=SavedCTFMaterials.FindOrAdd(M);
    if(!Saved) {
     Saved=NewObject<UMaterialInstanceConstant>(Package,NAME_None,RF_Public);
     Saved->SetParentEditorOnly(M->GetMaterial());
     Saved->CopyMaterialUniformParametersEditorOnly(M);
    }
    To->SetMaterial(I,Saved);
   } else if(M->GetOutermost()->GetName().StartsWith(TEXT("/Game/Materials/"))||M->GetOutermost()->GetName().StartsWith(TEXT("/Game/Vanthex/Materials/"))) To->SetMaterial(I,M);
 };
 for(auto* Part:Parts) {
  if(!Part->GetStaticMesh() || (!Part->IsVisible() && Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision)) continue;
  if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Part)) {
   if(!Instances->IsVisible()) continue;
   auto* Actor=World->SpawnActor<AActor>(); Actor->SetActorLabel(Part->GetName()); Actor->SetFolderPath(FName(*(Folder+TEXT("/Factory Kit"))));
   auto* Component=NewObject<UInstancedStaticMeshComponent>(Actor,TEXT("KitInstances")); Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
   Component->SetStaticMesh(Part->GetStaticMesh()); Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); Component->ComponentTags=Part->ComponentTags;
   if(bCTF) {
    Component->SetCollisionProfileName(Part->GetCollisionProfileName());
    Component->SetCollisionEnabled(Part->GetCollisionEnabled());
    Component->SetCollisionObjectType(Part->GetCollisionObjectType());
    Component->SetCollisionResponseToChannels(Part->GetCollisionResponseToChannels());
   }
   // Preserve emissive accents' no-shadow policy when serializing kit instances.
   Component->SetCastShadow(Part->CastShadow);
   CopyMaterials(Part,Component); Component->RegisterComponent();
   for(int32 I=0;I<Instances->GetInstanceCount();++I) { FTransform T; Instances->GetInstanceTransform(I,T,true); Component->AddInstance(T,true); }
  } else if(Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || (bCTF&&Part->IsVisible())) {
   auto* Actor=World->SpawnActor<AStaticMeshActor>(); Actor->SetActorLabel(bCTF?Part->GetName():FString::Printf(TEXT("Structure_%03d"),Count));
   const TCHAR* Section=!Part->IsVisible()?TEXT("/Collision"):bCTF&&Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision?TEXT("/Decoration"):TEXT("/Structure");
   Actor->SetFolderPath(FName(*(Folder+Section)));
   auto* Component=Actor->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable);
   Component->SetStaticMesh(Part->GetStaticMesh()); Component->SetWorldTransform(Part->GetComponentTransform());
   Component->SetCollisionProfileName(TEXT("BlockAll")); Component->SetVisibility(Part->IsVisible()); Component->SetCastShadow(Part->IsVisible());
   if(bCTF) {
    // Architectural trim remains decorative after the procedural owner is removed.
    Component->SetCollisionProfileName(Part->GetCollisionProfileName());
    Component->SetCollisionEnabled(Part->GetCollisionEnabled());
    Component->SetCollisionObjectType(Part->GetCollisionObjectType());
    Component->SetCollisionResponseToChannels(Part->GetCollisionResponseToChannels());
    Component->SetCastShadow(Part->CastShadow);
   }
   Component->ComponentTags=Part->ComponentTags;
   CopyMaterials(Part,Component); Component->SetMobility(EComponentMobility::Static);
  }
  ++Count;
 }
 // Gameplay actors are created by the authoritative mode, avoiding baked duplicates.
 TArray<AActor*> Remove;
 for(TActorIterator<AActor> It(World);It;++It) if(It->IsA<AVanthexTestArena>() || It->IsA<AVanthexCore>() || It->IsA<AVanthexFlag>() || It->IsA<AVanthexShiftDevice>() || It->IsA<AVanthexExtractionZone>() || It->IsA<APlayerStart>()) Remove.Add(*It);
 for(auto* Actor:Remove) World->DestroyActor(Actor);
 Package->MarkAsFullyLoaded(); Package->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_None;
 const FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetMapPackageExtension());
 const bool OK=UPackage::SavePackage(Package,World,*Filename,Args);
 UE_LOG(LogTemp,Display,TEXT("%s authored level: %d component groups, saved=%d at %s"),*LevelName,Count,OK,*Filename);
 World->DestroyWorld(false); return OK;
}
#endif
