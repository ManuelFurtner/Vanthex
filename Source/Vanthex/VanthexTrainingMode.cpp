#include "VanthexTrainingMode.h"

#include "HealthComponent.h"
#include "VanthexCharacter.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexPlayerState.h"
#include "VanthexTestTarget.h"
#include "Components/TextRenderComponent.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

namespace
{
constexpr double FiringLineY = -3000.0;
constexpr double MovingLaneX = 1400.0;
constexpr double MovingLaneY = FiringLineY + 2500.0;
}

AVanthexTrainingMode::AVanthexTrainingMode()
{
 RoundDuration = 3600.f;
 BuyDuration = .1f;
 IntermissionDuration = .5f;
}

void AVanthexTrainingMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
 AGameModeBase::InitGame(MapName, Options, ErrorMessage);
 if(auto* Range = GetWorld()->SpawnActor<AVanthexTrainingArena>())
 {
  Range->BuildRange();
  Arena = Range;
  Core = Range->GetCore();
 }
}

void AVanthexTrainingMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
 if(!NewPlayer || !NewPlayer->IsLocalController() || bMatchStarted) return;
 auto* State = NewPlayer->GetPlayerState<AVanthexPlayerState>();
 if(!State || !Arena) return;

 State->SetTeam(EVanthexTeam::Alpha, 0);
 AGameModeBase::HandleStartingNewPlayer_Implementation(NewPlayer);
 if(auto* Pawn = Cast<AVanthexCharacter>(NewPlayer->GetPawn()))
 {
  RegisterCombatant(Pawn);
  bMatchStarted = true;
  RestartMatch();
 }
}

void AVanthexTrainingMode::BeginPlay()
{
 Super::BeginPlay();
#if !UE_BUILD_SHIPPING
 if(FParse::Param(FCommandLine::Get(), TEXT("TrainingRangeTest")))
 {
  FTimerHandle Timer;
  GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
  {
   extern void VanthexRunTrainingRangeTest(AVanthexTrainingMode*);
   VanthexRunTrainingRangeTest(this);
  }), .1f, true, 1.f);
 }
#endif
}

void AVanthexTrainingArena::BuildRange()
{
 UWorld* World = GetWorld();
 CubeMesh = VanthexArt::LoadCube();
 if(!World || !CubeMesh) return;

 const FLinearColor Floor(.15f, .17f, .19f);
 const FLinearColor Wall(.21f, .24f, .28f);
 const FLinearColor Marker(.27f, .8f, .79f);
 SpawnBlock(FVector(0, 0, -10), FVector(60, 90, .2), Floor, .82f, .08f, EVanthexSurface::Floor);
 for(const double Side : {-1.0, 1.0})
 {
  SpawnBlock(FVector(Side * 3000, 0, 185), FVector(.5, 90, 3.7), Wall, .85f, .08f, EVanthexSurface::Concrete);
  SpawnBlock(FVector(0, Side * 4500, 185), FVector(60, .5, 3.7), Wall, .85f, .08f, EVanthexSurface::Concrete);
 }

 // One wall separates the objective lane from the shooting stations.
 SpawnBlock(FVector(-1550, 900, 160), FVector(.55, 66, 3.2), Wall, .85f, .08f, EVanthexSurface::Concrete);
 VanthexArt::AddMesh(this, SceneRoot, CubeMesh, FVector(500, FiringLineY, 1),
  FRotator::ZeroRotator, FVector(38, .1, .02), VanthexArt::MakeSurface(this, Marker, .7f, .05f), false);

 const auto AddSign = [World](const FVector& Position, const TCHAR* Label)
 {
  if(auto* Sign = World->SpawnActor<ATextRenderActor>(Position, FRotator(0, -90, 0)))
  {
   auto* Text = Sign->GetTextRender();
   Text->SetText(FText::FromString(Label));
   Text->SetHorizontalAlignment(EHTA_Center);
   Text->SetWorldSize(45.f);
   Text->SetTextRenderColor(FColor(220, 245, 246));
  }
 };

 AddTarget(FVector(-900, FiringLineY + 1000, 0));
 AddTarget(FVector(-250, FiringLineY + 2500, 0));
 AddTarget(FVector(450, FiringLineY + 5000, 0));
 MovingTarget = AddTarget(FVector(MovingLaneX, MovingLaneY, 0));
 if(MovingTarget) MovingTarget->Tags.Add(TEXT("TrainingMovingTarget"));

 const double SignY = FiringLineY + 280;
 AddSign(FVector(-900, SignY, 260), TEXT("10 m"));
 AddSign(FVector(-250, SignY, 260), TEXT("25 m"));
 AddSign(FVector(450, SignY, 260), TEXT("50 m"));
 AddSign(FVector(MovingLaneX, SignY, 260), TEXT("BEWEGT  25 m"));
 AddSign(FVector(-2200, SignY, 260), TEXT("CORE-UEBUNG\nCORE ZUR EXTRAKTION A BRINGEN"));
 AddSign(FVector(-250, -3820, 260), TEXT("F5 UEBUNG NEU STARTEN"));

 if(auto* Start = World->SpawnActor<APlayerStart>(FVector(-250, FiringLineY - 180, 100), FRotator(0, 90, 0)))
  AlphaSpawns.Add(Start);

 const FVector CoreHome(-2200, -1800, 50);
 Core = World->SpawnActor<AVanthexCore>(CoreHome, FRotator::ZeroRotator);
 if(Core) Core->SetHome(CoreHome);

 ExtractionA = World->SpawnActor<AVanthexExtractionZone>(FVector(-2200, 1200, 20), FRotator::ZeroRotator);
 if(ExtractionA) ExtractionA->Setup(EVanthexExtractionId::A, FVector(320, 410, 220));
 AddSign(FVector(-2200, 1620, 255), TEXT("EXTRAKTION A  /  15 s"));
 BuildAtmosphere();
}

AVanthexTestTarget* AVanthexTrainingArena::AddTarget(const FVector& Position)
{
 auto* Dummy = GetWorld()->SpawnActor<AVanthexTestTarget>(Position, FRotator::ZeroRotator);
 if(Dummy)
  Dummy->GetHealthComponent()->OnDeath.AddUObject(this, &AVanthexTrainingArena::RespawnTarget);
 return Dummy;
}

void AVanthexTrainingArena::RespawnTarget(AActor* TargetActor)
{
 const TWeakObjectPtr<AVanthexTestTarget> Dummy = Cast<AVanthexTestTarget>(TargetActor);
 FTimerHandle Timer;
 GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [Dummy]()
 {
  if(auto* Target = Dummy.Get()) Target->ResetForRound();
 }), 1.25f, false);
}

void AVanthexTrainingArena::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(MovingTarget)
 {
  const double PositionX = MovingLaneX + 380.0 * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.15);
  MovingTarget->SetActorLocation(FVector(PositionX, MovingLaneY, 0));
 }
}
