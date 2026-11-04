#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexGadgetPresentationProfile.h"
#include "VanthexGadgetVisualComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"

namespace { constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexGadgetPresentationAssets,"Vanthex.Gadgets.PresentationAssets",Flags)
bool FVanthexGadgetPresentationAssets::RunTest(const FString& Parameters)
{
 auto* P=LoadObject<UVanthexGadgetPresentationProfile>(nullptr,TEXT("/Game/Vanthex/Gadgets/DA_GadgetPresentation.DA_GadgetPresentation"));
 if(!TestNotNull(TEXT("Imported gadget profile"),P)) return false;
 TestEqual(TEXT("15 original gadget audio events"),P->Sounds.Num(),15);
 int32 Loops=0;
 for(const auto& Entry:P->Sounds) {
  const auto* W=Cast<USoundWave>(Entry.Value);
  TestTrue(TEXT("Every gadget event is short mono audio"),W && W->NumChannels==1 && W->Duration>0.f && W->Duration<=1.25f);
  if(W && W->bLooping) ++Loops;
 }
 TestEqual(TEXT("Only flight, smoke and EMP hum loop"),Loops,3);
 TestTrue(TEXT("All primitive geometry is referenced for cooking"),P->Sphere && P->Cylinder && P->Cube);
 TestNotNull(TEXT("Shared concurrency limit"),P->Concurrency.Get());
 TestTrue(TEXT("Near and far sources have spatial range limits"),P->NearAttenuation && P->FarAttenuation
  && P->NearAttenuation->Attenuation.bSpatialize && P->FarAttenuation->Attenuation.bAttenuate
  && P->NearAttenuation->Attenuation.FalloffDistance<P->FarAttenuation->Attenuation.FalloffDistance);
 auto* M=P->SmokeMaterial?P->SmokeMaterial->GetMaterial():nullptr;
 TestTrue(TEXT("Smoke is opaque and two-sided for an inside camera"),M && M->BlendMode==BLEND_Opaque && M->TwoSided);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexGadgetSmokeClipping,"Vanthex.Gadgets.SmokeWallClipping",Flags)
bool FVanthexGadgetSmokeClipping::RunTest(const FString& Parameters)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false)
  .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("Smoke collision fixture"),W)) return false;
 AActor* Gadget=W->SpawnActor<AActor>();
 auto* Root=NewObject<USceneComponent>(Gadget); Gadget->SetRootComponent(Root); Root->RegisterComponent();
 // A rolled grenade must still create smoke 140 cm upward in world space.
 Gadget->SetActorRotation(FRotator(38,21,75));
 auto* Visual=NewObject<UVanthexGadgetVisualComponent>(Gadget); Visual->RegisterComponent(); Visual->Configure(TEXT("Smoke"));
 if(!TestNotNull(TEXT("Smoke profile loaded"),Visual->GetProfile())) { W->DestroyWorld(false); return false; }
 TestTrue(TEXT("Throwable is visible geometry"),Visual->GetProjectilePartCount()>=8);
 Visual->ActivateEffect(18.f,450.f,.5f);
 const int32 OpenCount=Visual->GetSmokeInstanceCount();
 TestTrue(TEXT("Dense smoke contains overlapping volumes"),OpenCount>20);
 Visual->EndEffect(); TestFalse(TEXT("EndEffect deactivates smoke"),Visual->IsEffectVisible());
 auto* Wall=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(10,1000,1000)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Box->RegisterComponent(); Wall->SetActorLocation(FVector(120,0,140));
 Visual->ActivateEffect(18.f,450.f,.6f);
 TestTrue(TEXT("Wall removes unreachable smoke lobes"),Visual->GetSmokeInstanceCount()>0 && Visual->GetSmokeInstanceCount()<OpenCount);
 TArray<UInstancedStaticMeshComponent*> Clouds; Gadget->GetComponents(Clouds);
 bool CenterCorrect=false,InsideWall=true; int32 Checked=0;
 for(auto* Cloud:Clouds) if(Cloud->GetMaterial(0)==Visual->GetProfile()->SmokeMaterial) {
  CenterCorrect=Cloud->GetComponentLocation().Equals(FVector(0,0,140),.01);
  for(int32 I=0; I<Cloud->GetInstanceCount(); ++I) {
   FTransform T; Cloud->GetInstanceTransform(I,T,true); ++Checked;
   InsideWall &= T.GetLocation().X+50.*T.GetScale3D().X<=110.01;
  }
 }
 TestTrue(TEXT("Smoke offset is world-up despite rotated projectile"),CenterCorrect);
 TestTrue(TEXT("No smoke sphere crosses the blocking wall"),Checked>0 && InsideWall);
 Visual->EndEffect(); W->DestroyWorld(false);
 return true;
}
#endif
