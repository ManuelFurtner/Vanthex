#include "VanthexGameMode.h"
#if !UE_BUILD_SHIPPING
#include "VanthexFlag.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexBotController.h"
#include "VanthexNavigation.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#endif
void VanthexRunCTFTest(AVanthexGameMode* Mode)
{
#if !UE_BUILD_SHIPPING
 if(!Mode||!Mode->HasAuthority())return;
 static int32 Step=0,Failures=0;static float DiedAt=0;static TArray<FString> Lines;
 if(Step==100)return;
 auto Check=[&](bool OK,const TCHAR* What){Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+What);if(!OK)++Failures;};
 auto* A=Mode->FindCombatant(EVanthexTeam::Alpha,0);auto* Ally=Mode->FindCombatant(EVanthexTeam::Alpha,1);
 auto* B=Mode->FindCombatant(EVanthexTeam::Bravo,0);
 auto* AF=AVanthexFlag::Find(Mode->GetWorld(),EVanthexTeam::Alpha);auto* BF=AVanthexFlag::Find(Mode->GetWorld(),EVanthexTeam::Bravo);
 if(!A||!Ally||!B||!AF||!BF)return;
 switch(Step) {
 case 0: {
  Check(Mode->Combatants.Num()==10&&Mode->TeamSize()==5,TEXT("five players on both teams"));
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
  ++Step;return;
 }
 case 1:
  if(Mode->GetWorld()->GetTimeSeconds()-DiedAt<4)return;
  Check(A->GetHealthComponent()->IsDead(),TEXT("no early respawn before eight seconds"));++Step;return;
 case 2:
  if(Mode->GetWorld()->GetTimeSeconds()-DiedAt<8.3f)return;
  Check(!A->GetHealthComponent()->IsDead()&&A->GetPlayerState<AVanthexPlayerState>()->IsAlive()&&A->GetActorLocation().Y<-3500,TEXT("automatic alive respawn at own base after eight seconds"));
  Check(A->GetPlayerState<AVanthexPlayerState>()->GetEconomy().PrimaryWeapon==TEXT("M4"),TEXT("respawn restores playable loadout"));
  Ally->SetActorLocation(FVector(1500,1000,100));
  B->SetActorLocation(AF->Base()+FVector(0,0,100));Check(AF->TryTouch(B),TEXT("opposing team can steal own flag"));
  B->SetActorLocation(FVector(-1800,0,100));AF->Drop();
  A->SetActorLocation(BF->Base()+FVector(0,0,100));BF->TryTouch(A);A->SetActorLocation(AF->Base()+FVector(0,0,100));BF->Tick(0);
  Check(Mode->AlphaScore==1,TEXT("capture blocked while own flag is away"));
  Ally->SetActorLocation(AF->GetActorLocation()+FVector(0,0,100));Check(AF->TryTouch(Ally)&&AF->IsHome(),TEXT("touching own dropped flag returns it"));
  BF->Tick(0);Check(Mode->AlphaScore==2,TEXT("capture succeeds after own flag is returned"));
  Mode->AlphaScore=Mode->BravoScore=2;Mode->TimeRemaining=0;Mode->ResolveTimeout();
  Check(Mode->bCTFOvertime&&Mode->TimeRemaining==180&&Mode->IsRoundActive(),TEXT("tied regulation grants three-minute overtime"));
  Mode->AlphaScore=3;Mode->TimeRemaining=0;Mode->ResolveTimeout();
  Check(Mode->Phase==EVanthexRoundPhase::MatchEnded&&Mode->MatchWinner==EVanthexTeam::Alpha,TEXT("higher capture score wins after overtime"));
  Mode->Phase=EVanthexRoundPhase::RoundActive;Mode->AlphaScore=Mode->BravoScore=3;Mode->ResolveTimeout();
  Check(Mode->Phase==EVanthexRoundPhase::MatchEnded&&Mode->MatchWinner==EVanthexTeam::None,TEXT("tied overtime ends in draw"));
  Step=100;
  const FString Report=FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),Failures?TEXT("FAIL"):TEXT("PASS"),Lines.Num(),Failures)+FString::Join(Lines,TEXT("\n"));
  FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("CTFTest.txt")));
  FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);return;
 }
#endif
}
