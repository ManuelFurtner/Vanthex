#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "VanthexFlag.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexBotController.h"
#include "VanthexNavigation.h"
#include "VanthexTestArena.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#endif
void VanthexRunCTFTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority())return;
 static int32 Step=0,Failures=0;static float DiedAt=0,MatchEndedAt=0;static TArray<FString> Lines;
 if(Step==100)return;
 static const double StartedAt=FPlatformTime::Seconds();
 auto Check=[&](bool OK,const TCHAR* What){Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+What);if(!OK)++Failures;};
 auto Finish=[&]() {
  Step=100;
  const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"));
  FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("CTFTest.txt")));
  FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
 };
 if(FPlatformTime::Seconds()-StartedAt>35.0) {
  Check(false,TEXT("CTF integration test completed within 35 seconds"));
  Finish();
  return;
 }
 auto* A=Mode->FindCombatant(EVanthexTeam::Alpha,0);auto* Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 auto* B=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 auto* PC=A?Cast<AVanthexPlayerController>(A->GetController()):nullptr;
 auto* AF=AVanthexFlag::Find(Mode->GetWorld(),EVanthexTeam::Alpha);auto* BF=AVanthexFlag::Find(Mode->GetWorld(),EVanthexTeam::Bravo);
 if(!A||!Ally||!B||!AF||!BF)return;
 switch(Step) {
 case 0: {
  Check(Mode->Combatants.Num()==10&&Mode->TeamSize()==5,TEXT("five players on both teams"));
  int32 ArchitectureParts=0,PavingInstances=0,SunCount=0;
  bool bShadowedAtmosphereSun=false,bRomanSky=false,bRomanSkyLight=false;
  for(TActorIterator<AActor> It(Mode->GetWorld());It;++It) {
   TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
   for(auto* Part:Parts) {
    if(Part->ComponentHasTag(TEXT("CTF_Roman_Architecture")))++ArchitectureParts;
    if(Part->ComponentHasTag(TEXT("RomanPaving")))
     if(const auto* Instances=Cast<UInstancedStaticMeshComponent>(Part))PavingInstances+=Instances->GetInstanceCount();
   }
   if(It->ActorHasTag(TEXT("CTF_Roman_Sun"))) {
    ++SunCount;
    if(const auto* Sun=Cast<ADirectionalLight>(*It))
     if(const auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
      bShadowedAtmosphereSun=Light->bAtmosphereSunLight&&Light->CastShadows;
   }
   bRomanSky|=It->ActorHasTag(TEXT("CTF_Roman_Sky"))&&It->IsA<ASkyAtmosphere>();
   bRomanSkyLight|=It->ActorHasTag(TEXT("CTF_Roman_SkyLight"))&&It->IsA<ASkyLight>();
  }
  Check(ArchitectureParts>0,TEXT("baked Roman architectural mesh tags are preserved"));
  Check(PavingInstances>1000,TEXT("Roman paving contains more than one thousand stone instances"));
  Check(SunCount==1&&bShadowedAtmosphereSun,TEXT("one Roman sun provides atmosphere disk and cast shadows"));
  Check(bRomanSky&&bRomanSkyLight,TEXT("Roman sky atmosphere and skylight are present"));
  Mode->ActivateRound();Check(FMath::IsNearlyEqual(Mode->TimeRemaining,900.f),TEXT("fifteen-minute regulation clock"));
  for(float X:{-1850.f,0.f,1850.f}) {
   TArray<FVector> Route;
   Check(VanthexNavigation::FindRoute(Mode->GetWorld(),FVector(X,-2400,100),FVector(X,2400,100),Route)&&Route.Num()>0,TEXT("centre and both flank routes are navigable"));
  }
  A->SetActorLocation(BF->Base()+FVector(0,0,100));
  Check(BF->TryTouch(A)&&BF->GetCarrier()==A,TEXT("enemy flag picked up by contact"));
  A->SetActorLocation(FVector(1800,0,100));
  FVanthexDamageRequest Hit;Hit.Damage=1000;Hit.DamageKind=EVanthexDamageKind::Melee;Hit.InstigatorActor=B;Hit.InstigatorController=B->GetController();A->GetHealthComponent()->ApplyDamage(Hit);
  BF->Tick(0);Check(!BF->GetCarrier()&&!BF->IsHome(),TEXT("carrier death drops flag on ground"));
  DiedAt=Mode->GetWorld()->GetTimeSeconds();
  Check(Mode->IsRoundActive()&&Mode->CTFRespawns.Contains(A),TEXT("death schedules respawn without ending match"));
  Ally->SetActorLocation(BF->GetActorLocation()+FVector(0,0,100));
  Check(BF->TryTouch(Ally)&&BF->GetCarrier()==Ally,TEXT("teammate takes over the dropped flag"));
  Ally->SetActorLocation(AF->Base()+FVector(0,0,100));BF->Tick(0);
  Check(Mode->AlphaScore==1&&BF->IsHome()&&Mode->IsRoundActive(),TEXT("capture adds point and continues match"));
  Check(Ally->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==1
   && A->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==0,
   TEXT("only the actual flag carrier receives the capture statistic"));
  const int32 RespawnSlot=A->GetPlayerState<AVanthexPlayerState>()->GetSpawnSlot();
  if(auto* AssignedStart=Mode->Arena->GetTeamSpawn(EVanthexTeam::Alpha,RespawnSlot))
   Ally->SetActorLocation(AssignedStart->GetActorLocation());
  else
   Check(false,TEXT("assigned team start exists for respawn occupancy test"));
  ++Step;return;
 }
 case 1:
  if(Mode->GetWorld()->GetTimeSeconds()-DiedAt<4)return;
  Check(A->GetHealthComponent()->IsDead(),TEXT("no early respawn before eight seconds"));
  Check(PC&&PC->GetSpectatedTeammate()==Ally&&PC->GetViewTarget()==Ally,
   TEXT("death automatically spectates the first living teammate"));
  if(PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Right,IE_Pressed,1.f));
  Step=4;return;
 case 4: {
  const auto* Next=Mode->FindCombatant(EVanthexTeam::Alpha,2);
  Check(PC&&Next&&PC->GetSpectatedTeammate()==Next&&PC->GetViewTarget()==Next,
   TEXT("right arrow changes camera to the next living teammate"));
  if(PC) {
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Right,IE_Released,0.f));
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Left,IE_Pressed,1.f));
  }
  Step=5;return;
 }
 case 5:
  Check(PC&&PC->GetSpectatedTeammate()==Ally&&PC->GetViewTarget()==Ally,
   TEXT("left arrow returns the camera to the previous teammate"));
  if(PC) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Left,IE_Released,0.f));
  Step=2;return;
 case 2:
  if(Mode->GetWorld()->GetTimeSeconds()-DiedAt<8.3f)return;
  Check(!A->GetHealthComponent()->IsDead()&&A->GetPlayerState<AVanthexPlayerState>()->IsAlive()&&A->GetActorLocation().Y<-3500,TEXT("automatic alive respawn at own base after eight seconds"));
  Check(PC&&!PC->GetSpectatedTeammate()&&PC->GetViewTarget()==A,TEXT("respawn returns the camera to the owned pawn"));
  Check(FVector::DistSquared2D(A->GetActorLocation(),Ally->GetActorLocation())>FMath::Square(84.0),
    TEXT("respawn avoids assigned start occupied by a live ally"));
  Check(A->GetPlayerState<AVanthexPlayerState>()->GetEconomy().PrimaryWeapon==TEXT("M4"),TEXT("respawn restores playable loadout"));
  Ally->SetActorLocation(FVector(1500,1000,100));
  B->SetActorLocation(AF->Base()+FVector(0,0,100));Check(AF->TryTouch(B),TEXT("opposing team can steal own flag"));
  B->SetActorLocation(FVector(-1800,0,100));AF->Drop();
  A->SetActorLocation(BF->Base()+FVector(0,0,100));BF->TryTouch(A);A->SetActorLocation(AF->Base()+FVector(0,0,100));BF->Tick(0);
  Check(Mode->AlphaScore==1&&A->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==0,
   TEXT("capture and individual credit blocked while own flag is away"));
  Ally->SetActorLocation(AF->GetActorLocation()+FVector(0,0,100));Check(AF->TryTouch(Ally)&&AF->IsHome(),TEXT("touching own dropped flag returns it"));
  Check(!AF->TryTouch(Ally)&&Ally->GetPlayerState<AVanthexPlayerState>()->GetFlagReturns()==1,
   TEXT("only the actual flag return counts, even if touched again"));
  BF->Tick(0);Check(Mode->AlphaScore==2&&A->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==1,
   TEXT("capture and individual credit succeed after own flag is returned"));
  Mode->AlphaScore=Mode->BravoScore=2;Mode->TimeRemaining=0;Mode->ResolveTimeout();
  Check(Mode->bCTFOvertime&&Mode->TimeRemaining==180&&Mode->IsRoundActive(),TEXT("tied regulation grants three-minute overtime"));
  Mode->AlphaScore=3;Mode->TimeRemaining=0;Mode->ResolveTimeout();
  Check(Mode->Phase==EVanthexRoundPhase::MatchEnded&&Mode->MatchWinner==EVanthexTeam::Alpha,TEXT("higher capture score wins after overtime"));
  Mode->Phase=EVanthexRoundPhase::RoundActive;
  B->SetActorLocation(AF->Base()+FVector(0,0,100));
  Check(AF->TryTouch(B)&&!AF->IsHome(),TEXT("a flag is away from home when overtime ends"));
  Mode->AlphaScore=Mode->BravoScore=3;Mode->ResolveTimeout();
  Check(Mode->Phase==EVanthexRoundPhase::MatchEnded&&Mode->MatchWinner==EVanthexTeam::None,TEXT("tied overtime ends in draw"));
  MatchEndedAt=Mode->GetWorld()->GetTimeSeconds();
  ++Step;
  return;
 case 3: {
  const float Elapsed=Mode->GetWorld()->GetTimeSeconds()-MatchEndedAt;
  if(Mode->Phase==EVanthexRoundPhase::MatchEnded&&Elapsed<12.f)return;
  Check(Elapsed>=9.5f&&Elapsed<12.f&&Mode->IsBuyPhase()&&Mode->RoundIndex==1,
    TEXT("automatic CTF rematch starts a new buy phase after ten seconds"));
  Check(Mode->AlphaScore==0&&Mode->BravoScore==0&&!Mode->bCTFOvertime&&Mode->MatchWinner==EVanthexTeam::None,
    TEXT("CTF rematch clears scores, overtime and match winner"));
  Check(AF->IsHome()&&BF->IsHome(),TEXT("CTF rematch returns both flags home"));
  Check(A->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==0
   && Ally->GetPlayerState<AVanthexPlayerState>()->GetFlagCaptures()==0
   && Ally->GetPlayerState<AVanthexPlayerState>()->GetFlagReturns()==0,
   TEXT("CTF rematch resets individual objective statistics"));
  Finish();
  return;
 }
 }
#endif
}
