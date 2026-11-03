#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VanthexWeaponAudioAcoustics.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexWeaponAudioProfile.h"
#include "VanthexWeaponAudioMovement.h"
#include "VanthexMapLayout.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"

namespace { constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponAudioPriorities,"Vanthex.WeaponAudio.MixPriorities",Flags)
bool FVanthexWeaponAudioPriorities::RunTest(const FString& Parameters)
{
 using E=EVanthexWeaponAudioEvent;
 auto Rank=[](E Event,bool Own=false,bool Tail=false){return UVanthexWeaponAudioComponent::EventPriority(Event,Own,Tail);};
 TestTrue(TEXT("Opponent shot takes priority over own gunshot"),Rank(E::Shot)>Rank(E::Shot,true));
 TestTrue(TEXT("Opponent knife threat takes priority over own gunshot"),Rank(E::KnifeSwing)>Rank(E::Shot,true));
 TestTrue(TEXT("Own gunshot takes priority over reload and mechanics"),Rank(E::Shot,true)>Rank(E::ReloadMagazineIn) && Rank(E::Shot,true)>Rank(E::Mechanic));
 TestTrue(TEXT("Reload phases take priority over purchase UI"),Rank(E::ReloadStart,true)>Rank(E::PurchaseAccepted,true));
 TestTrue(TEXT("A room tail never inherits its shot priority"),Rank(E::Shot,false,true)<Rank(E::PurchaseAccepted,true));
 TestTrue(TEXT("Foot and gear movement ranks between shots and mechanics"),Rank(E::EquipmentMove)>Rank(E::Mechanic) && Rank(E::EquipmentMove)<Rank(E::Shot,true));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponAudioAssets,"Vanthex.WeaponAudio.CompleteMonoBanks",Flags)
bool FVanthexWeaponAudioAssets::RunTest(const FString& Parameters)
{
 auto* P=LoadObject<UVanthexWeaponAudioProfile>(nullptr,TEXT("/Game/Vanthex/Audio/Weapons/DA_WeaponAudio.DA_WeaponAudio"));
 if(!TestNotNull(TEXT("Imported weapon audio profile"),P)) return false;
 TestNotNull(TEXT("Spatial attenuation"),P->Attenuation.Get());
 TestNotNull(TEXT("Bounded concurrency"),P->Concurrency.Get());
 TestNotNull(TEXT("Short diffuse tail"),P->RoomTail.Get());
 TestEqual(TEXT("All actual catalog weapon banks"),P->Weapons.Num(),10);
 int32 WaveCount=1; // Diffuse room tail.
 for(const TCHAR* Name:{TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun"),TEXT("Knife")}) {
  const auto* Bank=P->Weapons.Find(FName(Name));
  if(!TestNotNull(FString::Printf(TEXT("%s bank exists"),Name),Bank)) continue;
  const bool Knife=FCString::Strcmp(Name,TEXT("Knife"))==0;
  TestEqual(FString::Printf(TEXT("%s complete event phases"),Name),Bank->Events.Num(),Knife?3:10);
  if(!Knife) {
   const auto* Shots=Bank->Events.Find(EVanthexWeaponAudioEvent::Shot);
   TestTrue(TEXT("Three shot variants"),Shots && Shots->Variants.Num()==3);
  }
  for(const auto& Event:Bank->Events) for(const auto& Sound:Event.Value.Variants) {
   ++WaveCount;
   const auto* Wave=Cast<USoundWave>(Sound);
   TestTrue(TEXT("Every world variant is short mono PCM"),Wave && Wave->NumChannels==1 && Wave->Duration>0.f && Wave->Duration<.6f);
  }
 }
 TestEqual(TEXT("Four local-only UI feedback events"),P->UI.Num(),4);
 for(const auto& Event:P->UI) WaveCount+=Event.Value.Variants.Num();
 TestEqual(TEXT("Three shared foot+gear variants independent of armor"),P->Movement.Variants.Num(),3);
 for(const auto& Sound:P->Movement.Variants) {
  ++WaveCount;
  const auto* Wave=Cast<USoundWave>(Sound);
  TestTrue(TEXT("Movement action is one short mono source"),Wave && Wave->NumChannels==1 && Wave->Duration>0.f && Wave->Duration<.3f);
 }
 TestEqual(TEXT("All 119 generated sources are referenced"),WaveCount,119);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexEquipmentMovementCadence,"Vanthex.WeaponAudio.EquipmentMovementCadence",Flags)
bool FVanthexEquipmentMovementCadence::RunTest(const FString& Parameters)
{
 const FVector Velocity(600.,0.,0.);
 FVanthexEquipmentStepClock Clock;
 TestFalse(TEXT("Initial sample emits nothing"),Clock.Advance(FVector::ZeroVector,Velocity,.1f,true,true,true,false,false));
 int32 Events=0;
 for(int32 I=1; I<=10; ++I) Events+=Clock.Advance(FVector(I*60.,0.,0.),Velocity,.1f,true,true,true,false,false);
 TestEqual(TEXT("600 cm actual walking gives three combined foot/gear actions"),Events,3);
 bool Idle=false;
 for(int32 I=0; I<20; ++I) Idle|=Clock.Advance(FVector(600.,0.,0.),FVector::ZeroVector,.1f,true,true,true,false,false);
 TestFalse(TEXT("Idle has no movement sounds"),Idle);
 TestFalse(TEXT("Moving platform with no walking velocity is silent"),Clock.Advance(FVector(660.,0.,0.),FVector::ZeroVector,.1f,true,true,true,false,false));
 for(int32 Inhibited=0; Inhibited<3; ++Inhibited) {
  FVanthexEquipmentStepClock Silent;
  bool Any=false;
  for(int32 I=0; I<10; ++I) Any|=Silent.Advance(FVector(I*60.,0.,0.),Velocity,.1f,Inhibited!=0,Inhibited!=1,Inhibited!=2,false,false);
  TestFalse(FString::Printf(TEXT("No action when %s"),Inhibited==0?TEXT("airborne"):(Inhibited==1?TEXT("dead"):TEXT("round inactive"))),Any);
 }
 TestFalse(TEXT("Explicit teleport emits no action"),Clock.Advance(FVector(10000.,0.,0.),Velocity,.1f,true,true,true,true,false));
 TestFalse(TEXT("First short walk after teleport has no residual stride"),Clock.Advance(FVector(10060.,0.,0.),Velocity,.1f,true,true,true,false,false));
 TestFalse(TEXT("Unmarked position discontinuity is rejected"),Clock.Advance(FVector(20000.,0.,0.),Velocity,.1f,true,true,true,false,false));
 TestFalse(TEXT("Long frame stall cannot emit accumulated burst"),Clock.Advance(FVector(20600.,0.,0.),Velocity,1.f,true,true,true,false,false));
 FVanthexEquipmentStepClock Crouch;
 int32 CrouchEvents=0;
 for(int32 I=0; I<=10; ++I) CrouchEvents+=Crouch.Advance(FVector(I*28.,0.,0.),FVector(280.,0.,0.),.1f,true,true,true,false,true);
 TestEqual(TEXT("Crouch follows actual shorter slower steps"),CrouchEvents,2);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanthexWeaponAudioPortals,"Vanthex.WeaponAudio.PhysicalPortalAndWall",Flags)
bool FVanthexWeaponAudioPortals::RunTest(const FString& Parameters)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false)
  .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
 // GetMapName reads the outer package name, not the UWorld object's name. An anonymous
 // CreateWorld package is /Temp/Untitled_* and would exercise only the non-GRID fallback.
 UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Temp/VanthexWeaponAudio_%s/GridV3"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("WeaponAudioGridV3"),Package,true,ERHIFeatureLevel::Num,&Init);
 if(!TestNotNull(TEXT("GRID topology collision world"),W)) return false;
 TestTrue(TEXT("Fixture package activates the production GRID route"),VanthexMap::IsGridV3(W));
 const FVector Source(-3600.,-2400.,145.), Listener(-1900.,-2400.,145.);
 const auto Open=VanthexWeaponAcoustics::Route(W,Source,Listener);
 TestTrue(TEXT("Open room-to-room path is direct"),Open.bDirect && Open.Transmission==1.f);
 TestEqual(TEXT("Uses actual GRID source room"),Open.SourceRoom,0);
 TestEqual(TEXT("Uses actual GRID listener room"),Open.ListenerRoom,1);
 auto* Door=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Door);
 Door->SetRootComponent(Box); Box->SetBoxExtent(FVector(12.,300.,210.));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Box->RegisterComponent(); Door->SetActorLocation(FVector(-2750.,-2400.,145.));
 VanthexWeaponAcoustics::Invalidate(W);
 const auto Around=VanthexWeaponAcoustics::Route(W,Source,Listener);
 TestTrue(TEXT("Closed physical doorway removes direct ray"),!Around.bDirect);
 TestTrue(TEXT("A longer physically clear route uses GRID portals"),Around.bPortal && Around.Hops>0 && Around.Distance>Open.Distance);
 TestTrue(TEXT("Portal route is quieter and low-passed"),Around.Transmission<Open.Transmission && Around.LowPassHz<Open.LowPassHz);
 Box->SetBoxExtent(FVector(12.,20000.,20000.)); VanthexWeaponAcoustics::Invalidate(W);
 const auto Wall=VanthexWeaponAcoustics::Route(W,Source,Listener);
 TestTrue(TEXT("Solid separation blocks all portal routes"),!Wall.bDirect && !Wall.bPortal);
 TestTrue(TEXT("Wall transmission is below open portal"),Wall.Transmission<Around.Transmission && Wall.LowPassHz<Around.LowPassHz);
 Door->SetActorLocation(FVector(-2750.,-2400.,50000.)); VanthexWeaponAcoustics::Invalidate(W);
 const auto Reopened=VanthexWeaponAcoustics::Route(W,Source,Listener);
 TestTrue(TEXT("Moving door open immediately restores direct audio"),Reopened.bDirect && Reopened.Transmission==1.f);
 // A horizontal physical ceiling must attenuate sound from a higher source as well.
 Box->SetBoxExtent(FVector(20000.,20000.,12.)); Door->SetActorLocation(FVector(-3600.,-2400.,400.)); VanthexWeaponAcoustics::Invalidate(W);
 const auto Ceiling=VanthexWeaponAcoustics::Route(W,Source+FVector(0.,0.,600.),Source);
 TestTrue(TEXT("Real source height and blocking ceiling affect transmission"),!Ceiling.bDirect && Ceiling.Transmission<1.f);
 W->DestroyWorld(false);
 return true;
}
#endif
