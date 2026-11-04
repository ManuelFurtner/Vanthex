#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexShadows.h"
#include "VanthexGridV4Layout.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexSavedMapShadows,"Vanthex.Rendering.SavedMapShadowUpgrade",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVanthexSavedMapShadows::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
  .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("test world"),World)) return false;
 const FVector Center=GridV4::Rooms()[0].Center;
 auto Spawn=[&](FVector Position,bool Tagged) {
  auto* Actor=World->SpawnActor<APointLight>(Position,FRotator::ZeroRotator);
  if(Actor && Tagged) Actor->Tags.Add(TEXT("GridV4Light"));
  return Actor;
 };
 auto* Key=Spawn(Center+FVector(0,0,330),true);
 auto* Fill=Spawn(Center+FVector(220,220,285),true);
 auto* Unrelated=Spawn(Center+FVector(0,0,330),false);
 if(!Key || !Fill || !Unrelated) { AddError(TEXT("Cannot spawn light fixtures")); World->DestroyWorld(false); return false; }
 for(auto* Actor:{Key,Fill,Unrelated}) Actor->GetLightComponent()->SetCastShadows(false);
 auto* KeyLight=CastChecked<UPointLightComponent>(Key->GetLightComponent());
 const float OriginalIntensity=KeyLight->Intensity, OriginalRadius=KeyLight->AttenuationRadius;
 TestEqual(TEXT("Upgrades only authored room key in an old map"),VanthexShadows::ApplyGridV4(World),1);
 TestTrue(TEXT("Main light now casts player and object shadows"),bool(KeyLight->CastShadows));
 TestFalse(TEXT("Fill lights retain bounded shadow cost"),bool(Fill->GetLightComponent()->CastShadows));
 TestFalse(TEXT("Unrelated light is untouched"),bool(Unrelated->GetLightComponent()->CastShadows));
 TestEqual(TEXT("Repeat application is idempotent"),VanthexShadows::ApplyGridV4(World),1);
 TestEqual(TEXT("Brightness unchanged"),KeyLight->Intensity,OriginalIntensity);
 TestEqual(TEXT("Influence radius unchanged"),KeyLight->AttenuationRadius,OriginalRadius);
 World->DestroyWorld(false); return true;
}
#endif
