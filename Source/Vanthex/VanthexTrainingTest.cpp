#include "VanthexTrainingMode.h"

#if !UE_BUILD_SHIPPING
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexTestTarget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void VanthexRunTrainingRangeTest(AVanthexTrainingMode* Mode)
{
 static const double Started = FPlatformTime::Seconds();
 static int32 Step = 0;
 static int32 Failures = 0;
 static TArray<FString> Results;
 static TWeakObjectPtr<AVanthexTestTarget> MovingTarget;
 static TWeakObjectPtr<AVanthexTestTarget> NearTarget;
 static FVector MovingStart;
 static bool bSawMovement = false;
 static double ShotAt = 0.0;
 if(Step == 3) return;

 const auto Check = [&](bool bOK, const TCHAR* Label)
 {
  Results.Add(FString(bOK ? TEXT("PASS ") : TEXT("FAIL ")) + Label);
  if(!bOK) ++Failures;
 };
 const auto Finish = [&]()
 {
  Step = 3;
  const FString Summary = FString::Printf(TEXT("OVERALL %s CHECKS %d FAILURES %d\n"),
   Failures ? TEXT("FAIL") : TEXT("PASS"), Results.Num(), Failures) + FString::Join(Results, TEXT("\n"));
  FFileHelper::SaveStringToFile(Summary, *(FPaths::ProjectSavedDir() / TEXT("TrainingRangeTest.txt")));
  FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
 };
 if(FPlatformTime::Seconds() - Started > 40.0)
 {
  Check(false, TEXT("training range scenario completes within forty seconds"));
  Finish();
  return;
 }

 auto* Pawn = Mode->GetTestCharacter();
 if(!Pawn || !Mode->IsRoundActive()) return;
 UWorld* World = Mode->GetWorld();
 if(Step == 0)
 {
  Check(Mode->Combatants.Num() == 1 && Pawn->IsPlayerControlled(), TEXT("solo start without bots"));
  TArray<double> Distances;
  double Nearest = TNumericLimits<double>::Max();
  for(TActorIterator<AVanthexTestTarget> It(World); It; ++It)
  {
   if(It->ActorHasTag(TEXT("TrainingMovingTarget")))
   {
    MovingTarget = *It;
    MovingStart = It->GetActorLocation();
    continue;
   }
   const double Distance = FVector::Dist2D(Pawn->GetActorLocation(), It->GetActorLocation());
   Distances.Add(Distance);
   if(Distance < Nearest)
   {
    Nearest = Distance;
    NearTarget = *It;
   }
  }
  Distances.Sort();
  Check(Distances.Num() == 3 && Distances[0] > 900.0 && Distances[0] < 1600.0
   && Distances[1] > 2300.0 && Distances[1] < 3100.0 && Distances[2] > 4900.0
   && MovingTarget.IsValid(), TEXT("three shooting distances and a separate moving target"));

  auto* Dummy = NearTarget.Get();
  auto* Weapon = Pawn->GetWeapon();
  if(!Dummy || !Weapon || !Pawn->GetController())
  {
   Check(false, TEXT("training firing fixture is available"));
   Finish();
   return;
  }
  FVector FiringPosition = Pawn->GetActorLocation();
  FiringPosition.X = Dummy->GetActorLocation().X;
  Pawn->SetActorLocation(FiringPosition);
  Pawn->GetController()->SetControlRotation((Dummy->GetBodyCenter() - Pawn->GetPawnViewLocation()).Rotation());
  const float InitialHealth = Dummy->GetHealthComponent()->GetHealth();
  Weapon->FireShot();
  Check(Dummy->GetHealthComponent()->GetHealth() < InitialHealth, TEXT("normal hitscan damages a range target"));
  FVanthexDamageRequest FatalHit;
  FatalHit.Damage = 1000.f;
  Dummy->GetHealthComponent()->ApplyDamage(FatalHit);
  ShotAt = World->GetTimeSeconds();
  ++Step;
  return;
 }

 if(Step == 1)
 {
  if(const auto* Moving = MovingTarget.Get())
   bSawMovement |= FVector::DistSquared2D(Moving->GetActorLocation(), MovingStart) > FMath::Square(25.0);
  if(World->GetTimeSeconds() - ShotAt < 1.5) return;
  Check(bSawMovement, TEXT("moving target traverses its lane"));
  const auto* Dummy = NearTarget.Get();
  Check(Dummy && !Dummy->GetHealthComponent()->IsDead() && Dummy->IsCombatCollisionEnabled(),
   TEXT("destroyed dummy resets for another shot"));

  auto* Core = Mode->GetCore();
  const auto* Zone = Mode->Arena->GetExtractionZone(EVanthexExtractionId::A);
  if(!Core || !Zone)
  {
   Check(false, TEXT("core and extraction pad are available"));
   Finish();
   return;
  }
  Pawn->SetActorLocation(Core->GetActorLocation() + FVector(0, 0, 50));
  Pawn->TryPickupCore();
  if(Core->GetCarrier() != Pawn)
  {
   Check(false, TEXT("player can pick up the training core"));
   Finish();
   return;
  }
  Pawn->SetActorLocation(Zone->GetActorLocation() + FVector(0, 0, 100));
  ++Step;
  return;
 }

 if(Mode->AlphaScore < 1) return;
 Check(Mode->GetCore()->IsAtHome(), TEXT("completed extraction resets the core for another drill"));
 Finish();
}
#endif
