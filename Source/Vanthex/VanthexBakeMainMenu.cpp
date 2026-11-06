#include "VanthexFrontEndMode.h"

#if WITH_EDITOR
#include "Vanthex.h"
#include "VanthexMapLayout.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

bool VanthexBakeMainMenu()
{
 UPackage* Package = CreatePackage(VanthexMap::FrontEndPath);
 UWorld::InitializationValues Init;
 Init.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(TEXT("MainMenu")), Package, true, ERHIFeatureLevel::SM6, &Init);
 if(!World) return false;

 World->SetFlags(RF_Public | RF_Standalone);
 World->GetWorldSettings()->DefaultGameMode = AVanthexFrontEndMode::StaticClass();
 Package->MarkAsFullyLoaded();
 Package->MarkPackageDirty();

 FSavePackageArgs Args;
 Args.TopLevelFlags = RF_Public | RF_Standalone;
 const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetMapPackageExtension());
 const bool bSaved = UPackage::SavePackage(Package, World, *Filename, Args);
 UE_LOG(LogVanthex, Display, TEXT("Main menu level saved=%d at %s"), bSaved, *Filename);

 World->DestroyWorld(false);
 return bSaved;
}
#endif
