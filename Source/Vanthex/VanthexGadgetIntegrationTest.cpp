#include "VanthexGameMode.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexGadgetActor.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGadgetRules.h"
#include "VanthexPlayerState.h"
#include "VanthexShiftDevice.h"
#include "HealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
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
const FVector GadgetOrigin(0,0,20000);

AActor* MakeGadgetBlocker(UWorld* World,const FVector& Location,const FVector& Extent)
{
 auto* Actor=World->SpawnActor<AActor>();
 if(!Actor) return nullptr;
 auto* Box=NewObject<UBoxComponent>(Actor,TEXT("GadgetDiagnosticBlocker"));
 Actor->SetRootComponent(Box);Actor->AddInstanceComponent(Box);
 Box->SetBoxExtent(Extent);Box->SetCollisionProfileName(TEXT("BlockAll"));
 Box->RegisterComponent();Actor->SetActorLocation(Location);
 return Actor;
}

AVanthexGadgetActor* MakeGadget(UWorld* World,FName Id,AVanthexCharacter* Thrower,const FVector& Position,bool Activate=true)
{
 const FTransform Transform(FRotator::ZeroRotator,Position);
 auto* Gadget=World->SpawnActorDeferred<AVanthexGadgetActor>(AVanthexGadgetActor::StaticClass(),Transform,Thrower,Thrower,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Gadget)return nullptr;
 Gadget->Configure(Id,Thrower);Gadget->FinishSpawning(Transform);
 if(Activate)Gadget->Detonate();
 return Gadget;
}

int32 ActiveMarks(AVanthexCharacter* Character)
{
 if(!Character||!Character->GetGadgets())return 0;
 const auto* Component=Character->GetGadgets();int32 Count=0;
 for(const auto& Mark:Component->GetMarks())if(Mark.Expires>Component->GetServerTime())++Count;
 return Count;
}

int32 LiveGadgets(UWorld* World)
{
 int32 Count=0;for(TActorIterator<AVanthexGadgetActor> It(World);It;++It)if(IsValid(*It))++Count;return Count;
}

struct FGadgetProbe
{
 FTimerHandle Timer;
 TArray<FString> Lines;
 TArray<TWeakObjectPtr<AActor>> Fixtures;
 TWeakObjectPtr<AVanthexCharacter> Human,Ally,Enemy,HiddenEnemy;
 TWeakObjectPtr<AVanthexGadgetActor> Thrown,Sensor,Smoke;
 TWeakObjectPtr<AVanthexShiftDevice> PoweredDevice,UnpoweredDevice;
 TWeakObjectPtr<AActor> Wall,Ceiling;
 FVector ThrowStart;
 int32 Stage=0,Failures=0;
 double Started=0,StageStarted=0,EMPStarted=0,SmokeStarted=0,SensorStarted=0;
 bool Finished=false;
 void Check(bool Value,const TCHAR* Name)
 {
  Lines.Add(FString::Printf(TEXT("%s %s"),Value?TEXT("PASS"):TEXT("FAIL"),Name));
  if(!Value)++Failures;
  UE_LOG(LogVanthex,Display,TEXT("GADGET_TEST %s"),*Lines.Last());
 }
 void Finish(UWorld* World)
 {
  if(Finished)return;Finished=true;
  AVanthexGadgetActor::ResetAll(World);
  for(auto& Actor:Fixtures)if(Actor.IsValid())Actor->Destroy();
  Lines.Insert(FString::Printf(TEXT("OVERALL %s\nCHECKS %d\nFAILURES %d\nELAPSED %.2f"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures,World->GetTimeSeconds()-Started),0);
  const bool Saved=FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n"))+TEXT("\n"),*(FPaths::ProjectSavedDir()/TEXT("GadgetIntegration.txt")));
  const uint8 ExitCode=Failures||!Saved?1:0;
  World->GetTimerManager().ClearTimer(Timer);
  FPlatformMisc::RequestExitWithStatus(false,ExitCode);
 }
};
}
#endif

void VanthexRunGadgetTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority()||!Mode->GetWorld())return;
 static bool Started=false;if(Started)return;
 static const double AwaitStarted=Mode->GetWorld()->GetTimeSeconds();
 bool Ready=Mode->Combatants.Num()==6;
 for(AVanthexCharacter* Pawn:Mode->Combatants)if(!Pawn||!Pawn->GetGadgets()||!Pawn->GetHealthComponent()||!Mode->GetVanthexPlayerState(Pawn))Ready=false;
 if(!Ready)
 {
  if(Mode->GetWorld()->GetTimeSeconds()-AwaitStarted>45.)
  {Started=true;FGadgetProbe P;P.Started=AwaitStarted;P.Check(false,TEXT("roster initializes within 45 seconds"));P.Finish(Mode->GetWorld());}
  return;
 }
 Started=true;
 auto Probe=MakeShared<FGadgetProbe>();Probe->Started=AwaitStarted;
 UWorld* World=Mode->GetWorld();
 Mode->GetWorldTimerManager().ClearTimer(Mode->SelfTestTimer);Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 Probe->Human=Mode->FindCombatant(EVanthexTeam::Alpha,0);Probe->Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 Probe->Enemy=Mode->FindCombatant(EVanthexTeam::Bravo,0);Probe->HiddenEnemy=Mode->FindCombatant(EVanthexTeam::Bravo,1);
 if(!Probe->Human.IsValid()||!Probe->Ally.IsValid()||!Probe->Enemy.IsValid()||!Probe->HiddenEnemy.IsValid())
 {Probe->Check(false,TEXT("four roster participants exist"));Probe->Finish(World);return;}
 Probe->Check(Mode->IsBuyPhase()&&!Mode->UsesLegacyCombatFixture(),TEXT("normal purchase phase, not legacy combat fixture"));
 for(int32 I=0;I<Mode->Combatants.Num();++I)
 {
  AVanthexCharacter* Pawn=Mode->Combatants[I];Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();
  Pawn->SetActorEnableCollision(false);Pawn->SetActorLocation(GadgetOrigin+FVector(5000+I*500,5000,0),false,nullptr,ETeleportType::TeleportPhysics);
  Mode->GetVanthexPlayerState(Pawn)->ResetEconomyForMatch();
 }
 auto* Human=Probe->Human.Get();auto* State=Human->GetPlayerState<AVanthexPlayerState>();
 Probe->Check(State->TryPurchase(TEXT("Flash"),1)&&State->TryPurchase(TEXT("Flash"),2),TEXT("real purchases provide two Flash inventory slots"));
 Probe->Check(!Human->GetGadgets()->TryThrow(0,1)&&State->GetEconomy().Gadgets.Num()==2&&LiveGadgets(World)==0,TEXT("purchase-phase throw rejected without consuming or spawning"));
 Mode->ActivateRound();Mode->GetWorldTimerManager().ClearTimer(Mode->FlowTimer);
 for(AVanthexCharacter* Pawn:Mode->Combatants)Pawn->GetCharacterMovement()->DisableMovement();
 Probe->Fixtures.Add(MakeGadgetBlocker(World,GadgetOrigin+FVector(0,0,-110),FVector(2200,2200,10)));
 Human->SetActorLocation(GadgetOrigin);Human->GetController()->SetControlRotation(FRotator::ZeroRotator);
 Probe->Check(!Human->GetGadgets()->TryThrow(-1,2)&&State->GetEconomy().Gadgets.Num()==2,TEXT("negative slot rejected without consumption"));
 State->SetAlive(false);
 Probe->Check(!Human->GetGadgets()->TryThrow(0,3)&&State->GetEconomy().Gadgets.Num()==2,TEXT("nonliving roster participant cannot throw"));
 State->SetAlive(true);
 Probe->Check(Human->GetGadgets()->TryThrow(0,4)&&State->GetEconomy().Gadgets.Num()==1&&LiveGadgets(World)==1,TEXT("active authorized throw consumes exactly one slot and creates one actor"));
 for(TActorIterator<AVanthexGadgetActor> It(World);It;++It){Probe->Thrown=*It;Probe->ThrowStart=It->GetActorLocation();}
 Probe->Check(!Human->GetGadgets()->TryThrow(0,4)&&!Human->GetGadgets()->TryThrow(0,5)&&State->GetEconomy().Gadgets.Num()==1&&LiveGadgets(World)==1,TEXT("duplicate request and throw cooldown cannot consume the other slot"));
 Probe->StageStarted=World->GetTimeSeconds();
 const TWeakObjectPtr<AVanthexGameMode> WeakMode(Mode);
 World->GetTimerManager().SetTimer(Probe->Timer,FTimerDelegate::CreateLambda([WeakMode,Probe]()
 {
  // ClearTimer can destroy this callback's capture while Finish is executing.
  const auto Lifetime=Probe;
  auto* GM=WeakMode.Get();if(!GM||Probe->Finished)return;auto* W=GM->GetWorld();
  auto* Human=Probe->Human.Get();auto* Ally=Probe->Ally.Get();auto* Enemy=Probe->Enemy.Get();auto* Hidden=Probe->HiddenEnemy.Get();
  if(!W||!Human||!Ally||!Enemy||!Hidden){if(W){Probe->Check(false,TEXT("participants survive diagnostic"));Probe->Finish(W);}return;}
  const double Now=W->GetTimeSeconds(),Age=Now-Probe->StageStarted;
  if(Now-Probe->Started>45.){Probe->Check(false,TEXT("45-second bounded gadget test timeout"));Probe->Finish(W);return;}
  auto* H=Human->GetPlayerState<AVanthexPlayerState>();auto* E=Enemy->GetPlayerState<AVanthexPlayerState>();
  auto Advance=[&](int32 Next){Probe->Stage=Next;Probe->StageStarted=Now;};
  auto Park=[&](){for(int32 I=0;I<GM->Combatants.Num();++I){AVanthexCharacter* P=GM->Combatants[I];P->GetCharacterMovement()->DisableMovement();P->SetActorEnableCollision(false);P->SetActorLocation(GadgetOrigin+FVector(5000+I*500,5000,0));P->GetGadgets()->ResetForRound();}};
  auto Purchase=[&](AVanthexCharacter* Pawn,FName Id)
  {
   GM->Phase=EVanthexRoundPhase::BuyPhase;GM->PublishMatch();auto* PS=Pawn->GetPlayerState<AVanthexPlayerState>();PS->ResetEconomyForMatch();PS->AwardCredits(6200);
   const bool Result=PS->TryPurchase(Id,1);GM->Phase=EVanthexRoundPhase::RoundActive;GM->PublishMatch();return Result;
  };
  switch(Probe->Stage)
  {
  case 0:
   if(Age<.25)break;
   Probe->Check(Probe->Thrown.IsValid()&&!Probe->Thrown->bActivated&&FVector::Dist(Probe->ThrowStart,Probe->Thrown->GetActorLocation())>100.f,TEXT("thrown actor moves through actual world ticks before fuse expires"));
   Advance(1);break;
  case 1:
   if(Age<1.5)break;
   Probe->Check(Probe->Thrown.IsValid()&&Probe->Thrown->bActivated,TEXT("Flash fuse detonates from actor ticks without manual activation"));
   Probe->Check(Probe->Thrown.IsValid()&&Probe->Thrown->GetActorLocation().Z>=GadgetOrigin.Z-100.0,TEXT("physical thrown actor collides with diagnostic floor instead of falling through"));
   AVanthexGadgetActor::ResetAll(W);Human->GetGadgets()->ResetForRound();
   Probe->Check(!H->ConsumeGadget(0,TEXT("Frag"))&&H->GetEconomy().Gadgets.Num()==1,TEXT("expected-ID mismatch cannot consume another gadget"));
   Probe->Wall=MakeGadgetBlocker(W,Human->GetPawnViewLocation(),FVector(80,80,80));Probe->Fixtures.Add(Probe->Wall);
   Probe->Check(!Human->GetGadgets()->TryThrow(0,6)&&H->GetEconomy().Gadgets.Num()==1&&LiveGadgets(W)==0,TEXT("blocked spawn rejects throw atomically"));
   if(Probe->Wall.IsValid())Probe->Wall->Destroy();
   Probe->Check(Human->GetGadgets()->TryThrow(0,7)&&H->GetEconomy().Gadgets.IsEmpty(),TEXT("second valid throw consumes final slot"));
   AVanthexGadgetActor::ResetAll(W);Park();
   Human->SetActorLocation(GadgetOrigin+FVector(400,-300,0));Human->GetController()->SetControlRotation((GadgetOrigin+FVector(0,0,64)-Human->GetPawnViewLocation()).Rotation());
   Enemy->SetActorLocation(GadgetOrigin+FVector(400,0,0));Enemy->GetController()->SetControlRotation(FRotator(0,180,0));
   Ally->SetActorLocation(GadgetOrigin+FVector(400,300,0));Ally->GetController()->SetControlRotation(FRotator::ZeroRotator);
   Hidden->SetActorLocation(GadgetOrigin+FVector(-400,0,0));Hidden->GetController()->SetControlRotation(FRotator::ZeroRotator);
   Probe->Wall=MakeGadgetBlocker(W,GadgetOrigin+FVector(-200,0,64),FVector(10,180,180));Probe->Fixtures.Add(Probe->Wall);
   if(auto* Flash=MakeGadget(W,TEXT("Flash"),Human,GadgetOrigin+FVector(0,0,64)))
   {
    const float Full=Enemy->GetGadgets()->GetFlashOpacity(),Away=Ally->GetGadgets()->GetFlashOpacity();
    Probe->Check(Full>.5f&&Away>0.f&&Away<Full,TEXT("real Flash applies range/facing falloff"));
    Probe->Check(Human->GetGadgets()->IsBlinded(),TEXT("thrower can blind self; Flash is not team-immune"));
    Probe->Check(!Hidden->GetGadgets()->IsBlinded(),TEXT("solid wall blocks Flash"));
    Flash->Detonate();Probe->Check(FMath::IsNearlyEqual(Full,Enemy->GetGadgets()->GetFlashOpacity()),TEXT("repeated Detonate does not reapply Flash"));
    Probe->Check(Enemy->GetHealthComponent()->GetHealth()==100.f,TEXT("Flash does not deal HP damage"));
   }else Probe->Check(false,TEXT("Flash actor spawns"));
   if(Probe->Wall.IsValid())Probe->Wall->Destroy();AVanthexGadgetActor::ResetAll(W);Park();
   Probe->Check(Purchase(Human,TEXT("ArmorLight"))&&Purchase(Enemy,TEXT("ArmorHeavy")),TEXT("real armor purchases prepare explosion case"));
   Human->SetActorLocation(GadgetOrigin+FVector(0,-325,0));Enemy->SetActorLocation(GadgetOrigin+FVector(325,0,0));
   Ally->SetActorLocation(GadgetOrigin+FVector(0,325,0));Hidden->SetActorLocation(GadgetOrigin+FVector(-325,0,0));
   Probe->Wall=MakeGadgetBlocker(W,GadgetOrigin+FVector(-150,0,0),FVector(10,140,150));Probe->Fixtures.Add(Probe->Wall);
   if(auto* Frag=MakeGadget(W,TEXT("Frag"),Human,GadgetOrigin))
   {
    Probe->Check(FMath::IsNearlyEqual(Enemy->GetHealthComponent()->GetHealth(),67.f,.05f)&&FMath::IsNearlyEqual(E->GetArmorPoints(),58.f,.05f),TEXT("55 explosion damage applies 40 percent heavy armor absorption"));
    Probe->Check(FMath::IsNearlyEqual(Human->GetHealthComponent()->GetHealth(),58.75f,.05f)&&FMath::IsNearlyEqual(H->GetArmorPoints(),26.25f,.05f),TEXT("self blast applies 25 percent light armor absorption"));
    Probe->Check(Ally->GetHealthComponent()->GetHealth()==100.f,TEXT("Frag friendly fire is disabled"));
    Probe->Check(Hidden->GetHealthComponent()->GetHealth()==100.f,TEXT("solid wall blocks explosion damage"));
    Frag->Detonate();Probe->Check(FMath::IsNearlyEqual(Enemy->GetHealthComponent()->GetHealth(),67.f,.05f),TEXT("repeat explosion cannot damage twice"));
   }else Probe->Check(false,TEXT("Frag actor spawns"));
   if(Probe->Wall.IsValid())Probe->Wall->Destroy();AVanthexGadgetActor::ResetAll(W);Park();
   Human->SetActorLocation(GadgetOrigin+FVector(-1500,0,0));Ally->SetActorLocation(GadgetOrigin+FVector(-1500,300,0));
   Enemy->SetActorLocation(GadgetOrigin+FVector(600,0,0));Hidden->SetActorLocation(GadgetOrigin+FVector(1200,0,0));
   Probe->Sensor=MakeGadget(W,TEXT("Sensor"),Human,GadgetOrigin);Probe->SensorStarted=Now;
   Probe->Check(Probe->Sensor.IsValid()&&Probe->Sensor->IsSensorActive(),TEXT("deployed Sensor activates"));
   Advance(2);break;
  case 2:
   if(Age<1.15)break;
   Probe->Check(ActiveMarks(Human)==1&&ActiveMarks(Ally)==1,TEXT("Sensor reports one in-range enemy to both teammates"));
   Probe->Check(ActiveMarks(Enemy)==0&&ActiveMarks(Hidden)==0,TEXT("Sensor never publishes positions to opponents"));
   Probe->Smoke=MakeGadget(W,TEXT("Smoke"),Human,GadgetOrigin+FVector(300,0,-140),false);
   Probe->Check(Probe->Smoke.IsValid()&&!AVanthexGadgetActor::SmokeBlocks(W,GadgetOrigin,Enemy->GetActorLocation()),TEXT("unactivated Smoke cannot occlude"));
   if(Probe->Smoke.IsValid())Probe->Smoke->Detonate();
   Probe->Check(Probe->Smoke.IsValid()&&Probe->Smoke->IsSmokeActive()&&AVanthexGadgetActor::SmokeBlocks(W,GadgetOrigin,Enemy->GetActorLocation()),TEXT("activated Smoke blocks real Sensor sightline"));
   Human->GetGadgets()->ResetForRound();Ally->GetGadgets()->ResetForRound();Advance(3);break;
  case 3:
  {
   if(Age<1.25)break;
   Probe->Check(ActiveMarks(Human)==0&&ActiveMarks(Ally)==0,TEXT("Sensor ticks cannot reveal enemies through smoke"));
   if(Probe->Smoke.IsValid())Probe->Smoke->Destroy();
   // The ray stays entirely in one room while the smoke's raw radius overlaps
   // from the room beside it. A solid dividing wall must clip gameplay as well as VFX.
   auto* RoomWall=MakeGadgetBlocker(W,GadgetOrigin+FVector(300,150,0),FVector(800,10,220));Probe->Fixtures.Add(RoomWall);
   auto* AdjacentSmoke=MakeGadget(W,TEXT("Smoke"),Human,GadgetOrigin+FVector(300,300,-140));
   Probe->Check(AdjacentSmoke&&VanthexGadgetRules::SegmentIntersectsSmoke(GadgetOrigin,Enemy->GetActorLocation(),GadgetOrigin+FVector(300,300,0),450.f)
    &&!AVanthexGadgetActor::SmokeBlocks(W,GadgetOrigin,Enemy->GetActorLocation()),TEXT("wall-isolated smoke in parallel room cannot occlude an otherwise clear sightline"));
   if(RoomWall)RoomWall->Destroy();
   Probe->Check(AdjacentSmoke&&AVanthexGadgetActor::SmokeBlocks(W,GadgetOrigin,Enemy->GetActorLocation()),TEXT("same smoke occludes when the separating wall is removed"));
   if(AdjacentSmoke)AdjacentSmoke->Destroy();
   Probe->Wall=MakeGadgetBlocker(W,GadgetOrigin+FVector(300,0,0),FVector(15,200,180));Probe->Fixtures.Add(Probe->Wall);Advance(4);break;
  }
  case 4:
   if(Age<1.25)break;
   Probe->Check(ActiveMarks(Human)==0,TEXT("Sensor ticks cannot reveal enemies through a solid wall"));
   if(Probe->Wall.IsValid())Probe->Wall->Destroy();Advance(5);break;
  case 5:
   if(Age<1.15)break;
   Probe->Check(ActiveMarks(Human)==1,TEXT("Sensor resumes after sightline is restored"));
   for(int32 I=0;I<2;++I)
   {
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* D=W->SpawnActor<AVanthexShiftDevice>(GadgetOrigin+FVector(I?400.f:-400.f,300.f,0),FRotator::ZeroRotator,P);
    if(!D){Probe->Check(false,TEXT("EMP device fixture spawns"));continue;}
    D->Configure(3);D->Circuit=900+I;D->bPowered=I==0;Probe->Fixtures.Add(D);
    if(I==0)Probe->PoweredDevice=D;else Probe->UnpoweredDevice=D;
   }
   Ally->SetActorLocation(GadgetOrigin+FVector(0,500,0));
   MakeGadget(W,TEXT("EMP"),Human,GadgetOrigin);Probe->EMPStarted=Now;
   Probe->Check(Ally->GetGadgets()->IsJammed()&&ActiveMarks(Ally)==0,TEXT("EMP jams player electronics and clears received marks"));
   Probe->Check(Probe->Sensor.IsValid()&&!Probe->Sensor->IsSensorActive(),TEXT("EMP disables deployed Sensor"));
   Probe->Check(Probe->PoweredDevice.IsValid()&&Probe->PoweredDevice->IsEMPDisabled()&&Probe->PoweredDevice->bPowered,TEXT("EMP disables powered terminal without overwriting circuit state"));
   Probe->Check(Probe->UnpoweredDevice.IsValid()&&Probe->UnpoweredDevice->IsEMPDisabled()&&!Probe->UnpoweredDevice->bPowered,TEXT("EMP preserves previously unpowered circuit"));
   Probe->Check(!Probe->UnpoweredDevice.IsValid()||!Probe->UnpoweredDevice->Activate(Ally,1200.f),TEXT("jammed terminal rejects hack interaction"));
   Probe->Smoke=MakeGadget(W,TEXT("Smoke"),Human,GadgetOrigin+FVector(0,2500,0));Probe->SmokeStarted=Now;
   Probe->Fixtures.Add(MakeGadgetBlocker(W,GadgetOrigin+FVector(10000,0,-10),FVector(1200,1000,10)));
   Human->SetActorEnableCollision(true);Human->SetActorLocation(GadgetOrigin+FVector(10000,0,98),false,nullptr,ETeleportType::TeleportPhysics);
   Human->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Human->SetCrouching(true);Advance(6);break;
  case 6:
   if(Age<.35)break;
   Probe->Check(Human->bIsCrouched&&FMath::IsNearlyEqual(Human->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),60.f),TEXT("crouch applies 60 cm collision half-height through movement ticks"));
   Probe->Check(FMath::IsNearlyEqual(Human->GetCharacterMovement()->GetMaxSpeed(),238.f),TEXT("crouching with firearm applies the 15 percent movement penalty"));
   Probe->Check(FMath::IsNearlyEqual(Human->GetFirstPersonCamera()->GetRelativeLocation().Z,42.0),TEXT("crouch lowers first-person camera to 42 cm"));
   Probe->Ceiling=MakeGadgetBlocker(W,GadgetOrigin+FVector(10000,0,160),FVector(200,200,10));Probe->Fixtures.Add(Probe->Ceiling);
   Human->SetCrouching(false);Advance(7);break;
  case 7:
   if(Age<.35)break;
   Probe->Check(Human->bIsCrouched&&FMath::IsNearlyEqual(Human->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),60.f),TEXT("low ceiling prevents standing without capsule penetration"));
   Human->SetActorLocation(Human->GetActorLocation()+FVector(500,0,0),false,nullptr,ETeleportType::TeleportPhysics);Human->SetCrouching(false);Advance(8);break;
  case 8:
   if(Age<.35)break;
   Probe->Check(!Human->bIsCrouched&&FMath::IsNearlyEqual(Human->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),96.f),TEXT("clear space restores standing 96 cm capsule"));
   Probe->Check(FMath::IsNearlyEqual(Human->GetFirstPersonCamera()->GetRelativeLocation().Z,64.0),TEXT("standing restores first-person camera to 64 cm"));
   Probe->Check(ActiveMarks(Human)==0,TEXT("old Sensor marks expire within 1.25 seconds during EMP"));
   Advance(9);break;
  case 9:
   if(Now-Probe->EMPStarted<8.25)break;
   Probe->Check(!Ally->GetGadgets()->IsJammed(),TEXT("player EMP state expires after eight seconds"));
   Probe->Check(Probe->PoweredDevice.IsValid()&&!Probe->PoweredDevice->IsEMPDisabled()&&Probe->PoweredDevice->bPowered,TEXT("powered terminal recovers automatically with original circuit state"));
   Probe->Check(Probe->UnpoweredDevice.IsValid()&&!Probe->UnpoweredDevice->IsEMPDisabled()&&!Probe->UnpoweredDevice->bPowered,TEXT("unpowered terminal remains unpowered after EMP expires"));
   Probe->Check(Probe->Sensor.IsValid()&&Probe->Sensor->IsSensorActive(),TEXT("Sensor resumes after EMP while deployment lifetime remains"));
   Advance(10);break;
  case 10:
  {
   if(Now-Probe->SmokeStarted<18.2)break;
   Probe->Check(!Probe->Smoke.IsValid()||!Probe->Smoke->IsSmokeActive(),TEXT("Smoke expires after eighteen seconds of world time"));
   Probe->Check(!Probe->Sensor.IsValid()||!Probe->Sensor->IsSensorActive(),TEXT("Sensor deployment expires after fifteen seconds"));
   Probe->Check(!Enemy->GetGadgets()->IsBlinded(),TEXT("Flash exposure does not persist indefinitely"));
   Probe->Check(Purchase(Human,TEXT("Sensor")),TEXT("new inventory prepared for round reset"));
   auto* Smoke=MakeGadget(W,TEXT("Smoke"),Human,GadgetOrigin);Human->GetGadgets()->ApplyFlash(1.f);Human->GetGadgets()->ApplyEMP(8.f);
   Probe->Check(Smoke&&Smoke->IsSmokeActive()&&Human->GetGadgets()->IsBlinded()&&Human->GetGadgets()->IsJammed(),TEXT("live effects exist before actual BeginRound reset"));
   GM->BeginRound();GM->GetWorldTimerManager().ClearTimer(GM->FlowTimer);
   Probe->Check(LiveGadgets(W)==0&&!Human->GetGadgets()->IsBlinded()&&!Human->GetGadgets()->IsJammed()&&ActiveMarks(Human)==0,TEXT("actual round transition removes actors and clears temporary effects"));
   Probe->Check(H->GetEconomy().Gadgets.Num()==1&&H->GetEconomy().Gadgets[0]==FName(TEXT("Sensor")),TEXT("unused survivor gadget remains in inventory across round transition"));
   GM->ActivateRound();GM->GetWorldTimerManager().ClearTimer(GM->FlowTimer);
   Probe->Check(!Human->GetGadgets()->TryThrow(0,7)&&H->GetEconomy().Gadgets.Num()==1,TEXT("round reset preserves request high-water mark against old packet replay"));
   Probe->Finish(W);break;
  }
  default:Probe->Check(false,TEXT("valid diagnostic stage"));Probe->Finish(W);break;
  }
 }),.05f,true);
#endif
}
