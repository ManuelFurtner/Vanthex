#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexDamage.h"
#include "VanthexExtractionZone.h"
#include "VanthexGridV3Layout.h"
#include "VanthexNavigation.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
bool FindPassLane(AVanthexGameMode* Mode,FVector& Start,FVector& End)
{
 UWorld* World=Mode->GetWorld();
 TArray<FVector> Sources={Mode->GetCore()->GetHome()+FVector(0,0,30)};
 for(const auto& Room:GridV3::Rooms()) Sources.Add(Room.Center+FVector(0,0,110));
 FCollisionObjectQueryParams Objects;
 Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CoreEconomyLane),false,Mode->GetCore());
 for(const FVector& Source:Sources) for(TActorIterator<AVanthexExtractionZone> Zone(World);Zone;++Zone)
 {
  TArray<FVector> Route;
  if(!VanthexNavigation::FindRoute(World,Source,Zone->GetActorLocation()+FVector(0,0,96),Route)) continue;
  for(int32 I=0;I<Route.Num();++I) for(int32 J=I+1;J<Route.Num();++J)
  {
   const FVector A=Route[I], B=Route[J];
   const float Distance=FVector::Dist2D(A,B);
   if(Distance<1100.f || Distance>1250.f || FMath::Abs(A.Z-B.Z)>2.f) continue;
   if(!VanthexNavigation::CheckWalkableSegment(World,A-FVector(0,0,110),B-FVector(0,0,110))) continue;
   FHitResult Hit;
   if(World->SweepSingleByObjectType(Hit,A,B,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(42,96),Query)) continue;
   const float Before=Mode->ShortestExtractionRoute(A), After=Mode->ShortestExtractionRoute(B);
   // More than 900 cm leaves room for the launch offset and early catch radius.
   if(Before>=TNumericLimits<float>::Max() || After>=TNumericLimits<float>::Max() || Before-After<900.f) continue;
   Start=A; End=B;
   return true;
  }
 }
 return false;
}

struct FCoreEconomyProbe
{
 FTimerHandle Timer;
 FVector Start,End;
 TWeakObjectPtr<AVanthexCharacter> Thrower,Ally,Enemy;
 TArray<FString> Lines;
 int32 Stage=0,Failures=0;
 double Started=0.,StageStarted=0.;
 bool TeleportChecked=false,Finished=false;
 void Check(bool Value,const TCHAR* Name)
 {
  Lines.Add(FString::Printf(TEXT("%s %s"),Value?TEXT("PASS"):TEXT("FAIL"),Name));
  if(!Value) ++Failures;
  UE_LOG(LogVanthex,Display,TEXT("CORE_ECONOMY %s"),*Lines.Last());
 }
 void Finish(UWorld* World)
 {
  if(Finished) return;
  Finished=true;
  Lines.Insert(FString::Printf(TEXT("OVERALL %s\nCHECKS %d\nFAILURES %d\nELAPSED %.2f"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures,World->GetTimeSeconds()-Started),0);
  const bool Saved=FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n"))+TEXT("\n"),*(FPaths::ProjectSavedDir()/TEXT("CoreEconomy.txt")));
  World->GetTimerManager().ClearTimer(Timer);
  FPlatformMisc::RequestExitWithStatus(false,Failures||!Saved?1:0);
 }
};
}
#endif

void VanthexRunCoreEconomyTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode || !Mode->HasAuthority() || !Mode->GetWorld()) return;
 static bool Started=false;
 if(Started) return;
 static const double AwaitStarted=Mode->GetWorld()->GetTimeSeconds();
 bool Ready=Mode->Core && Mode->Combatants.Num()==6;
 for(AVanthexCharacter* Pawn:Mode->Combatants) if(!Pawn || !Pawn->GetHealthComponent() || !Pawn->GetWeapon() || !Mode->GetVanthexPlayerState(Pawn)) Ready=false;
 if(!Ready)
 {
  if(Mode->GetWorld()->GetTimeSeconds()-AwaitStarted>45.)
  {
   Started=true;
   FCoreEconomyProbe Failed; Failed.Started=AwaitStarted;
   Failed.Check(false,TEXT("roster failed to initialize within 45 seconds")); Failed.Finish(Mode->GetWorld());
  }
  return;
 }
 Started=true;
 auto Probe=MakeShared<FCoreEconomyProbe>();
 UWorld* World=Mode->GetWorld();
 Probe->Started=AwaitStarted;
 Mode->GetWorldTimerManager().ClearTimer(Mode->SelfTestTimer);
 Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Probe->Check(Mode->IsBuyPhase(),TEXT("fixture starts in genuine purchase phase"));
 Mode->ActivateRound();
 Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Probe->Check(Mode->IsRoundActive(),TEXT("normal activation enables real Core interactions"));
 Probe->Thrower=Mode->FindCombatant(EVanthexTeam::Alpha,0);
 Probe->Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 Probe->Enemy=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 if(!Probe->Thrower.IsValid() || !Probe->Ally.IsValid() || !Probe->Enemy.IsValid())
 {
  Probe->Check(false,TEXT("roster participants exist")); Probe->Finish(World); return;
 }
 // Keep test participants stationary; only the real Core flight simulation moves.
 for(int32 I=0;I<Mode->Combatants.Num();++I)
 {
  AVanthexCharacter* Pawn=Mode->Combatants[I];
  Pawn->GetCharacterMovement()->StopMovementImmediately();
  Pawn->GetCharacterMovement()->DisableMovement();
  Pawn->SetActorEnableCollision(false);
  Pawn->SetActorLocation(FVector(-3000+I*500,0,2200),false,nullptr,ETeleportType::TeleportPhysics);
  Mode->GetVanthexPlayerState(Pawn)->ResetEconomyForMatch();
 }
 const bool Found=FindPassLane(Mode,Probe->Start,Probe->End);
 Probe->Check(Found,TEXT("real collision graph supplies flat clear 1100-1250 cm lane with >=900 cm extraction progress"));
 if(!Found) { Probe->Finish(World); return; }
 UE_LOG(LogVanthex,Display,TEXT("CORE_ECONOMY lane from=%s to=%s"),*Probe->Start.ToString(),*Probe->End.ToString());
 const TWeakObjectPtr<AVanthexGameMode> WeakMode(Mode);
 Probe->StageStarted=World->GetTimeSeconds();
 World->GetTimerManager().SetTimer(Probe->Timer,FTimerDelegate::CreateLambda([WeakMode,Probe]()
 {
  auto* GM=WeakMode.Get();
  if(!GM || Probe->Finished) return;
  auto* W=GM->GetWorld();
  auto* Core=GM->Core.Get();
  auto* Human=Probe->Thrower.Get(); auto* Ally=Probe->Ally.Get(); auto* Enemy=Probe->Enemy.Get();
  if(!W || !Core || !Human || !Ally || !Enemy) return;
  const double Now=W->GetTimeSeconds(),Age=Now-Probe->StageStarted;
  if(Now-Probe->Started>45.) { Probe->Check(false,TEXT("45-second bounded Core test timeout")); Probe->Finish(W); return; }
  auto* H=Human->GetPlayerState<AVanthexPlayerState>();
  auto* A=Ally->GetPlayerState<AVanthexPlayerState>();
  auto* E=Enemy->GetPlayerState<AVanthexPlayerState>();
  const FVector Direction=(Probe->End-Probe->Start).GetSafeNormal2D();
  auto Advance=[&](int32 Next) { Probe->Stage=Next; Probe->StageStarted=Now; };
  auto Park=[&]() { Ally->SetActorLocation(FVector(0,0,2200)); Enemy->SetActorLocation(FVector(700,0,2200)); };
  auto Pickup=[&](AVanthexCharacter* Catcher,const FVector& CatchPosition)
  {
   Core->ResetToSpawn();
   Park();
   Human->GetHealthComponent()->ResetHealth(); H->SetAlive(true);
   Human->GetCharacterMovement()->DisableMovement();
   Human->SetActorLocation(Probe->Start,false,nullptr,ETeleportType::TeleportPhysics);
   Human->GetController()->SetControlRotation(Direction.Rotation());
   if(Catcher && Catcher!=Human) Catcher->SetActorLocation(CatchPosition,false,nullptr,ETeleportType::TeleportPhysics);
   Core->SetHome(Probe->Start+FVector(0,0,-30));
   Core->ResetToSpawn();
   const bool Picked=Core->TryPickup(Human);
   Probe->Check(Picked,TEXT("real TryPickup attaches Core before case"));
   return Picked;
  };
  switch(Probe->Stage)
  {
  case 0:
   if(!Pickup(Ally,Probe->End)) { Probe->Finish(W); return; }
   Core->Throw();
   Probe->Check(Core->GetCoreState()==EVanthexCoreState::Thrown && Core->IsRewardEligible(),TEXT("real Throw starts an eligible flight"));
   Advance(1); break;
  case 1:
   if(Core->GetCarrier()!=Ally && Age<3.) break;
   Probe->Check(Core->GetCarrier()==Ally,TEXT("world-ticked flight is caught by teammate"));
   Probe->Check(H->GetEconomy().Credits==850 && A->GetEconomy().Credits==800,TEXT("genuine qualifying catch pays exactly 50 to thrower"));
   Advance(2); break;
  case 2:
   if(Age<.4) break;
   Probe->Check(H->GetEconomy().Credits==850,TEXT("subsequent world ticks cannot pay the catch twice"));
   if(!Pickup(nullptr,FVector::ZeroVector)) { Probe->Finish(W); return; }
   Core->Throw();
   Human->SetActorLocation(Probe->End,false,nullptr,ETeleportType::TeleportPhysics);
   Advance(3); break;
  case 3:
   if(Core->GetCarrier()!=Human && Age<3.) break;
   Probe->Check(Core->GetCarrier()==Human,TEXT("thrower can physically catch own returning Core"));
   Probe->Check(H->GetEconomy().Credits==850,TEXT("own catch does not pay"));
   if(!Pickup(Ally,Probe->Start+Direction*200.f)) { Probe->Finish(W); return; }
   Core->Drop();
   Probe->Check(!Core->IsRewardEligible(),TEXT("manual Drop is ineligible immediately"));
   Advance(4); break;
  case 4:
   if(Age<1.2) break;
   Probe->Check(Core->GetCarrier()==nullptr,TEXT("manual drop does not become an automatic rewarding catch"));
   Probe->Check(Core->TryPickup(Ally),TEXT("teammate physically picks settled drop from floor"));
   Probe->Check(H->GetEconomy().Credits==850 && A->GetEconomy().Credits==800,TEXT("floor pickup pays no pass bonus"));
   if(!Pickup(Ally,Probe->Start+Direction*140.f)) { Probe->Finish(W); return; }
   {
    FVanthexDamageRequest Death; Death.Damage=100.f;
    Human->GetHealthComponent()->ApplyDamage(Death);
   }
   Probe->Check(Human->GetHealthComponent()->IsDead() && !Core->IsRewardEligible(),TEXT("actual death releases an ineligible Core"));
   Advance(5); break;
  case 5:
   if(Core->GetCarrier()!=Ally && Age<2.) break;
   Probe->Check(Core->GetCarrier()==Ally,TEXT("teammate catches actual death drop during world ticks"));
   Probe->Check(H->GetEconomy().Credits==850 && A->GetEconomy().Credits==800,TEXT("death drop catch pays no bonus"));
   if(!Pickup(Enemy,Probe->End)) { Probe->Finish(W); return; }
   Core->Throw();
   Probe->Check(Core->IsRewardEligible(),TEXT("teleport case starts from eligible real throw"));
   Core->SetActorLocation(Core->GetActorLocation()+Direction*10.f,false,nullptr,ETeleportType::TeleportPhysics);
   Advance(6); break;
  case 6:
   if(Age>=.1 && !Probe->TeleportChecked)
   {
    Probe->TeleportChecked=true;
    Probe->Check(Core->GetCoreState()==EVanthexCoreState::Thrown && !Core->IsRewardEligible(),TEXT("10 cm external teleport invalidates reward on next real flight ticks"));
   }
   if(Core->GetCarrier()!=Enemy && Age<3.) break;
   Probe->Check(Core->GetCarrier()==Enemy,TEXT("teleported flight can still be caught for gameplay"));
   Probe->Check(E->GetEconomy().Credits==800,TEXT("teleported flight pays no intercept"));
   if(!Pickup(Enemy,Probe->End)) { Probe->Finish(W); return; }
   Core->Throw();
   Core->ResetToSpawn();
   Probe->Check(!Core->IsRewardEligible() && Core->GetCoreState()==EVanthexCoreState::AtRest,TEXT("reset during flight immediately invalidates reward"));
   Advance(7); break;
  case 7:
   if(Age<.8) break;
   Probe->Check(!Core->GetCarrier() && E->GetEconomy().Credits==800,TEXT("reset flight cannot later catch or pay"));
   if(!Pickup(Enemy,Probe->End)) { Probe->Finish(W); return; }
   Core->Throw();
   Advance(8); break;
  case 8:
   if(Core->GetCarrier()!=Enemy && Age<3.) break;
   Probe->Check(Core->GetCarrier()==Enemy,TEXT("opponent catches real untampered throw"));
   Probe->Check(E->GetEconomy().Credits==900 && H->GetEconomy().Credits==850,TEXT("real intercept pays exactly 100 without a pass reward"));
   Advance(9); break;
  default:
   if(Age<.4) break;
   Probe->Check(E->GetEconomy().Credits==900,TEXT("intercept remains single payment on subsequent ticks"));
   Probe->Finish(W); break;
  }
 }),.05f,true);
#endif
}
