#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexBuyMenuComponent.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "VanthexWeaponAudioComponent.h"
#include "HitscanWeapon.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
AVanthexCharacter* FindEconomyPawn(UWorld* World, EVanthexTeam Team, int32 Slot)
{
 for(TActorIterator<AVanthexCharacter> It(World);It;++It)
  if(const auto* PS=It->GetPlayerState<AVanthexPlayerState>();PS && PS->GetTeam()==Team && PS->GetSpawnSlot()==Slot) return *It;
 return nullptr;
}

bool PurchasedState(const AVanthexPlayerState* PS)
{
 return PS && PS->GetEconomy().Credits==827 && PS->GetEconomy().Sidearm==TEXT("P250")
  && PS->GetEconomy().PrimaryWeapon.IsNone() && PS->GetEconomy().ArmorTier==EVanthexArmorTier::Light
  && FMath::IsNearlyEqual(PS->GetArmorPoints(),40.f) && PS->GetEconomy().Gadgets.IsEmpty();
}

bool FindClearMovementYaw(AVanthexCharacter* Pawn,float& Yaw)
{
 auto* World=Pawn?Pawn->GetWorld():nullptr;
 auto* Capsule=Pawn?Pawn->GetCapsuleComponent():nullptr;
 if(!World || !Capsule) return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(EconomyNetworkMovementLane),false,Pawn);
 if(Pawn->GetWeapon()) Query.AddIgnoredActor(Pawn->GetWeapon());
 const FVector Start=Pawn->GetActorLocation();
 constexpr float Length=450.f;
 const float HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
 const float OriginalYaw=Pawn->GetControlRotation().Yaw;
 for(float Offset:{0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f,180.f})
 {
  const float CandidateYaw=OriginalYaw+Offset;
  const FVector End=Start+FRotator(0,CandidateYaw,0).Vector()*Length;
  FHitResult Hit;
  if(World->SweepSingleByProfile(Hit,Start+FVector(0,0,3),End+FVector(0,0,3),FQuat::Identity,
   Capsule->GetCollisionProfileName(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),HalfHeight),Query)) continue;
  bool Supported=true;
  for(int32 Step=0;Step<=4;++Step)
  {
   const FVector Sample=FMath::Lerp(Start,End,float(Step)/4.f);
   FHitResult Floor;
   if(!World->LineTraceSingleByChannel(Floor,Sample+FVector(0,0,20),Sample-FVector(0,0,HalfHeight+65.f),ECC_Visibility,Query)
    || Floor.ImpactNormal.Z<.7f || FMath::Abs(Floor.ImpactPoint.Z-(Start.Z-HalfHeight))>45.f) { Supported=false; break; }
  }
  if(Supported) { Yaw=CandidateYaw; return true; }
 }
 return false;
}

struct FEconomyNetworkProbe
{
 FTimerHandle Timer;
 TWeakObjectPtr<AVanthexCharacter> Participant;
 TWeakObjectPtr<AVanthexPlayerController> RemoteController;
 double Started=0., Ready=-1., BuyProbe=-1., Active=-1., ShotAt=-1.;
 FVector BuyOrigin=FVector::ZeroVector, ActiveOrigin=FVector::ZeroVector;
 int32 BuyAmmo=-1, BuyAmmoAfter=-1, BuyMovementMode=-1, ActiveAmmo=-1, LastCredits=-1, LastAmmo=-1, Phase=-1;
 uint32 AudioBefore=0, AudioAfter=0;
 float Moved=0.f, MovementYaw=0.f, BuyMoved=-1.f;
 bool Seeded=false, NormalBuy=false, Joined=false, SentPurchases=false, Purchased=false;
 bool BuyChecked=false, BuyBlocked=false, Privacy=false, Profile=false;
 bool SawBuyKnife=false, SawBuyReturn=false; int32 WheelStage=0;
 bool SentShot=false, SawShot=false, SawMovement=false, SawAudio=false, ReturnedToBot=false, Finished=false, MovementLane=false;

 void Finish(UWorld* World,bool Host,bool TimedOut)
 {
  if(Finished) return;
  Finished=true;
  if(auto* Pawn=Participant.Get()) Pawn->SetHoldForward(false);
  const bool Pass=!TimedOut && Joined && Purchased && BuyChecked && BuyBlocked && Privacy && Profile && SawShot && SawMovement && SawAudio && SawBuyKnife && SawBuyReturn
   && (Host ? Seeded && NormalBuy && ReturnedToBot : SentPurchases && SentShot && MovementLane);
  const FString Report=FString::Printf(TEXT("%s role=%s joined_seed1577=%d purchases827=%d buy_checked=%d buy_blocked=%d owner_private=%d p250_profile=%d shot=%d movement=%d audio=%d bot_return=%d host_seed=%d normal_buy12=%d sent_purchases=%d sent_shot=%d credits=%d ammo=%d moved_cm=%.1f lane_checked=%d move_yaw=%.1f buy_mode=%d buy_moved_cm=%.1f buy_ammo_before=%d buy_ammo_after=%d audio_before=%u audio_after=%u phase=%d elapsed=%.2f timeout=%d\n"),
   Pass?TEXT("PASS"):TEXT("FAIL"),Host?TEXT("host"):TEXT("client"),Joined,Purchased,BuyChecked,BuyBlocked,Privacy,Profile,SawShot,SawMovement,SawAudio,ReturnedToBot,Seeded,NormalBuy,SentPurchases,SentShot,LastCredits,LastAmmo,Moved,MovementLane,MovementYaw,BuyMovementMode,BuyMoved,BuyAmmo,BuyAmmoAfter,AudioBefore,AudioAfter,Phase,World->GetTimeSeconds()-Started,TimedOut);
  const FString File=FPaths::ProjectSavedDir()/(Host?TEXT("EconomyNetworkHost.txt"):TEXT("EconomyNetworkClient.txt"));
  const bool Saved=FFileHelper::SaveStringToFile(Report,*File);
  UE_LOG(LogVanthex,Display,TEXT("ECONOMY_NETWORK %s saved=%d"),*Report,Saved);
  World->GetTimerManager().ClearTimer(Timer);
  FPlatformMisc::RequestExitWithStatus(false,Pass&&Saved?0:1);
 }
};
}
#endif

// Explicit diagnostic: the client must purchase and fire through its real owned RPCs.
void VanthexStartEconomyNetworkTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
 if(!Controller || !Controller->IsLocalController() || !Controller->GetWorld()) return;
 UWorld* World=Controller->GetWorld();
 const TWeakObjectPtr<AVanthexPlayerController> WeakController(Controller);
 const TWeakObjectPtr<UWorld> WeakWorld(World);
 const auto Probe=MakeShared<FEconomyNetworkProbe>();
 Probe->Started=World->GetTimeSeconds();
 World->GetTimerManager().SetTimer(Probe->Timer,FTimerDelegate::CreateLambda([WeakController,WeakWorld,Probe]()
 {
  if(!WeakController.IsValid() || !WeakWorld.IsValid() || Probe->Finished) return;
  auto* PC=WeakController.Get();
  auto* W=WeakWorld.Get();
  const bool Host=PC->HasAuthority();
  const double Now=W->GetTimeSeconds();
  if(Now-Probe->Started>80.) { Probe->Finish(W,Host,true); return; }
  const auto* GS=W->GetGameState<AVanthexGameState>();
  if(!GS) return;
  Probe->Phase=int32(GS->GetPhase());
  const bool Buy=GS->GetPhase()==EVanthexRoundPhase::BuyPhase;
  const bool Active=GS->GetPhase()==EVanthexRoundPhase::RoundActive;

  if(Host && !Probe->Seeded)
  {
   auto* Own=PC->GetPlayerState<AVanthexPlayerState>();
   auto* BotPawn=FindEconomyPawn(W,EVanthexTeam::Bravo,0);
   auto* Bot=BotPawn?Cast<AVanthexBotController>(BotPawn->GetController()):nullptr;
   auto* BotState=Bot?Bot->GetPlayerState<AVanthexPlayerState>():nullptr;
   if(!Buy || !Own || !PC->GetPawn() || !BotState) return;
   Own->AwardCredits(111);
   const bool OwnBought=Own->TryPurchase(TEXT("ArmorLight"),1);
   BotState->AwardCredits(777);
   Probe->Seeded=OwnBought && Own->GetEconomy().Credits==511 && BotState->GetEconomy().Credits==1577;
   Probe->NormalBuy=GS->GetTimeRemaining()>7.f && GS->GetTimeRemaining()<=12.f;
   Probe->Privacy=Own->GetEconomy().Credits==511 && Own->GetArmorPoints()==40.f;
   Probe->Participant=BotPawn;
   UE_LOG(LogVanthex,Display,TEXT("ECONOMY_NETWORK seeded host=%d bravo0=%d seconds=%.1f"),Own->GetEconomy().Credits,BotState->GetEconomy().Credits,GS->GetTimeRemaining());
   if(!Probe->Seeded) { Probe->Finish(W,true,false); return; }
  }

  if(Host && !Probe->RemoteController.IsValid())
  {
   for(TActorIterator<AVanthexPlayerController> It(W);It;++It)
    if(!It->IsLocalController() && It->GetPawn()==Probe->Participant.Get()) { Probe->RemoteController=*It; break; }
  }
  if(Host && Probe->Joined && !Probe->Participant.IsValid())
  {
   UE_LOG(LogVanthex,Error,TEXT("ECONOMY_NETWORK original roster pawn was destroyed before bot handover"));
   Probe->Finish(W,true,false); return;
  }
  if(!Host) Probe->Participant=Cast<AVanthexCharacter>(PC->GetPawn());
  auto* Pawn=Probe->Participant.Get();
  auto* PS=Pawn?Pawn->GetPlayerState<AVanthexPlayerState>():nullptr;
  auto* Weapon=Pawn?Pawn->GetWeapon():nullptr;
  if(!Pawn || !PS || !Weapon) return;
  Probe->LastCredits=PS->GetEconomy().Credits;
  Probe->LastAmmo=Weapon->GetAmmoInMagazine();

  if(Host && Probe->Joined && Cast<AVanthexBotController>(Pawn->GetController()))
  {
   // The original pawn must survive Logout; its new owner receives the same account.
   Probe->ReturnedToBot=PurchasedState(PS) && Weapon->GetWeaponId()==TEXT("P250") && PS->GetTeam()==EVanthexTeam::Bravo && PS->GetSpawnSlot()==0;
   Probe->Finish(W,true,false);
   return;
  }
  if(Host && !Probe->RemoteController.IsValid()) return;
  if(!Probe->Joined)
  {
   if(!Buy || PS->GetTeam()!=EVanthexTeam::Bravo || PS->GetSpawnSlot()!=0 || PS->GetEconomy().Credits!=1577) return;
   Probe->Joined=true;
   Probe->Ready=Now;
   UE_LOG(LogVanthex,Display,TEXT("ECONOMY_NETWORK join preserves 1577 role=%s pawn=%s"),Host?TEXT("host"):TEXT("client"),*Pawn->GetName());
  }

  if(!Host)
  {
   // Enemy PlayerState exists, but its private economy remains the default client value.
   for(TActorIterator<AVanthexPlayerState> It(W);It;++It)
    if(It->GetTeam()==EVanthexTeam::Alpha && It->GetSpawnSlot()==0)
     Probe->Privacy=It->GetEconomy().Credits==800 && It->GetArmorPoints()==0.f && It->GetEconomy().Sidearm==TEXT("Pistol");
   if(Buy && !Probe->SentPurchases && Now-Probe->Ready>=.5 && PC->GetBuyMenu())
   {
    Probe->SentPurchases=true;
    PC->GetBuyMenu()->ServerPurchase(TEXT("P250"),1);
    PC->GetBuyMenu()->ServerPurchase(TEXT("ArmorLight"),2);
    PC->GetBuyMenu()->ServerPurchase(TEXT("InvalidEconomyProbeItem"),3);
    PC->GetBuyMenu()->ServerPurchase(TEXT("ArmorLight"),2);
    // A changed payload with the same old ID must also fail; full armor alone
    // would otherwise hide a broken replay guard behind the armor-limit check.
    PC->GetBuyMenu()->ServerPurchase(TEXT("Smoke"),2);
   }
  }

  if(Buy && PurchasedState(PS) && Weapon->GetWeaponId()==TEXT("P250"))
  {
   Probe->Purchased=true;
   const auto* Profile=VanthexWeapons::Find(TEXT("P250"));
   Probe->Profile=Profile && Weapon->GetMagazineSize()==Profile->MagazineSize && Weapon->GetReserveAmmo()==Profile->ReserveAmmo && !Weapon->IsAutomatic();
   if(Probe->BuyProbe<0.)
   {
    Probe->BuyProbe=Now;
    Probe->BuyOrigin=Pawn->GetActorLocation();
    Probe->BuyAmmo=Weapon->GetAmmoInMagazine();
    Weapon->FireShot(); // Host checks server gate; client additionally uses its owned RPC below.
    if(!Host) { Pawn->ServerAction(0,false); Pawn->ServerAction(1,false); }
   }
   const double BuyAge=Now-Probe->BuyProbe;
   // HoldForward calls the same DoMove path every Character tick. A 20 Hz
   // timer pulse alone leaves braking frames between input samples at 60 FPS.
   if(!Host) Pawn->SetHoldForward(BuyAge<.8);
   if(BuyAge>=1.2 && !Probe->BuyChecked)
   {
    Probe->BuyChecked=true;
    Probe->BuyAmmoAfter=Weapon->GetAmmoInMagazine();
    Probe->BuyMovementMode=int32(Pawn->GetCharacterMovement()->MovementMode);
    Probe->BuyMoved=float(FVector::Dist2D(Probe->BuyOrigin,Pawn->GetActorLocation()));
    Probe->BuyBlocked=Probe->BuyAmmoAfter==Probe->BuyAmmo && Probe->BuyMoved<3.f && Probe->BuyMovementMode==int32(MOVE_None);
   }
  }

  if(Buy&&Probe->BuyChecked) {
   auto* E=Pawn->GetEquipment();
   if(E&&!E->IsSwitching()) {
    if(Weapon->GetWeaponId()==TEXT("Knife")) Probe->SawBuyKnife=true;
    if(Probe->SawBuyKnife&&Weapon->GetWeaponId()==TEXT("P250")) Probe->SawBuyReturn=true;
    if(!Host&&Probe->WheelStage==0) { Pawn->NextWeapon();++Probe->WheelStage; }
    else if(!Host&&Probe->WheelStage==1&&Probe->SawBuyKnife) { Pawn->PreviousWeapon();++Probe->WheelStage; }
   }
  }
  if(!Active || !Probe->Purchased) return;
  auto* Audio=Host ? Probe->RemoteController->GetWeaponAudio() : PC->GetWeaponAudio();
  if(!Audio) return;
  if(Probe->Active<0.)
  {
   Probe->Active=Now;
   Probe->ActiveOrigin=Pawn->GetActorLocation();
   Probe->ActiveAmmo=Weapon->GetAmmoInMagazine();
   Probe->AudioBefore=Host?Audio->SentShotPackets:Audio->ReceivedShotPackets;
   if(!Host)
   {
    Probe->MovementLane=FindClearMovementYaw(Pawn,Probe->MovementYaw);
    UE_LOG(LogVanthex,Display,TEXT("ECONOMY_NETWORK movement lane clear=%d yaw=%.1f start=%s"),Probe->MovementLane,Probe->MovementYaw,*Pawn->GetActorLocation().ToString());
    if(!Probe->MovementLane) { Probe->Finish(W,false,false); return; }
    // Pitch keeps the later shot above operators; yaw follows a checked free lane.
    PC->SetControlRotation(FRotator(60.f,Probe->MovementYaw,0.f));
   }
  }
  const double ActiveAge=Now-Probe->Active;
  Probe->Moved=FMath::Max(Probe->Moved,float(FVector::Dist2D(Probe->ActiveOrigin,Pawn->GetActorLocation())));
  Probe->SawMovement=Probe->Moved>15.f && Pawn->GetCharacterMovement()->MovementMode!=MOVE_None;
  if(!Host) Pawn->SetHoldForward(ActiveAge<.8);
  if(!Host && ActiveAge>=1.2 && !Probe->SentShot)
  {
   Probe->SentShot=true;
   Probe->ShotAt=Now;
   // Both reliable actions use the real pawn RPC; the semiautomatic pistol fires once.
   Pawn->ServerAction(0,false);
   Pawn->ServerAction(1,false);
  }
  Probe->SawShot=Weapon->GetWeaponId()==TEXT("P250") && Weapon->GetAmmoInMagazine()==Probe->ActiveAmmo-1;
  Probe->AudioAfter=Host?Audio->SentShotPackets:Audio->ReceivedShotPackets;
  Probe->SawAudio=Probe->SawShot && Probe->AudioAfter>Probe->AudioBefore;
  if(!Host && Probe->SentShot && Now-Probe->ShotAt>=3.) Probe->Finish(W,false,false);
 }),.05f,true);
#endif
}
