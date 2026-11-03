#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexGameState.h"
#include "VanthexOperatorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
struct FEquipmentNetworkProbe
{
 FTimerHandle Timer;
 TWeakObjectPtr<AVanthexCharacter> Participant;
 double Started=0., Ready=-1., HostFinish=-1.;
 bool SentKnife=false, SentAttack=false, SentRifle=false;
 bool SawKnife=false, SawSwing=false, SawRifleAfter=false, SawSwitch=false;
 bool NoDoubleWeapon=true, Finished=false;
 int32 Samples=0;

 void Finish(UWorld* World,bool Host,bool TimedOut)
 {
  if(Finished) return;
  Finished=true;
  const bool Pass=!TimedOut && SawKnife && SawSwing && SawRifleAfter && SawSwitch && NoDoubleWeapon && Samples>20
   && (Host || (SentKnife && SentAttack && SentRifle));
  const FString Report=FString::Printf(TEXT("%s role=%s client_driven=1 knife=%d swing=%d rifle_restored=%d switch_seen=%d no_double_weapon=%d samples=%d timeout=%d\n"),
   Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),SawKnife,SawSwing,SawRifleAfter,SawSwitch,NoDoubleWeapon,Samples,TimedOut);
  const FString File=FPaths::ProjectSavedDir()/(Host?TEXT("EquipmentNetworkHost.txt"):TEXT("EquipmentNetworkClient.txt"));
  const bool Saved=FFileHelper::SaveStringToFile(Report,*File);
  UE_LOG(LogVanthex,Display,TEXT("EQUIPMENT_NETWORK %s saved=%d"),*Report,Saved);
  World->GetTimerManager().ClearTimer(Timer);
  FPlatformMisc::RequestExitWithStatus(false,Pass&&Saved?0:1);
 }
};
}
#endif

// Explicit two-process diagnostic. Only the client's real component RPCs alter equipment.
void VanthexStartEquipmentNetworkTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
 if(!Controller || !Controller->IsLocalController() || !Controller->GetWorld()) return;
 UWorld* World=Controller->GetWorld();
 const TWeakObjectPtr<AVanthexPlayerController> WeakController(Controller);
 const TWeakObjectPtr<UWorld> WeakWorld(World);
 const auto State=MakeShared<FEquipmentNetworkProbe>();
 State->Started=World->GetTimeSeconds();
 World->GetTimerManager().SetTimer(State->Timer,FTimerDelegate::CreateLambda([WeakController,WeakWorld,State]()
 {
  if(!WeakController.IsValid() || !WeakWorld.IsValid() || State->Finished) return;
  auto* PC=WeakController.Get();
  auto* W=WeakWorld.Get();
  const bool Host=PC->HasAuthority();
  const double Now=W->GetTimeSeconds();
  if(Now-State->Started>45.) { State->Finish(W,Host,true); return; }
  if(Host && State->HostFinish>=0. && Now>=State->HostFinish) { State->Finish(W,true,false); return; }
  if(!State->Participant.IsValid())
  {
   if(!Host) State->Participant=Cast<AVanthexCharacter>(PC->GetPawn());
   else for(TActorIterator<AVanthexPlayerController> It(W);It;++It)
   {
    if(It->IsLocalController()) continue;
    if(auto* Remote=Cast<AVanthexCharacter>(It->GetPawn())) { State->Participant=Remote; break; }
   }
  }
  auto* Pawn=State->Participant.Get();
  auto* Equipment=Pawn?Pawn->GetEquipment():nullptr;
  auto* Operator=Pawn?Pawn->FindComponentByClass<UVanthexOperatorComponent>():nullptr;
  auto* GS=W->GetGameState<AVanthexGameState>();
  if(!Equipment || !Operator || !Operator->IsPresentationReady() || !GS || GS->GetPhase()!=EVanthexRoundPhase::RoundActive) return;
  auto* Rifle=Operator->GetWorldRifle();
  auto* Knife=Equipment->GetWorldKnifeRoot();
  if(!Rifle || !Knife) return;
  if(State->Ready<0.)
  {
   State->Ready=Now;
   UE_LOG(LogVanthex,Display,TEXT("EQUIPMENT_NETWORK ready role=%s pawn=%s local_owner=%d"),Host?TEXT("host"):TEXT("client"),*Pawn->GetName(),Pawn->IsLocallyControlled());
  }
  const double Age=Now-State->Ready;
  ++State->Samples;
  State->NoDoubleWeapon &= !(Rifle->IsVisible() && Knife->IsVisible());
  State->SawSwitch |= Equipment->IsSwitching();
  if(Equipment->GetSelectedSlot()==EVanthexWeaponSlot::Knife && !Equipment->IsSwitching() && Knife->IsVisible() && !Rifle->IsVisible()) State->SawKnife=true;
  if(State->SawKnife && Equipment->IsSwinging() && Knife->IsVisible() && !Rifle->IsVisible()) State->SawSwing=true;
  if(State->SawKnife && State->SawSwing && Equipment->CanUseRifle() && Rifle->IsVisible() && !Knife->IsVisible()) State->SawRifleAfter=true;
  if(!Host)
  {
   if(Age>=2. && !State->SentKnife) { State->SentKnife=true; Equipment->SelectKnife(); }
   if(Age>=3. && State->SawKnife && !State->SentAttack) { State->SentAttack=true; Equipment->Attack(); }
   if(Age>=5. && State->SentAttack && !Equipment->IsSwinging() && !Equipment->IsSwitching() && !State->SentRifle) { State->SentRifle=true; Equipment->SelectRifle(); }
   if(Age>=7.5) State->Finish(W,false,false);
  }
  else if(State->SawRifleAfter && State->HostFinish<0.)
  {
   // Leave time for the client to record its local replicated result and exit.
   State->HostFinish=Now+3.;
  }
 }),.05f,true);
#endif
}
