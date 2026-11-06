#include "VanthexGameMode.h"
#include "VanthexMapLayout.h"
#include "VanthexFlag.h"
#include "VanthexGadgetActor.h"
#include "VanthexGadgetComponent.h"

#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexCore.h"
#include "VanthexShiftDevice.h"
#include "VanthexExtractionZone.h"
#include "VanthexGameState.h"
#include "VanthexHUD.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexTestArena.h"
#include "VanthexTestTarget.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "UnrealClient.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

AVanthexGameMode::AVanthexGameMode()
{
	DefaultPawnClass = AVanthexCharacter::StaticClass();
	PlayerControllerClass = AVanthexPlayerController::StaticClass();
	PlayerStateClass = AVanthexPlayerState::StaticClass();
	GameStateClass = AVanthexGameState::StaticClass();
	HUDClass = AVanthexHUD::StaticClass();
}

void AVanthexGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
 if(IsCTF()) RoundDuration=900.f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Name = TEXT("VanthexArena");
 Arena = World->SpawnActor<AVanthexTestArena>(AVanthexTestArena::StaticClass(), FTransform::Identity, Params);
	if (Arena)
	{
		Arena->Build();
		TestTarget = Arena->GetTarget();
		Core = Arena->GetCore();
	}
}

AActor* AVanthexGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AVanthexPlayerState* PlayerState = Player ? Player->GetPlayerState<AVanthexPlayerState>() : nullptr;
	if (Arena && PlayerState)
	{
		if (APlayerStart* Start = Arena->GetTeamSpawn(PlayerState->GetTeam(), PlayerState->GetSpawnSlot()))
		{
			return Start;
		}
	}
	if (Arena)
	{
		if (APlayerStart* Start = Arena->GetPlayerStart())
		{
			return Start;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AVanthexGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (AVanthexPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AVanthexPlayerState>() : nullptr)
	{
		if (PlayerState->GetTeam() == EVanthexTeam::None)
		{
			EVanthexTeam SelectedTeam = EVanthexTeam::Alpha;
            int32 SelectedSlot = 0;
            if (bRosterReady) {
                AVanthexCharacter* Replacement = nullptr;
                for (int32 Slot = 0; Slot < TeamSize() && !Replacement; ++Slot) {
                    for (EVanthexTeam Team : {EVanthexTeam::Bravo, EVanthexTeam::Alpha}) {
                        auto* Candidate = FindCombatant(Team, Slot);
                        if (Candidate && Cast<AVanthexBotController>(Candidate->GetController())) { Replacement = Candidate; SelectedTeam = Team; SelectedSlot = Slot; break; }
                    }
                }
                if (!Replacement) { NewPlayer->StartSpectatingOnly(); return; }
                auto* Bot = Replacement->GetController();
                PlayerState->CopyEconomyFrom(Bot->GetPlayerState<AVanthexPlayerState>());
                Bot->UnPossess();
                PlayerState->SetTeam(SelectedTeam, SelectedSlot);
                NewPlayer->Possess(Replacement);
                PlayerState->SetAlive(!Replacement->GetHealthComponent()->IsDead());
                // Remote possession dispatches Character::Restart, restoring Walking.
                // Reapply the authoritative phase/death lock after that engine callback.
                if (IsBuyPhase() || Replacement->GetHealthComponent()->IsDead()) {
                    Replacement->GetCharacterMovement()->StopMovementImmediately();
                    Replacement->GetCharacterMovement()->DisableMovement();
                }
                Bot->Destroy();
                if (FParse::Param(FCommandLine::Get(), TEXT("VanthexNetworkProbe"))) { FVanthexDamageRequest Damage; Damage.Damage=40; Replacement->GetHealthComponent()->ApplyDamage(Damage); }
                return;
            }
            PlayerState->SetTeam(SelectedTeam, SelectedSlot);
		}
	}

	EnsureRoster();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (AVanthexCharacter* Pawn = NewPlayer ? Cast<AVanthexCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		RegisterCombatant(Pawn);
	}

	if (!bMatchStarted)
	{
		bMatchStarted = true;
		RestartMatch();
	}
}

void AVanthexGameMode::RestartTest()
{
	if (bRestartPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// F5 is delivered from the pawn input stack. Destroying that pawn in the callback
	// would free the component that is still dispatching the key, so the reset waits a tick.
	bRestartPending = true;
	World->GetTimerManager().SetTimerForNextTick([this]()
	{
		PerformSessionReset();
	});
}

void AVanthexGameMode::PerformSessionReset()
{
	bRestartPending = false;
	if (!GetWorld())
	{
		return;
	}

	bMatchStarted = true;
	RestartMatch();
	UE_LOG(LogVanthex, Display, TEXT("Gameplay session reset"));
}

void AVanthexGameMode::BeginPlay()
{
 if(FParse::Param(FCommandLine::Get(),TEXT("CTFTest"))) { FTimerHandle Test; GetWorldTimerManager().SetTimer(Test,FTimerDelegate::CreateWeakLambda(this,[this](){extern void VanthexRunCTFTest(AVanthexGameMode*); VanthexRunCTFTest(this);}),.1f,true,2.f); }
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
 if (FParse::Param(FCommandLine::Get(), TEXT("RoundCycleTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { extern void VanthexRunRoundCycleTest(AVanthexGameMode*); VanthexRunRoundCycleTest(this); }), .05f, true, 1.f);
 }
#endif
 if(FParse::Param(FCommandLine::Get(),TEXT("PoseHitTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunPoseHitTest(AVanthexGameMode*); VanthexRunPoseHitTest(this); }),.05f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("ExtractionRulesTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunExtractionRulesTest(AVanthexGameMode*); VanthexRunExtractionRulesTest(this); }),.05f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("LooseCoreExtractionTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunLooseCoreExtractionTest(AVanthexGameMode*); VanthexRunLooseCoreExtractionTest(this); }),.05f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("ControlsTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunControlsTest(AVanthexGameMode*); VanthexRunControlsTest(this); }),.05f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("WeaponProfileTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunWeaponProfileTest(AVanthexGameMode*); VanthexRunWeaponProfileTest(this); }),.05f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("GadgetTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunGadgetTest(AVanthexGameMode*); VanthexRunGadgetTest(this); }),.05f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("WeaponModelShots"))) {
  GetWorldTimerManager().SetTimer(VisualShotTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunWeaponModelShots(AVanthexGameMode*); VanthexRunWeaponModelShots(this); }),.1f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("GadgetShots"))) {
  GetWorldTimerManager().SetTimer(VisualShotTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunGadgetShots(AVanthexGameMode*); VanthexRunGadgetShots(this); }),.05f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("AimTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunAimTest(AVanthexGameMode*); VanthexRunAimTest(this); }),.05f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("AimShots"))||FParse::Param(FCommandLine::Get(),TEXT("BodyViewShots"))||FParse::Param(FCommandLine::Get(),TEXT("HUDShots"))||FParse::Param(FCommandLine::Get(),TEXT("CoreShots"))) {
  GetWorldTimerManager().SetTimer(VisualShotTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunAimShots(AVanthexGameMode*); VanthexRunAimShots(this); }),.05f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("EconomyShots"))) {
  GetWorldTimerManager().SetTimer(VisualShotTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunEconomyShots(AVanthexGameMode*); VanthexRunEconomyShots(this); }),.1f,true,1.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("CoreEconomyTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunCoreEconomyTest(AVanthexGameMode*); VanthexRunCoreEconomyTest(this); }),.05f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("EconomyTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunEconomyIntegrationTest(AVanthexGameMode*); VanthexRunEconomyIntegrationTest(this); }),.1f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("OperatorTest"))) {
  GetWorldTimerManager().SetTimer(SelfTestTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunOperatorTest(AVanthexGameMode*); VanthexRunOperatorTest(this); }),.1f,true,2.f);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("OperatorShots"))) {
  GetWorldTimerManager().SetTimer(VisualShotTimer,FTimerDelegate::CreateWeakLambda(this,[this]() { extern void VanthexRunOperatorShots(AVanthexGameMode*); VanthexRunOperatorShots(this); }),.1f,true,3.f);
 }
 if (FParse::Param(FCommandLine::Get(),TEXT("VanthexMapShots"))) GetWorldTimerManager().SetTimer(VisualShotTimer,this,&AVanthexGameMode::AdvanceMapShots,4.f,true,8.f);
 if (FParse::Param(FCommandLine::Get(),TEXT("GridV4Shots"))) GetWorldTimerManager().SetTimer(VisualShotTimer,this,&AVanthexGameMode::AdvanceGridV4Shots,4.f,true,12.f);
 if (FParse::Param(FCommandLine::Get(),TEXT("GridV4GameTest"))) GetWorldTimerManager().SetTimer(SelfTestTimer,this,&AVanthexGameMode::AdvanceGridV4GameTest,0.05f,true,3.f);
 if (FParse::Param(FCommandLine::Get(),TEXT("GridV3Shots"))) GetWorldTimerManager().SetTimer(VisualShotTimer,this,&AVanthexGameMode::AdvanceGridV3Shots,4.f,true,12.f);
 if (FParse::Param(FCommandLine::Get(),TEXT("GridV3GameTest"))) GetWorldTimerManager().SetTimer(SelfTestTimer,this,&AVanthexGameMode::AdvanceGridV3GameTest,0.05f,true,3.f);
	UE_LOG(LogVanthex, Display, TEXT("VANTHEX M2 ready"));

	if (FParse::Param(FCommandLine::Get(), TEXT("VanthexSelfTest")))
	{
		bSelfTest = true;
		SelfTestStage = EVanthexSelfTestStage::WaitForWorld;
		GetWorldTimerManager().SetTimer(SelfTestTimer, this, &AVanthexGameMode::AdvanceSelfTest, 0.05f, true);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("VanthexVisualShots")))
	{
		GetWorldTimerManager().SetTimer(VisualShotTimer, this, &AVanthexGameMode::AdvanceVisualShots, 0.45f, true, 6.f);
	}
}

void AVanthexGameMode::AdvanceVisualShots()
{
	struct FShot
	{
		FVector Location;
		FRotator Rotation;
		const TCHAR* Name;
		int32 Action;
	};

	static const FShot Shots[] = {
		{FVector(0.f, -1400.f, 160.f), FRotator(0.f, 90.f, 0.f), TEXT("01_alpha_spawn"), 0},
		{FVector(0.f, -980.f, 155.f), FRotator(-2.f, 90.f, 0.f), TEXT("02_alpha_approach"), 0},
		{FVector(-80.f, -420.f, 150.f), FRotator(-6.f, 70.f, 0.f), TEXT("03_central_chamber"), 0},
		{FVector(40.f, -280.f, 130.f), FRotator(-6.f, 78.f, 0.f), TEXT("04_core_medium"), 0},
		{FVector(40.f, -150.f, 90.f), FRotator(-12.f, 72.f, 0.f), TEXT("05_core_close"), 0},
		{FVector(-620.f, -250.f, 150.f), FRotator(-4.f, 90.f, 0.f), TEXT("06_left_route"), 0},
		{FVector(680.f, 180.f, 150.f), FRotator(-4.f, -90.f, 0.f), TEXT("07_right_route"), 0},
		{FVector(-1280.f, -160.f, 370.f), FRotator(-8.f, 78.f, 0.f), TEXT("08_catwalk"), 0},
		{FVector(-480.f, 420.f, 165.f), FRotator(-12.f, 125.f, 0.f), TEXT("09_extraction_a"), 0},
		{FVector(430.f, -280.f, 165.f), FRotator(-12.f, -52.f, 0.f), TEXT("10_extraction_b"), 0},
		{FVector(0.f, 1650.f, 155.f), FRotator(0.f, -90.f, 0.f), TEXT("11_bravo_approach"), 0},
		{FVector(80.f, -1100.f, 155.f), FRotator(-2.f, 72.f, 0.f), TEXT("12_rifle_firing"), 4},
		{FVector(280.f, -720.f, 145.f), FRotator(-2.f, -90.f, 0.f), TEXT("13_impact_concrete"), 5},
		{FVector(-500.f, 520.f, 120.f), FRotator(-6.f, -90.f, 0.f), TEXT("14_impact_metal"), 6},
		{FVector(220.f, 80.f, 140.f), FRotator(-4.f, 90.f, 0.f), TEXT("15_core_thrown"), 1},
		{FVector(-480.f, 480.f, 160.f), FRotator(-8.f, 118.f, 0.f), TEXT("16_extraction_mid"), 2},
		{FVector(-420.f, 560.f, 155.f), FRotator(-10.f, 130.f, 0.f), TEXT("17_extraction_late"), 3},
		{FVector(0.f, -900.f, 155.f), FRotator(0.f, 90.f, 0.f), TEXT("18_combat"), 0}
	};

	UWorld* World = GetWorld();
	AVanthexCharacter* Human = GetTestCharacter();
	if (!World || !Human)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Human->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
	}

	if (VisualShotIndex >= UE_ARRAY_COUNT(Shots))
	{
		if (!FScreenshotRequest::IsScreenshotRequested())
		{
			FPlatformMisc::RequestExitWithStatus(false, 0);
		}
		return;
	}

	const FShot& Shot = Shots[VisualShotIndex];
	const uint32 Bit = 1u << VisualShotIndex;
	const bool bFirstAction = Shot.Action != 0 && (VisualActionMask & Bit) == 0;
	if (bFirstAction)
	{
		VisualActionMask |= Bit;
		if (Shot.Action == 1 && Core)
		{
			Human->SetActorLocation(Core->GetActorLocation() + FVector(0.f, -80.f, 40.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* Controller = Human->GetController())
			{
				Controller->SetControlRotation(FRotator(8.f, 90.f, 0.f));
			}
			Core->TryPickup(Human);
			Core->Throw();
			return;
		}
		if ((Shot.Action == 2 || Shot.Action == 3) && Core && Arena)
		{
			Human->SetActorLocation(Core->GetActorLocation() + FVector(0.f, -60.f, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
			Core->TryPickup(Human);
			Human->SetActorLocation(FVector(-850.f, 850.f, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
			Core->SetExtracting(true);
			const float Charge = Shot.Action == 2 ? 0.5f : 0.92f;
			ExtractionElapsed = Charge * ExtractionDuration;
			ActiveExtraction = EVanthexExtractionId::A;
			if (AVanthexExtractionZone* ZoneA = Arena->GetExtractionZone(EVanthexExtractionId::A))
			{
				ZoneA->SetCharge(Charge);
				ZoneA->HoldCharge(2.f);
			}
			return;
		}
	}

	Human->SetActorLocation(Shot.Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* Controller = Human->GetController())
	{
		Controller->SetControlRotation(Shot.Rotation);
	}

	if (bFirstAction && Shot.Action >= 4)
	{
		if (AHitscanWeapon* Weapon = Human->GetWeapon())
		{
			Weapon->FireShot();
		}
	}

	const FString Directory = FPaths::ProjectSavedDir() / TEXT("VanthexShots");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FScreenshotRequest::RequestScreenshot(Directory / Shot.Name, false, false);
	UE_LOG(LogVanthex, Display, TEXT("Visual shot %s"), Shot.Name);
	++VisualShotIndex;
}

void AVanthexGameMode::EnsureRoster()
{
	if (bRosterReady || !Arena || !GetWorld())
	{
		return;
	}

 for(int32 Slot=0;Slot<TeamSize();++Slot) {
  if(Slot>0) SpawnBot(EVanthexTeam::Alpha,Slot);
  SpawnBot(EVanthexTeam::Bravo,Slot);
 }
	bRosterReady = true;
}

void AVanthexGameMode::SpawnBot(EVanthexTeam Team, int32 Slot)
{
	UWorld* World = GetWorld();
	APlayerStart* Start = Arena ? Arena->GetTeamSpawn(Team, Slot) : nullptr;
	if (!World || !Start)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVanthexBotController* Bot = World->SpawnActor<AVanthexBotController>(AVanthexBotController::StaticClass(), Start->GetActorTransform(), Params);
	if (!Bot)
	{
		return;
	}

	Bot->InitPlayerState();
	if (AVanthexPlayerState* PlayerState = Bot->GetPlayerState<AVanthexPlayerState>())
	{
		PlayerState->SetTeam(Team, Slot);
		PlayerState->SetPlayerName(FString::Printf(TEXT("%s %d"), Team == EVanthexTeam::Bravo ? TEXT("Bravo") : TEXT("Alpha"), Slot + 1));
	}

	if (APawn* Pawn = SpawnDefaultPawnFor(Bot, Start))
	{
		Bot->Possess(Pawn);
		Bot->SetControlRotation(Start->GetActorRotation());
		RegisterCombatant(Cast<AVanthexCharacter>(Pawn));
	}
}

void AVanthexGameMode::RegisterCombatant(AVanthexCharacter* Combatant)
{
	if (!Combatant || Combatants.Contains(Combatant))
	{
		return;
	}

	Combatants.Add(Combatant);
	if (const AVanthexPlayerState* PlayerState = GetVanthexPlayerState(Combatant))
	{
		Combatant->SetTeamAppearance(PlayerState->GetTeam());
	}
}

AVanthexPlayerState* AVanthexGameMode::GetVanthexPlayerState(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	const AController* Controller = Pawn ? Pawn->GetController() : Cast<AController>(Actor);
	return Controller ? Controller->GetPlayerState<AVanthexPlayerState>() : nullptr;
}

bool AVanthexGameMode::CanDamage(const AActor* InstigatorActor, const AActor* TargetActor) const
{
	if (!InstigatorActor || !TargetActor || InstigatorActor == TargetActor)
	{
		return false;
	}

	const AVanthexPlayerState* InstigatorState = GetVanthexPlayerState(InstigatorActor);
	const AVanthexPlayerState* TargetState = GetVanthexPlayerState(TargetActor);
	if (!InstigatorState || !TargetState)
	{
		return true;
	}
	if (!bAllowFriendlyFire && InstigatorState->GetTeam() != EVanthexTeam::None && InstigatorState->GetTeam() == TargetState->GetTeam())
	{
		return false;
	}
	return true;
}

void AVanthexGameMode::NotifyCombatantDied(AVanthexCharacter* Combatant)
{
	if (Core && Combatant && Core->GetCarrier() == Combatant)
	{
		Core->DropFromDeath();
		RefreshCoreReadout();
	}

	if (!Combatant || Phase != EVanthexRoundPhase::RoundActive)
	{
		return;
	}

	if (AVanthexPlayerState* PlayerState = GetVanthexPlayerState(Combatant))
	{
		PlayerState->SetAlive(false);
		PlayerState->HandleEconomyDeath();
	}
 if(IsCTF()) {
  CTFRespawns.Add(Combatant,GetWorld()->GetTimeSeconds()+8.f);
  if(auto* State=GetVanthexPlayerState(Combatant)) State->SetRespawnTime(GetGameState<AVanthexGameState>()->GetServerWorldTimeSeconds()+8.f);
  return;
 }
	EvaluateElimination();
}

bool AVanthexGameMode::HasDepositedCore() const
{
	if (!Core || !Arena || Core->GetCarrier() || Core->GetExtractionTeam() == EVanthexTeam::None
		|| Core->GetCoreState() == EVanthexCoreState::Extracted) return false;
	for (const auto Id : {EVanthexExtractionId::A, EVanthexExtractionId::B})
		if (const auto* Zone = Arena->GetExtractionZone(Id); Zone && Zone->Contains(Core->GetActorLocation())) return true;
	return false;
}

bool AVanthexGameMode::HasPendingDepositedExtraction()
{
	if (Phase == EVanthexRoundPhase::RoundActive && Core && Arena
		&& Core->GetExtractionTeam() != EVanthexTeam::None && Core->GetCoreState() != EVanthexCoreState::Extracted)
	{
		for (const auto Id : {EVanthexExtractionId::A, EVanthexExtractionId::B})
			if (const auto* Zone = Arena->GetExtractionZone(Id); Zone && Zone->Contains(Core->GetActorLocation()))
			{
				if (!Core->GetCarrier()) { DepositedZone=Id; DepositedPossessionRevision=Core->GetPossessionRevision(); return true; }
				if (DepositedZone==Id && DepositedPossessionRevision==Core->GetPossessionRevision()) return true;
			}
	}
	DepositedZone=EVanthexExtractionId::None;
	DepositedPossessionRevision=0;
	return false;
}

void AVanthexGameMode::EvaluateElimination()
{
	if (Phase != EVanthexRoundPhase::RoundActive)
	{
		return;
	}

	const int32 AlphaAlive = CountAlive(EVanthexTeam::Alpha);
	const int32 BravoAlive = CountAlive(EVanthexTeam::Bravo);
	const bool bAlphaOut = AlphaAlive == 0;
	const bool bBravoOut = BravoAlive == 0;
	// Inspect the real Core position, including a death drop before the next clock tick.
	// A deposited objective gets its full countdown even when nobody remains alive.
	bEliminationDeferred = (bAlphaOut || bBravoOut) && HasPendingDepositedExtraction();
	if (bEliminationDeferred) return;
	if (bAlphaOut && bBravoOut)
	{
		EndRound(EVanthexTeam::None);
	}
	else if (bBravoOut)
	{
		EndRound(EVanthexTeam::Alpha);
	}
	else if (bAlphaOut)
	{
		EndRound(EVanthexTeam::Bravo);
	}
}

int32 AVanthexGameMode::CountAlive(EVanthexTeam Team) const
{
	int32 Count = 0;
	for (const AVanthexCharacter* Combatant : Combatants)
	{
		const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
		const UHealthComponent* Health = Combatant ? Combatant->GetHealthComponent() : nullptr;
		if (PlayerState && PlayerState->GetTeam() == Team && Health && !Health->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

AVanthexCharacter* AVanthexGameMode::FindCombatant(EVanthexTeam Team, int32 Slot) const
{
	for (AVanthexCharacter* Combatant : Combatants)
	{
		const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
		if (PlayerState && PlayerState->GetTeam() == Team && PlayerState->GetSpawnSlot() == Slot)
		{
			return Combatant;
		}
	}
	return nullptr;
}

void AVanthexGameMode::PublishMatch()
{
	if (AVanthexGameState* Match = GetGameState<AVanthexGameState>())
	{
		Match->SetCTFOvertime(bCTFOvertime);
		Match->PublishRound(Phase, AlphaScore, BravoScore, RoundIndex, TimeRemaining, RoundWinner, MatchWinner);
	}
}

void AVanthexGameMode::RestartMatch()
{
	ResetPlaytestReport();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlowTimer);
	}

	bCTFOvertime=false;
	AlphaScore = 0;
	AlphaLossStreak = BravoLossStreak = 0;
	LastSettledRound = -1;
	DamageHistory.Reset(); RewardedDeaths.Reset();
	for (AVanthexCharacter* Combatant : Combatants) if (auto* State = GetVanthexPlayerState(Combatant)) State->ResetEconomyForMatch();
	BravoScore = 0;
	RoundIndex = 0;
	MatchWinner = EVanthexTeam::None;
	RoundWinner = EVanthexTeam::None;
	TimeRemaining = RoundDuration;
	Phase = EVanthexRoundPhase::Waiting;
	PublishMatch();
	BeginRound();
}

void AVanthexGameMode::BeginRound()
{
	if (MatchWinner != EVanthexTeam::None || AlphaScore >= RoundsToWin || BravoScore >= RoundsToWin)
	{
		EnterMatchEnded();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlowTimer);
	}

	++RoundIndex;
	DamageHistory.Reset(); RewardedDeaths.Reset(); ProcessedThrows.Reset();
	PaidPasses.Reset(); PaidPassPairs.Reset(); PaidIntercepts.Reset(); AlphaPasses = BravoPasses = 0;
	RoundWinner = EVanthexTeam::None;
	const bool Legacy = UsesLegacyCombatFixture();
	TimeRemaining = Legacy ? RoundDuration : BuyDuration;
	Phase = Legacy ? EVanthexRoundPhase::RoundStarting : EVanthexRoundPhase::BuyPhase;
	CTFRespawns.Reset();
	ResetCombatants();
 if(IsCTF()) { for(AVanthexCharacter* Pawn:Combatants) RespawnCTF(Pawn); for(TActorIterator<AVanthexFlag> It(GetWorld());It;++It) It->ReturnHome(); }
	if (!Legacy) {
		for (AVanthexCharacter* Combatant : Combatants) if (Combatant) { Combatant->GetCharacterMovement()->StopMovementImmediately(); Combatant->GetCharacterMovement()->DisableMovement(); }
		BuyForBots();
	}
	PublishMatch();

	FlowStep = 1;
	if (UWorld* World = GetWorld())
	{
		if (Legacy) World->GetTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::OnRoundFlow, RoundStartDelay, false);
		else World->GetTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::TickBuyPhase, .1f, true);
	}
	UE_LOG(LogVanthex, Display, TEXT("Round %d starting"), RoundIndex);
}

void AVanthexGameMode::ActivateRound()
{
	if (Phase != EVanthexRoundPhase::RoundStarting && Phase != EVanthexRoundPhase::BuyPhase)
	{
		return;
	}

	Phase = EVanthexRoundPhase::RoundActive;
	for (AVanthexCharacter* Combatant : Combatants) if (Combatant) {
		RecordPlaytestSpawn(Combatant);
		Combatant->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		// Keep the weapon selected during preparation when combat begins.
		Combatant->SyncEconomyLoadout(true);
	}
	TimeRemaining = RoundDuration;
	PublishMatch();
	FlowStep = 2;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::OnRoundFlow, 0.1f, true);
	}
	UE_LOG(LogVanthex, Display, TEXT("Round %d active"), RoundIndex);
#if !UE_BUILD_SHIPPING
 extern void VanthexSchedulePerfCapture(AVanthexGameMode* Mode);
 VanthexSchedulePerfCapture(this);
#endif
}

void AVanthexGameMode::TickRoundClock()
{
	if (Phase != EVanthexRoundPhase::RoundActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlowTimer);
		}
		return;
	}

 SamplePlaytestActivity();
 if(IsCTF()) TickCTF(); else UpdateExtraction(0.1f);
	if (bEliminationDeferred) EvaluateElimination();
	if (Phase != EVanthexRoundPhase::RoundActive)
	{
		return;
	}

	TimeRemaining = FMath::Max(0.f, TimeRemaining - 0.1f);
	PublishMatch();
	if (TimeRemaining <= 0.f && !HasPendingDepositedExtraction())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlowTimer);
		}
		ResolveTimeout();
	}
}

void AVanthexGameMode::ResolveTimeout()
{
	if (Phase != EVanthexRoundPhase::RoundActive || HasPendingDepositedExtraction())
	{
		return;
	}

 if(IsCTF()) {
  if(AlphaScore==BravoScore&&!bCTFOvertime) {
   bCTFOvertime=true;TimeRemaining=180.f;PublishMatch();
   GetWorldTimerManager().SetTimer(FlowTimer,this,&AVanthexGameMode::OnRoundFlow,.1f,true);return;
  }
  MatchWinner=AlphaScore==BravoScore?EVanthexTeam::None:AlphaScore>BravoScore?EVanthexTeam::Alpha:EVanthexTeam::Bravo;
  CTFRespawns.Reset();EnterMatchEnded();return;
 }
	const int32 AlphaAlive = CountAlive(EVanthexTeam::Alpha);
	const int32 BravoAlive = CountAlive(EVanthexTeam::Bravo);
	UE_LOG(LogVanthex, Display, TEXT("Round timeout alive alpha %d bravo %d"), AlphaAlive, BravoAlive);
	if (AlphaAlive > BravoAlive)
	{
		EndRound(EVanthexTeam::Alpha);
	}
	else if (BravoAlive > AlphaAlive)
	{
		EndRound(EVanthexTeam::Bravo);
	}
	else
	{
		EndRound(EVanthexTeam::None);
	}
}

void AVanthexGameMode::EndRound(EVanthexTeam Winner)
{
	if (Phase != EVanthexRoundPhase::RoundActive)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlowTimer);
	}

	RoundWinner = Winner;
	SettleRoundEconomy(Winner, Core && Core->GetCoreState() == EVanthexCoreState::Extracted);
 for(AVanthexCharacter* Combatant:Combatants)if(Combatant&&Combatant->GetGadgets())Combatant->GetGadgets()->ResetForRound();
	for (AVanthexCharacter* Combatant : Combatants) if (Combatant && Combatant->GetWeapon()) { Combatant->GetWeapon()->StopFire(); Combatant->GetWeapon()->CancelReload(); }
	if (Winner == EVanthexTeam::Alpha)
	{
		++AlphaScore;
	}
	else if (Winner == EVanthexTeam::Bravo)
	{
		++BravoScore;
	}

	UE_LOG(LogVanthex, Display, TEXT("Round %d ended winner %d score %d-%d"), RoundIndex, static_cast<int32>(Winner), AlphaScore, BravoScore);

	if (AlphaScore >= RoundsToWin || BravoScore >= RoundsToWin)
	{
		EnterMatchEnded();
		return;
	}

	Phase = EVanthexRoundPhase::RoundEnding;
	PublishMatch();
	FlowStep = 3;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::OnRoundFlow, RoundEndDelay, false);
	}
}

void AVanthexGameMode::EnterIntermission()
{
	if (Phase == EVanthexRoundPhase::MatchEnded)
	{
		return;
	}

	Phase = EVanthexRoundPhase::Intermission;
	PublishMatch();
	FlowStep = 4;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::OnRoundFlow, IntermissionDuration, false);
	}
}

void AVanthexGameMode::EnterMatchEnded()
{
	// Duplicate end notifications must not extend the results countdown.
	if (Phase == EVanthexRoundPhase::MatchEnded) return;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlowTimer);
	}

	if (MatchWinner == EVanthexTeam::None && !IsCTF())
	{
		MatchWinner = AlphaScore >= BravoScore ? EVanthexTeam::Alpha : EVanthexTeam::Bravo;
	}
	Phase = EVanthexRoundPhase::MatchEnded;
	SavePlaytestReport();
	TimeRemaining = MatchRestartDelay;
	PublishMatch();
	FlowStep = 5;
	GetWorldTimerManager().SetTimer(FlowTimer, this, &AVanthexGameMode::OnRoundFlow, .1f, true);
	UE_LOG(LogVanthex, Display, TEXT("Match ended winner %d score %d-%d"), static_cast<int32>(MatchWinner), AlphaScore, BravoScore);
	UE_LOG(LogVanthex, Display, TEXT("Next match starts automatically in %.0f seconds"), MatchRestartDelay);
}

void AVanthexGameMode::TickMatchRestart()
{
	if (Phase != EVanthexRoundPhase::MatchEnded) return;
	TimeRemaining = FMath::Max(0.f, TimeRemaining - .1f);
	PublishMatch();
	if (TimeRemaining <= 0.f)
	{
		// Reset the match in place: keep human connections, teams and controllers.
		UE_LOG(LogVanthex, Display, TEXT("Automatic match restart"));
		RestartMatch();
	}
}

void AVanthexGameMode::ResetCombatants()
{
 AVanthexGadgetActor::ResetAll(GetWorld());
	if (Arena)
	{
		Arena->ResetRoundState();
		TestTarget = Arena->GetTarget();
	}

	for (AVanthexCharacter* Combatant : Combatants)
	{
		const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
		APlayerStart* Start = (Arena && PlayerState) ? Arena->GetTeamSpawn(PlayerState->GetTeam(), PlayerState->GetSpawnSlot()) : nullptr;
		if (!Combatant || !Start)
		{
			continue;
		}
		Combatant->PrepareForRound(Start->GetActorTransform());
	}

	ExtractionElapsed = 0.f;
	ActiveExtraction = EVanthexExtractionId::None;
	ExtractionTeam = EVanthexTeam::None;
	ExtractionPossessionRevision = 0;
	bEliminationDeferred = false;
	DepositedZone = EVanthexExtractionId::None;
	DepositedPossessionRevision = 0;
	RefreshCoreReadout();
}

void AVanthexGameMode::OnRoundFlow()
{
	switch (FlowStep)
	{
	case 1: ActivateRound(); break;
	case 2: TickRoundClock(); break;
	case 3: EnterIntermission(); break;
	case 4: BeginRound(); break;
	case 5: TickMatchRestart(); break;
	default: break;
	}
}

void AVanthexGameMode::RefreshCoreReadout()
{
	EVanthexCoreReadout Readout = EVanthexCoreReadout::Center;
	float Remaining = 0.f;
	if (Core)
	{
		switch (Core->GetCoreState())
		{
		case EVanthexCoreState::Carried:
			Readout = Core->GetCarrierTeam() == EVanthexTeam::Bravo ? EVanthexCoreReadout::Bravo : EVanthexCoreReadout::Alpha;
			break;
		case EVanthexCoreState::Thrown:
			Readout = EVanthexCoreReadout::Thrown;
			break;
		case EVanthexCoreState::Extracting:
			Readout = ActiveExtraction == EVanthexExtractionId::B ? EVanthexCoreReadout::ExtractingB : EVanthexCoreReadout::ExtractingA;
			Remaining = FMath::Max(0.f, ExtractionDuration - ExtractionElapsed);
			break;
		case EVanthexCoreState::Extracted:
			Readout = EVanthexCoreReadout::Extracted;
			break;
		case EVanthexCoreState::AtRest:
		default:
			Readout = Core->IsAtHome() ? EVanthexCoreReadout::Center : EVanthexCoreReadout::Dropped;
			break;
		}
	}

	// Extraction is an objective process, independent of the Core's flight/rest
	// state. Keep motion and pickup enabled while publishing the same countdown.
	if (Core && Phase == EVanthexRoundPhase::RoundActive && ActiveExtraction != EVanthexExtractionId::None
		&& Core->GetCoreState() != EVanthexCoreState::Extracted)
	{
		Readout = ActiveExtraction == EVanthexExtractionId::B ? EVanthexCoreReadout::ExtractingB : EVanthexCoreReadout::ExtractingA;
		Remaining = FMath::Max(0.f, ExtractionDuration - ExtractionElapsed);
	}
	if (AVanthexGameState* Match = GetGameState<AVanthexGameState>())
	{
		Match->PublishCore(Readout, Remaining, ExtractionDuration);
	}
}

void AVanthexGameMode::UpdateExtraction(float DeltaSeconds)
{
	HasPendingDepositedExtraction();
	if (!Core || Phase != EVanthexRoundPhase::RoundActive)
	{
		ExtractionElapsed = 0.f;
		ActiveExtraction = EVanthexExtractionId::None;
		ExtractionTeam = EVanthexTeam::None;
		ExtractionPossessionRevision = 0;
		if (Core && Core->GetCoreState() == EVanthexCoreState::Extracting)
		{
			Core->SetExtracting(false);
		}
		RefreshCoreReadout();
		return;
	}

	const EVanthexTeam Team = Core->GetExtractionTeam();
	AVanthexExtractionZone* Zone = nullptr;
	if (Team != EVanthexTeam::None && Core->GetCoreState() != EVanthexCoreState::Extracted && Arena)
	{
		if (AVanthexExtractionZone* ZoneA = Arena->GetExtractionZone(EVanthexExtractionId::A))
		{
			if (ZoneA->Contains(Core->GetActorLocation()))
			{
				Zone = ZoneA;
			}
		}
		if (!Zone)
		{
			if (AVanthexExtractionZone* ZoneB = Arena->GetExtractionZone(EVanthexExtractionId::B))
			{
				if (ZoneB->Contains(Core->GetActorLocation()))
				{
					Zone = ZoneB;
				}
			}
		}
	}

	if (!Zone)
	{
		ExtractionElapsed = 0.f;
		ActiveExtraction = EVanthexExtractionId::None;
		ExtractionTeam = EVanthexTeam::None;
		ExtractionPossessionRevision = 0;
		if (Core->GetCoreState() == EVanthexCoreState::Extracting)
		{
			Core->SetExtracting(false);
		}
		RefreshCoreReadout();
		if (Arena)
		{
			if (AVanthexExtractionZone* IdleA = Arena->GetExtractionZone(EVanthexExtractionId::A))
			{
				IdleA->SetCharge(0.f);
			}
			if (AVanthexExtractionZone* IdleB = Arena->GetExtractionZone(EVanthexExtractionId::B))
			{
				IdleB->SetCharge(0.f);
			}
		}
		return;
	}

	// A new zone or opponent possession starts a fresh countdown. A same-team
	// handoff/drop/throw preserves it, including release while still airborne.
	if (ActiveExtraction != Zone->GetId() || ExtractionTeam != Team || ExtractionPossessionRevision != Core->GetPossessionRevision())
		ExtractionElapsed = 0.f;
	ActiveExtraction = Zone->GetId();
	ExtractionTeam = Team;
	ExtractionPossessionRevision = Core->GetPossessionRevision();
	for (EVanthexExtractionId Id : {EVanthexExtractionId::A, EVanthexExtractionId::B})
		if (auto* OtherZone = Arena->GetExtractionZone(Id); OtherZone && OtherZone != Zone) OtherZone->SetCharge(0.f);
	Core->SetExtracting(true);
	ExtractionElapsed += DeltaSeconds;
	Zone->SetCharge(ExtractionElapsed / FMath::Max(ExtractionDuration, 0.01f));
	if (ExtractionElapsed + 0.02f >= ExtractionDuration)
	{
		const EVanthexTeam Winner = ExtractionTeam;
		Core->MarkExtracted();
		RefreshCoreReadout();
		EndRound(Winner);
		return;
	}

	RefreshCoreReadout();
}

void AVanthexGameMode::KillTeam(EVanthexTeam Team)
{
	for (AVanthexCharacter* Combatant : Combatants)
	{
		const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
		UHealthComponent* Health = Combatant ? Combatant->GetHealthComponent() : nullptr;
		if (!PlayerState || PlayerState->GetTeam() != Team || !Health || Health->IsDead())
		{
			continue;
		}

		FVanthexDamageRequest Request;
		Request.Damage = Health->GetMaxHealth();
		Request.bHeadshot = false;
		Health->ApplyDamage(Request);
	}
}

bool AVanthexGameMode::AreCombatantsReady() const
{
	if (Combatants.Num() < TeamSize() * 2)
	{
		return false;
	}

	for (const AVanthexCharacter* Combatant : Combatants)
	{
		const UHealthComponent* Health = Combatant ? Combatant->GetHealthComponent() : nullptr;
		const AHitscanWeapon* Weapon = Combatant ? Combatant->GetWeapon() : nullptr;
		const UCharacterMovementComponent* Movement = Combatant ? Combatant->GetCharacterMovement() : nullptr;
		const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
		APlayerStart* Start = (Arena && PlayerState) ? Arena->GetTeamSpawn(PlayerState->GetTeam(), PlayerState->GetSpawnSlot()) : nullptr;
		if (!Health || Health->IsDead() || !FMath::IsNearlyEqual(Health->GetHealth(), Health->GetMaxHealth(), 0.1f))
		{
			return false;
		}
		if (!Weapon || Weapon->IsReloading() || Weapon->GetAmmoInMagazine() != Weapon->GetMagazineSize())
		{
			return false;
		}
		if (!Movement || !Movement->IsMovingOnGround() || Movement->MovementMode != MOVE_Walking || Combatant->bIsCrouched)
		{
			return false;
		}
		if (!Start || FVector::Dist2D(Combatant->GetActorLocation(), Start->GetActorLocation()) > 80.f)
		{
			return false;
		}
	}
	return true;
}

AVanthexCharacter* AVanthexGameMode::GetTestCharacter() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	return PlayerController ? Cast<AVanthexCharacter>(PlayerController->GetPawn()) : nullptr;
}

void AVanthexGameMode::AimAt(const FVector& Point) const
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	PlayerController->SetControlRotation((Point - ViewLocation).Rotation());
}

void AVanthexGameMode::SelfTestExpect(bool bCondition, const TCHAR* Name, const FString& Detail)
{
	const FString Line = FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
	SelfTestLines.Add(Line);
	UE_LOG(LogVanthex, Display, TEXT("SelfTest %s"), *Line);
	if (!bCondition)
	{
		bSelfTestFailed = true;
	}
}

void AVanthexGameMode::FinishSelfTest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelfTestTimer);
	}

	// Regression coverage for ownership, death/reload and world collision.
 Phase=EVanthexRoundPhase::RoundActive;
 auto* Human=GetTestCharacter(); auto* Other=FindCombatant(EVanthexTeam::Bravo,0);
 if(Human && Other && Core) {
  Human->GetHealthComponent()->ResetHealth(); Other->GetHealthComponent()->ResetHealth(); Core->ResetToSpawn();
  Other->SetActorLocation(Core->GetActorLocation()+FVector(0,0,70)); Core->TryPickup(Other);
  Human->DropCarriedCore(); Human->ThrowCarriedCore();
  SelfTestExpect(Core->GetCarrier()==Other,TEXT("CORE_FOREIGN_INPUT_REJECTED"),TEXT("non-carrier cannot drop or throw"));
  Human->GetWeapon()->SetMagazineAmmo(5); Human->GetWeapon()->StartReload();
  FVanthexDamageRequest Damage; Damage.Damage=100; Human->GetHealthComponent()->ApplyDamage(Damage);
  SelfTestExpect(!Human->GetWeapon()->IsReloading() && Human->GetWeapon()->GetAmmoInMagazine()==5,TEXT("DEATH_CANCELS_RELOAD"),TEXT("ammo preserved"));
  Other->SetActorLocation(FVector(1900,0,100)); Other->GetController()->SetControlRotation(FRotator(0,0,0)); Core->Throw();
  for(int32 I=0;I<30;++I) Core->Tick(0.02f);
  SelfTestExpect(Core->GetActorLocation().X<1980,TEXT("CORE_WALL_COLLISION"),Core->GetActorLocation().ToString());
  Human->GetHealthComponent()->ResetHealth(); Human->SetActorLocation(FVector(0,-1400,100));
  auto* Device=GetWorld()->SpawnActor<AVanthexShiftDevice>(FVector(150,-1400,100),FRotator::ZeroRotator);
  if(Device) { Device->Configure(1); const bool Activated=Device->Activate(Human,250); const bool Repeated=Device->Activate(Human,250); SelfTestExpect(Activated && !Repeated,TEXT("SHIFT_USE_COOLDOWN"),TEXT("first use accepted, immediate repeat rejected")); Device->ResetDevice(); SelfTestExpect(!Device->bActive,TEXT("SHIFT_RESET"),TEXT("round state restored")); Device->Destroy(); }
 }
 const bool bPassed = !bSelfTestFailed;
	SelfTestLines.Insert(FString::Printf(TEXT("OVERALL %s"), bPassed ? TEXT("PASS") : TEXT("FAIL")), 0);
	const FString Report = FString::Join(SelfTestLines, TEXT("\n"));
	const FString Path = FPaths::ProjectSavedDir() / TEXT("VanthexSelfTest.txt");
	FFileHelper::SaveStringToFile(Report, *Path);
	UE_LOG(LogVanthex, Display, TEXT("Self-test %s\n%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Report);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void AVanthexGameMode::AdvanceCoreSelfTest()
{
	auto Fail = [this](const TCHAR* Name, const FString& Detail)
	{
		SelfTestExpect(false, Name, Detail);
		SelfTestStage = EVanthexSelfTestStage::Finish;
	};

	auto Pass = [this](const TCHAR* Name, const FString& Detail, EVanthexCoreTest Next)
	{
		SelfTestExpect(true, Name, Detail);
		CoreTest = Next;
		CoreTestMark = 0;
		SelfTestTicks = 0;
	};

	auto Place = [](AVanthexCharacter* Character, const FVector& Location, float Yaw)
	{
		if (!Character)
		{
			return;
		}
		Character->SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		}
	};

	auto Park = [this]()
	{
		for (AVanthexCharacter* Combatant : Combatants)
		{
			const AVanthexPlayerState* Roster = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
			APlayerStart* Start = (Arena && Roster) ? Arena->GetTeamSpawn(Roster->GetTeam(), Roster->GetSpawnSlot()) : nullptr;
			if (Combatant && Start)
			{
				Combatant->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	};

	if (!Core)
	{
		Fail(TEXT("CORE_SPAWN"), TEXT("missing core"));
		return;
	}

	AVanthexCharacter* Human = GetTestCharacter();
	AVanthexCharacter* Alpha2 = FindCombatant(EVanthexTeam::Alpha, 2);
	AVanthexCharacter* Bravo0 = FindCombatant(EVanthexTeam::Bravo, 0);
	AVanthexCharacter* Bravo1 = FindCombatant(EVanthexTeam::Bravo, 1);
	const AVanthexGameState* Match = GetGameState<AVanthexGameState>();

	switch (CoreTest)
	{
	case EVanthexCoreTest::Spawn:
		if (Core->GetCoreState() == EVanthexCoreState::AtRest && !Core->GetCarrier() && Core->IsAtHome())
		{
			Pass(TEXT("CORE_SPAWN"), TEXT("center at rest"), EVanthexCoreTest::PickupAlpha);
		}
		else
		{
			Fail(TEXT("CORE_SPAWN"), FString::Printf(TEXT("state %d home %d"), static_cast<int32>(Core->GetCoreState()), Core->IsAtHome() ? 1 : 0));
		}
		return;
	case EVanthexCoreTest::PickupAlpha:
		Place(Human, Core->GetActorLocation() + FVector(0.f, -70.f, 40.f), 90.f);
		if (Human && Core->TryPickup(Human) && Core->GetCarrier() == Human && Core->GetCoreState() == EVanthexCoreState::Carried)
		{
			Pass(TEXT("CORE_PICKUP_ALPHA"), TEXT("alpha carries"), EVanthexCoreTest::SingleCarrier);
		}
		else
		{
			Fail(TEXT("CORE_PICKUP_ALPHA"), TEXT("pickup failed"));
		}
		return;
	case EVanthexCoreTest::SingleCarrier:
		if (Bravo0)
		{
			Core->TryPickup(Bravo0);
		}
		if (Core->GetCarrier() == Human && Bravo0 && Core->GetCarrier() != Bravo0)
		{
			Pass(TEXT("CORE_SINGLE_CARRIER"), TEXT("bravo rejected"), EVanthexCoreTest::ManualDrop);
		}
		else
		{
			Fail(TEXT("CORE_SINGLE_CARRIER"), TEXT("carrier changed"));
		}
		return;
	case EVanthexCoreTest::ManualDrop:
		if (Human)
		{
			Human->DropCarriedCore();
		}
		if (!Core->GetCarrier() && Core->GetCoreState() != EVanthexCoreState::Carried)
		{
			Pass(TEXT("CORE_MANUAL_DROP"), TEXT("ownership cleared"), EVanthexCoreTest::WaitDrop);
		}
		else
		{
			Fail(TEXT("CORE_MANUAL_DROP"), TEXT("still carried"));
		}
		return;
	case EVanthexCoreTest::WaitDrop:
		if (Core->GetCoreState() == EVanthexCoreState::AtRest && !Core->GetCarrier())
		{
			CoreTest = EVanthexCoreTest::PickupBravo;
			SelfTestTicks = 0;
		}
		else if (++SelfTestTicks > 60)
		{
			Fail(TEXT("CORE_PICKUP_BRAVO"), TEXT("core did not settle"));
		}
		return;
	case EVanthexCoreTest::PickupBravo:
		Place(Bravo0, Core->GetActorLocation() + FVector(0.f, 70.f, 40.f), -90.f);
		if (Bravo0 && Core->TryPickup(Bravo0) && Core->GetCarrier() == Bravo0 && Core->GetCarrierTeam() == EVanthexTeam::Bravo)
		{
			Pass(TEXT("CORE_PICKUP_BRAVO"), TEXT("bravo carries"), EVanthexCoreTest::DropOnDeath);
		}
		else
		{
			Fail(TEXT("CORE_PICKUP_BRAVO"), TEXT("pickup failed"));
		}
		return;
	case EVanthexCoreTest::DropOnDeath:
		Park();
		if (Bravo0 && Bravo0->GetHealthComponent())
		{
			FVanthexDamageRequest Request;
			Request.Damage = Bravo0->GetHealthComponent()->GetMaxHealth();
			Bravo0->GetHealthComponent()->ApplyDamage(Request);
		}
		if (!Core->GetCarrier() && Core->GetCoreState() != EVanthexCoreState::Carried && Phase == EVanthexRoundPhase::RoundActive)
		{
			Pass(TEXT("CORE_DROP_ON_DEATH"), TEXT("dropped near carrier"), EVanthexCoreTest::Throw);
		}
		else
		{
			Fail(TEXT("CORE_DROP_ON_DEATH"), TEXT("core stayed with the dead carrier"));
		}
		return;
	case EVanthexCoreTest::Throw:
		if (Core->GetCoreState() != EVanthexCoreState::AtRest || Core->GetCarrier())
		{
			if (++SelfTestTicks > 60)
			{
				Fail(TEXT("CORE_THROW"), TEXT("core did not settle after death"));
			}
			return;
		}
		Park();
		Place(Human, Core->GetActorLocation(), 90.f);
		if (!Human || !Core->TryPickup(Human))
		{
			Fail(TEXT("CORE_THROW"), TEXT("pickup before throw failed"));
			return;
		}
		Place(Human, FVector(400.f, -200.f, 105.f), 90.f);
		Place(Alpha2, FVector(400.f, 40.f, 105.f), 90.f);
		Core->Throw();
		if (Core->GetCoreState() == EVanthexCoreState::Thrown && !Core->GetCarrier() && Core->GetVelocity().Size() > 500.f)
		{
			Pass(TEXT("CORE_THROW"), FString::Printf(TEXT("speed %.0f"), Core->GetVelocity().Size()), EVanthexCoreTest::TeammateCatch);
		}
		else
		{
			Fail(TEXT("CORE_THROW"), TEXT("did not enter thrown state"));
		}
		return;
	case EVanthexCoreTest::TeammateCatch:
		if (Core->GetCarrier() == Alpha2 && Core->GetCoreState() == EVanthexCoreState::Carried)
		{
			Pass(TEXT("CORE_TEAMMATE_CATCH"), TEXT("alpha caught the throw"), EVanthexCoreTest::Intercept);
		}
		else if (++SelfTestTicks > 40)
		{
			Fail(TEXT("CORE_TEAMMATE_CATCH"), FString::Printf(TEXT("state %d"), static_cast<int32>(Core->GetCoreState())));
		}
		return;
	case EVanthexCoreTest::Intercept:
		if (Core->GetCarrier() == Alpha2)
		{
			Park();
			Place(Alpha2, FVector(200.f, 200.f, 105.f), 0.f);
			Place(Bravo1, FVector(430.f, 200.f, 105.f), 0.f);
			Core->Throw();
			SelfTestTicks = 0;
			return;
		}
		if (Core->GetCarrier() == Bravo1 && Core->GetCoreState() == EVanthexCoreState::Carried)
		{
			Pass(TEXT("CORE_ENEMY_INTERCEPT"), TEXT("bravo intercepted"), EVanthexCoreTest::Settle);
		}
		else if (++SelfTestTicks > 40)
		{
			Fail(TEXT("CORE_ENEMY_INTERCEPT"), FString::Printf(TEXT("state %d"), static_cast<int32>(Core->GetCoreState())));
		}
		return;
	case EVanthexCoreTest::Settle:
		if (Core->GetCarrier())
		{
			Core->Drop();
			Park();
			SelfTestTicks = 0;
			return;
		}
		if (Core->GetCoreState() == EVanthexCoreState::AtRest && !Core->GetCarrier())
		{
			Pass(TEXT("CORE_SETTLE"), TEXT("at rest and pickup enabled"), EVanthexCoreTest::CancelLeave);
		}
		else if (++SelfTestTicks > 80)
		{
			Fail(TEXT("CORE_SETTLE"), TEXT("did not settle"));
		}
		return;
	case EVanthexCoreTest::CancelLeave:
		if (CoreTestMark == 0)
		{
			if (Core->GetCoreState() != EVanthexCoreState::AtRest)
			{
				if (++SelfTestTicks > 40)
				{
					Fail(TEXT("EXTRACTION_CANCEL_LEAVE"), TEXT("core not at rest"));
				}
				return;
			}
			Park();
			Place(Human, Core->GetActorLocation(), 0.f);
			if (!Human || !Core->TryPickup(Human))
			{
				Fail(TEXT("EXTRACTION_CANCEL_LEAVE"), TEXT("pickup failed"));
				return;
			}
			Place(Human, FVector(-850.f, 850.f, 105.f), 0.f);
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (CoreTestMark == 1)
		{
			if (Core->GetCoreState() == EVanthexCoreState::Extracting && ActiveExtraction == EVanthexExtractionId::A)
			{
				Place(Human, FVector(0.f, -1400.f, 105.f), 90.f);
				CoreTestMark = 2;
				SelfTestTicks = 0;
			}
			else if (++SelfTestTicks > 40)
			{
				Fail(TEXT("EXTRACTION_CANCEL_LEAVE"), TEXT("extraction did not start"));
			}
			return;
		}
		if (Core->GetCarrier() == Human && Core->GetCoreState() == EVanthexCoreState::Carried && ActiveExtraction == EVanthexExtractionId::None && Phase == EVanthexRoundPhase::RoundActive)
		{
			Pass(TEXT("EXTRACTION_CANCEL_LEAVE"), TEXT("left the zone"), EVanthexCoreTest::ContinueDrop);
		}
		else if (++SelfTestTicks > 40)
		{
			Fail(TEXT("EXTRACTION_CANCEL_LEAVE"), TEXT("extraction still active"));
		}
		return;
	case EVanthexCoreTest::ContinueDrop:
		if (CoreTestMark == 0)
		{
			Place(Human, FVector(-850.f, 850.f, 105.f), 0.f);
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (CoreTestMark == 1)
		{
			if (Core->GetCoreState() == EVanthexCoreState::Extracting)
			{
				if (Human)
				{
					Human->DropCarriedCore();
				}
				CoreTestMark = 2;
				SelfTestTicks = 0;
			}
			else if (++SelfTestTicks > 40)
			{
				Fail(TEXT("EXTRACTION_CONTINUE_DROP"), TEXT("extraction did not start"));
			}
			return;
		}
		if (!Core->GetCarrier() && ActiveExtraction == EVanthexExtractionId::A && Core->GetCoreState() != EVanthexCoreState::Extracting)
		{
			Pass(TEXT("EXTRACTION_CONTINUE_DROP"), TEXT("drop retains extraction"), EVanthexCoreTest::ContinueDeath);
		}
		else if (++SelfTestTicks > 40)
		{
			Fail(TEXT("EXTRACTION_CONTINUE_DROP"), TEXT("extraction still active"));
		}
		return;
	case EVanthexCoreTest::ContinueDeath:
		if (CoreTestMark == 0)
		{
			if (Core->GetCoreState() != EVanthexCoreState::AtRest || Core->GetCarrier())
			{
				if (++SelfTestTicks > 80)
				{
					Fail(TEXT("EXTRACTION_CONTINUE_DEATH"), TEXT("core did not settle"));
				}
				return;
			}
			Park();
			Place(Alpha2, Core->GetActorLocation(), 0.f);
			if (!Alpha2 || !Core->TryPickup(Alpha2))
			{
				Fail(TEXT("EXTRACTION_CONTINUE_DEATH"), TEXT("pickup failed"));
				return;
			}
			Place(Alpha2, FVector(-850.f, 850.f, 105.f), 0.f);
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (Core->GetCoreState() == EVanthexCoreState::Extracting && Alpha2 && Alpha2->GetHealthComponent())
		{
			FVanthexDamageRequest Request;
			Request.Damage = Alpha2->GetHealthComponent()->GetMaxHealth();
			Alpha2->GetHealthComponent()->ApplyDamage(Request);
			if (!Core->GetCarrier() && ActiveExtraction == EVanthexExtractionId::A && Phase == EVanthexRoundPhase::RoundActive)
			{
				Pass(TEXT("EXTRACTION_CONTINUE_DEATH"), TEXT("death drops Core while team extraction continues"), EVanthexCoreTest::ExtractionA);
			}
			else
			{
				Fail(TEXT("EXTRACTION_CONTINUE_DEATH"), TEXT("extraction or carrier remained"));
			}
			return;
		}
		if (++SelfTestTicks > 40)
		{
			Fail(TEXT("EXTRACTION_CONTINUE_DEATH"), TEXT("extraction did not start"));
		}
		return;
	case EVanthexCoreTest::ExtractionA:
		if (CoreTestMark == 0)
		{
			if (Core->GetCoreState() != EVanthexCoreState::AtRest || Core->GetCarrier())
			{
				if (++SelfTestTicks > 80)
				{
					Fail(TEXT("EXTRACTION_A"), TEXT("core did not settle"));
				}
				return;
			}
			Park();
			Place(Human, Core->GetActorLocation(), 0.f);
			if (!Human || !Core->TryPickup(Human))
			{
				Fail(TEXT("EXTRACTION_A"), TEXT("pickup failed"));
				return;
			}
			Place(Human, FVector(-850.f, 850.f, 105.f), 0.f);
			CoreScoreBefore = Match ? Match->GetAlphaScore() : AlphaScore;
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (Match && Match->GetAlphaScore() > CoreScoreBefore && Match->GetRoundWinner() == EVanthexTeam::Alpha)
		{
			Pass(TEXT("EXTRACTION_A"), FString::Printf(TEXT("score %d-%d"), Match->GetAlphaScore(), Match->GetBravoScore()), EVanthexCoreTest::RoundReset);
		}
		else if (++SelfTestTicks > 360)
		{
			Fail(TEXT("EXTRACTION_A"), TEXT("zone hold did not win the round"));
		}
		return;
	case EVanthexCoreTest::RoundReset:
		if (Match && Match->GetPhase() == EVanthexRoundPhase::RoundActive && Core->IsAtHome() && !Core->GetCarrier() && Core->GetCoreState() == EVanthexCoreState::AtRest && ActiveExtraction == EVanthexExtractionId::None)
		{
			Pass(TEXT("ROUND_CORE_RESET"), TEXT("core restored at center"), EVanthexCoreTest::ExtractionB);
		}
		else if (++SelfTestTicks > 80)
		{
			Fail(TEXT("ROUND_CORE_RESET"), TEXT("core was not restored"));
		}
		return;
	case EVanthexCoreTest::ExtractionB:
		if (CoreTestMark == 0)
		{
			if (Phase != EVanthexRoundPhase::RoundActive)
			{
				if (++SelfTestTicks > 40)
				{
					Fail(TEXT("EXTRACTION_B"), TEXT("round not active"));
				}
				return;
			}
			Park();
			Place(Bravo0, FVector(850.f, -750.f, 105.f), 180.f);
			if (!Bravo0 || !Core->TryPickup(Bravo0))
			{
				Place(Bravo0, Core->GetActorLocation(), 180.f);
				if (!Bravo0 || !Core->TryPickup(Bravo0))
				{
					Fail(TEXT("EXTRACTION_B"), TEXT("pickup failed"));
					return;
				}
				Place(Bravo0, FVector(850.f, -750.f, 105.f), 180.f);
			}
			CoreScoreBefore = Match ? Match->GetBravoScore() : BravoScore;
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (Match && Match->GetBravoScore() > CoreScoreBefore && Match->GetRoundWinner() == EVanthexTeam::Bravo)
		{
			Pass(TEXT("EXTRACTION_B"), FString::Printf(TEXT("score %d-%d"), Match->GetAlphaScore(), Match->GetBravoScore()), EVanthexCoreTest::F5Reset);
		}
		else if (++SelfTestTicks > 360)
		{
			Fail(TEXT("EXTRACTION_B"), TEXT("zone hold did not win the round"));
		}
		return;
	case EVanthexCoreTest::F5Reset:
		if (CoreTestMark == 0)
		{
			if (Phase != EVanthexRoundPhase::RoundActive || Core->GetCoreState() != EVanthexCoreState::AtRest || Core->GetCarrier())
			{
				if (++SelfTestTicks > 80)
				{
					Fail(TEXT("F5_CORE_RESET"), TEXT("could not start a carried restart"));
				}
				return;
			}
			Place(Human, Core->GetActorLocation(), 90.f);
			if (!Human || !Core->TryPickup(Human))
			{
				Fail(TEXT("F5_CORE_RESET"), TEXT("pickup failed"));
				return;
			}
			Place(Human, FVector(500.f, 500.f, 105.f), 90.f);
			if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				PlayerController->FlushPressedKeys();
				const FInputKeyEventArgs Press = FInputKeyEventArgs::CreateSimulated(EKeys::F5, IE_Pressed, 1.f);
				const bool bAccepted = PlayerController->InputKey(Press);
				UE_LOG(LogVanthex, Display, TEXT("SelfTest F5 inject accepted %d keydown %d"), bAccepted ? 1 : 0, PlayerController->IsInputKeyDown(EKeys::F5) ? 1 : 0);
			}
			CoreTestMark = 1;
			SelfTestTicks = 0;
			return;
		}
		if (Match && Match->GetAlphaScore() == 0 && Match->GetBravoScore() == 0 && Core->IsAtHome() && !Core->GetCarrier() && Core->GetCoreState() == EVanthexCoreState::AtRest && Human && Human->GetHealthComponent() && !Human->GetHealthComponent()->IsDead() && FVector::Dist2D(Human->GetActorLocation(), FVector(0.f, -1400.f, 0.f)) < 80.f)
		{
			Pass(TEXT("F5_CORE_RESET"), TEXT("match and core restored"), EVanthexCoreTest::M1Regression);
		}
		else if (++SelfTestTicks > 80)
		{
			Fail(TEXT("F5_CORE_RESET"), TEXT("core or score was not restored"));
		}
		return;
	case EVanthexCoreTest::M1Regression:
		if (Phase != EVanthexRoundPhase::RoundActive)
		{
			if (++SelfTestTicks > 40)
			{
				Fail(TEXT("M1_REGRESSION"), TEXT("round did not become active"));
			}
			return;
		}
		if (TestTarget)
		{
			AimAt(TestTarget->GetBodyCenter() + FVector(0.f, 0.f, 20.f));
		}
		CoreTest = EVanthexCoreTest::M1RegressionFire;
		SelfTestTicks = 0;
		return;
	case EVanthexCoreTest::M1RegressionFire:
	{
		AHitscanWeapon* Weapon = Human ? Human->GetWeapon() : nullptr;
		UHealthComponent* TargetHealth = TestTarget ? TestTarget->GetHealthComponent() : nullptr;
		UCharacterMovementComponent* Movement = Human ? Human->GetCharacterMovement() : nullptr;
		if (!Weapon || !TargetHealth || !Movement)
		{
			Fail(TEXT("M1_REGRESSION"), TEXT("missing weapon or target"));
			return;
		}
		const float HealthBefore = TargetHealth->GetHealth();
		Weapon->FireShot();
		const bool bShot = FMath::IsNearlyEqual(TargetHealth->GetLastDamage(), Weapon->GetBodyDamage(), 0.1f) && TargetHealth->GetHealth() < HealthBefore && !TargetHealth->WasLastHitHeadshot();
		const bool bMove = FMath::IsNearlyEqual(Movement->MaxWalkSpeed, 600.f, 1.f) && Weapon->GetMagazineSize() == 30;
		if (bShot && bMove)
		{
			Pass(TEXT("M1_REGRESSION"), TEXT("walk 600 body shot 40"), EVanthexCoreTest::M2Regression);
		}
		else
		{
			Fail(TEXT("M1_REGRESSION"), FString::Printf(TEXT("hp %.0f last %.0f speed %.0f"), TargetHealth->GetHealth(), TargetHealth->GetLastDamage(), Movement->MaxWalkSpeed));
		}
		return;
	}
	case EVanthexCoreTest::M2Regression:
	{
		const bool bTeams = CountAlive(EVanthexTeam::Alpha) == 3 && CountAlive(EVanthexTeam::Bravo) == 3;
		const bool bNoFriendly = Human && Alpha2 && !CanDamage(Human, Alpha2);
		const bool bRound = Phase == EVanthexRoundPhase::RoundActive;
		if (bTeams && bNoFriendly && bRound)
		{
			SelfTestExpect(true, TEXT("M2_REGRESSION"), TEXT("3v3 friendly fire off round active"));
			CoreTest = EVanthexCoreTest::Done;
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Match;
		}
		else
		{
			Fail(TEXT("M2_REGRESSION"), FString::Printf(TEXT("alpha %d bravo %d ff %d"), CountAlive(EVanthexTeam::Alpha), CountAlive(EVanthexTeam::Bravo), bNoFriendly ? 0 : 1));
		}
		return;
	}
	case EVanthexCoreTest::Done:
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::Match;
		return;
	default:
		Fail(TEXT("CORE_SPAWN"), TEXT("unknown core test"));
		return;
	}
}

void AVanthexGameMode::AdvanceSelfTest()
{
	if (SelfTestStage == EVanthexSelfTestStage::Finish)
	{
		FinishSelfTest();
		return;
	}

	AVanthexCharacter* Character = GetTestCharacter();
	AHitscanWeapon* Weapon = Character ? Character->GetWeapon() : nullptr;
	UHealthComponent* TargetHealth = TestTarget ? TestTarget->GetHealthComponent() : nullptr;

	switch (SelfTestStage)
	{
	case EVanthexSelfTestStage::WaitForWorld:
	{
		const bool bGrounded = Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();
		if (Character && Weapon && TestTarget && TargetHealth && bGrounded)
		{
			SelfTestExpect(true, TEXT("spawn"), Character->GetActorLocation().ToCompactString());
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Sprint;
			return;
		}
		if (++SelfTestTicks > 80)
		{
			SelfTestExpect(false, TEXT("spawn"), TEXT("player, weapon, target, or ground was not ready"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
		}
		return;
	}
	case EVanthexSelfTestStage::Sprint:
	{
		if (!Character)
		{
			SelfTestExpect(false, TEXT("sprint"), TEXT("no character"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		Character->SetSprinting(true);
		const float Speed = Character->GetCharacterMovement()->MaxWalkSpeed;
		SelfTestExpect(FMath::IsNearlyEqual(Speed, 900.f, 0.1f), TEXT("sprint"), FString::Printf(TEXT("speed %.1f"), Speed));
		Character->SetSprinting(false);
		Character->SetCrouching(true);
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::CrouchWait;
		return;
	}
	case EVanthexSelfTestStage::CrouchWait:
	{
		if (++SelfTestTicks < 6)
		{
			return;
		}
		SelfTestExpect(Character && Character->bIsCrouched, TEXT("crouch"), Character && Character->bIsCrouched ? TEXT("crouched") : TEXT("not crouched"));
		if (Character)
		{
			Character->SetCrouching(false);
		}
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::UncrouchWait;
		return;
	}
	case EVanthexSelfTestStage::UncrouchWait:
	{
		if (++SelfTestTicks < 6)
		{
			return;
		}
		SelfTestExpect(Character && !Character->bIsCrouched, TEXT("uncrouch"), Character && !Character->bIsCrouched ? TEXT("standing") : TEXT("still crouched"));
		MoveStart = Character ? Character->GetActorLocation() : FVector::ZeroVector;
		MoveStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Character)
		{
			Character->SetHoldForward(true);
		}
		SelfTestStage = EVanthexSelfTestStage::Move;
		return;
	}
	case EVanthexSelfTestStage::Move:
	{
		if (Character)
		{
			Character->DoMove(0.f, 1.f);
		}
		const float Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - MoveStartTime : 0.f;
		if (Elapsed < 0.4f)
		{
			return;
		}
		const float Moved = Character ? FVector::Dist2D(Character->GetActorLocation(), MoveStart) : 0.f;
		const float Speed = Character && Character->GetCharacterMovement() ? Character->GetCharacterMovement()->MaxWalkSpeed : 0.f;
		if (Character)
		{
			Character->SetHoldForward(false);
		}
		SelfTestExpect(Moved > 30.f, TEXT("move"), FString::Printf(TEXT("distance %.1f speed %.0f"), Moved, Speed));
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::BodyShot;
		return;
	}
	case EVanthexSelfTestStage::JumpWait:
	{
		const bool bAirborne = Character && Character->GetCharacterMovement() && (Character->GetCharacterMovement()->IsFalling() || Character->GetCharacterMovement()->Velocity.Z > 10.f);
		if (++SelfTestTicks >= 2)
		{
			SelfTestExpect(bAirborne, TEXT("jump"), TEXT("jump left the ground"));
			Character->StopJumping();
			SelfTestStage = EVanthexSelfTestStage::PlayerDeath;
		}
		return;
	}
	case EVanthexSelfTestStage::BodyShot:
	{
		if (!Weapon || !TestTarget || !TargetHealth)
		{
			SelfTestExpect(false, TEXT("hitscan_body"), TEXT("missing weapon or target"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		if (SelfTestTicks == 0)
		{
			const FVector AimPoint = TestTarget->GetBodyCenter() + FVector(0.f, 0.f, 20.f);
			AimAt(AimPoint);
			UE_LOG(LogVanthex, Display, TEXT("SelfTest aim body %s from %s"), *AimPoint.ToCompactString(), *Character->GetActorLocation().ToCompactString());
			++SelfTestTicks;
			return;
		}
		const float HealthBefore = TargetHealth->GetHealth();
		Weapon->FireShot();
		const float Expected = Weapon->GetBodyDamage();
		const bool bDamaged = FMath::IsNearlyEqual(TargetHealth->GetLastDamage(), Expected, 0.1f) && TargetHealth->GetHealth() < HealthBefore && !TargetHealth->WasLastHitHeadshot();
		SelfTestExpect(bDamaged, TEXT("hitscan_body"), FString::Printf(TEXT("hp %.0f last %.0f head %s"), TargetHealth->GetHealth(), TargetHealth->GetLastDamage(), TargetHealth->WasLastHitHeadshot() ? TEXT("yes") : TEXT("no")));
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::HeadShot;
		return;
	}
	case EVanthexSelfTestStage::HeadShot:
	{
		if (SelfTestTicks < 6)
		{
			++SelfTestTicks;
			return;
		}
		if (!Weapon || !TestTarget || !TargetHealth)
		{
			SelfTestExpect(false, TEXT("hitscan_head"), TEXT("missing weapon or target"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		if (SelfTestTicks == 6)
		{
			AimAt(TestTarget->GetHeadCenter());
			++SelfTestTicks;
			return;
		}
		Weapon->FireShot();
		const float Expected = Weapon->GetBodyDamage() * Weapon->GetHeadshotMultiplier();
		const bool bHeadKill = TargetHealth->IsDead() && TargetHealth->WasLastHitHeadshot() && FMath::IsNearlyEqual(TargetHealth->GetLastDamage(), Expected, 0.1f);
		SelfTestExpect(bHeadKill, TEXT("hitscan_head_kill"), FString::Printf(TEXT("dead %s last %.0f head %s"), TargetHealth->IsDead() ? TEXT("yes") : TEXT("no"), TargetHealth->GetLastDamage(), TargetHealth->WasLastHitHeadshot() ? TEXT("yes") : TEXT("no")));
		if (Weapon)
		{
			Weapon->SetMagazineAmmo(0);
			Weapon->StartReload();
		}
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::ReloadWait;
		return;
	}
	case EVanthexSelfTestStage::ReloadWait:
	{
		if (Weapon && !Weapon->IsReloading() && Weapon->GetAmmoInMagazine() == Weapon->GetMagazineSize() && SelfTestTicks > 0)
		{
			SelfTestExpect(true, TEXT("reload"), FString::Printf(TEXT("%d/%d"), Weapon->GetAmmoInMagazine(), Weapon->GetMagazineSize()));
			if (Character)
			{
				Character->Jump();
			}
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::JumpWait;
			return;
		}
		if (++SelfTestTicks > 50)
		{
			const int32 Ammo = Weapon ? Weapon->GetAmmoInMagazine() : -1;
			const bool bReloading = Weapon && Weapon->IsReloading();
			SelfTestExpect(false, TEXT("reload"), FString::Printf(TEXT("ammo %d reloading %s"), Ammo, bReloading ? TEXT("yes") : TEXT("no")));
			if (Character)
			{
				Character->Jump();
			}
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::JumpWait;
		}
		return;
	}
	case EVanthexSelfTestStage::PlayerDeath:
	{
		UHealthComponent* PlayerHealth = Character ? Character->GetHealthComponent() : nullptr;
		if (!PlayerHealth)
		{
			SelfTestExpect(false, TEXT("player_death"), TEXT("no health component"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		FVanthexDamageRequest Request;
		Request.Damage = PlayerHealth->GetMaxHealth();
		Request.bHeadshot = false;
		PlayerHealth->ApplyDamage(Request);
		SelfTestExpect(PlayerHealth->IsDead(), TEXT("player_death"), PlayerHealth->IsDead() ? TEXT("dead") : TEXT("alive"));
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::Restart;
		return;
	}
	case EVanthexSelfTestStage::Restart:
	{
		APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (!PlayerController)
		{
			SelfTestExpect(false, TEXT("restart"), TEXT("no player controller"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}

		const FInputKeyEventArgs Key = FInputKeyEventArgs::CreateSimulated(EKeys::F5, IE_Pressed, 1.f);
		PlayerController->InputKey(Key);
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::RestartWait;
		return;
	}
	case EVanthexSelfTestStage::RestartWait:
	{
		if (SelfTestTicks == 0)
		{
			if (APlayerController* ReleaseController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			{
				const FInputKeyEventArgs Release = FInputKeyEventArgs::CreateSimulated(EKeys::F5, IE_Released, 0.f);
				ReleaseController->InputKey(Release);
			}
		}
		AVanthexCharacter* ResetCharacter = GetTestCharacter();
		AHitscanWeapon* ResetWeapon = ResetCharacter ? ResetCharacter->GetWeapon() : nullptr;
		UHealthComponent* ResetPlayerHealth = ResetCharacter ? ResetCharacter->GetHealthComponent() : nullptr;
		UCharacterMovementComponent* Movement = ResetCharacter ? ResetCharacter->GetCharacterMovement() : nullptr;
		const bool bGrounded = Movement && Movement->IsMovingOnGround() && Movement->MovementMode == MOVE_Walking;
		const bool bAtSpawn = ResetCharacter && FVector::Dist2D(ResetCharacter->GetActorLocation(), FVector(0.f, -1400.f, 0.f)) < 80.f;
		const bool bPlayerReady = ResetPlayerHealth && !ResetPlayerHealth->IsDead() && FMath::IsNearlyEqual(ResetPlayerHealth->GetHealth(), ResetPlayerHealth->GetMaxHealth(), 0.1f);
		const bool bWeaponReady = ResetWeapon && !ResetWeapon->IsReloading() && ResetWeapon->GetAmmoInMagazine() == ResetWeapon->GetMagazineSize();
		const bool bTargetReady = TargetHealth && !TargetHealth->IsDead() && FMath::IsNearlyEqual(TargetHealth->GetHealth(), TargetHealth->GetMaxHealth(), 0.1f) && TestTarget->IsCombatCollisionEnabled();
		const float YawError = ResetCharacter ? FMath::Abs(FRotator::NormalizeAxis(ResetCharacter->GetControlRotation().Yaw - 90.f)) : 180.f;
		const bool bFacingSpawn = YawError < 5.f;
		const bool bStanding = ResetCharacter && !ResetCharacter->bIsCrouched;

		if (bGrounded && bAtSpawn && bPlayerReady && bWeaponReady && bTargetReady && bFacingSpawn && bStanding)
		{
			SelfTestExpect(true, TEXT("restart"), FString::Printf(
				TEXT("hp %.0f ammo %d/%d target %.0f at %s"),
				ResetPlayerHealth->GetHealth(),
				ResetWeapon->GetAmmoInMagazine(),
				ResetWeapon->GetMagazineSize(),
				TargetHealth->GetHealth(),
				*ResetCharacter->GetActorLocation().ToCompactString()));
			RoundEndDelay = 0.15f;
			IntermissionDuration = 0.15f;
			RoundStartDelay = 0.1f;
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Team;
			return;
		}

		if (++SelfTestTicks > 40)
		{
			SelfTestExpect(false, TEXT("restart"), FString::Printf(
				TEXT("ground %d spawn %d player %d weapon %d target %d facing %d standing %d loc %s"),
				bGrounded ? 1 : 0,
				bAtSpawn ? 1 : 0,
				bPlayerReady ? 1 : 0,
				bWeaponReady ? 1 : 0,
				bTargetReady ? 1 : 0,
				bFacingSpawn ? 1 : 0,
				bStanding ? 1 : 0,
				ResetCharacter ? *ResetCharacter->GetActorLocation().ToCompactString() : TEXT("none")));
			SelfTestStage = EVanthexSelfTestStage::Finish;
		}
		return;
	}
	case EVanthexSelfTestStage::Team:
	{
		int32 AlphaCount = 0;
		int32 BravoCount = 0;
		bool bSlots = true;
		bool bAlphaSlot[MaxPlayersPerTeam] = {};
		bool bBravoSlot[MaxPlayersPerTeam] = {};
		for (const AVanthexCharacter* Combatant : Combatants)
		{
			const AVanthexPlayerState* PlayerState = Combatant ? GetVanthexPlayerState(Combatant) : nullptr;
			if (!PlayerState)
			{
				bSlots = false;
				continue;
			}
			const int32 Slot = PlayerState->GetSpawnSlot();
			if (Slot < 0 || Slot >= MaxPlayersPerTeam)
			{
				bSlots = false;
				continue;
			}
			if (PlayerState->GetTeam() == EVanthexTeam::Alpha)
			{
				++AlphaCount;
				bSlots = bSlots && !bAlphaSlot[Slot];
				bAlphaSlot[Slot] = true;
			}
			else if (PlayerState->GetTeam() == EVanthexTeam::Bravo)
			{
				++BravoCount;
				bSlots = bSlots && !bBravoSlot[Slot];
				bBravoSlot[Slot] = true;
			}
			else
			{
				bSlots = false;
			}
		}
		const bool bTeams = AlphaCount == MaxPlayersPerTeam && BravoCount == MaxPlayersPerTeam && bSlots;
		SelfTestExpect(bTeams, TEXT("team"), FString::Printf(TEXT("alpha %d bravo %d slots %s"), AlphaCount, BravoCount, bSlots ? TEXT("ok") : TEXT("bad")));
		SelfTestTicks = 0;
		SelfTestStage = bTeams ? EVanthexSelfTestStage::FriendlyFireAim : EVanthexSelfTestStage::Finish;
		return;
	}
	case EVanthexSelfTestStage::FriendlyFireAim:
	{
		if (Phase != EVanthexRoundPhase::RoundActive)
		{
			if (++SelfTestTicks > 40)
			{
				SelfTestExpect(false, TEXT("friendly_fire"), TEXT("round was not active"));
				SelfTestStage = EVanthexSelfTestStage::Finish;
			}
			return;
		}
		AVanthexCharacter* Teammate = FindCombatant(EVanthexTeam::Alpha, 1);
		if (!Teammate)
		{
			SelfTestExpect(false, TEXT("friendly_fire"), TEXT("missing alpha teammate"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		AimAt(Teammate->GetActorLocation());
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::FriendlyFire;
		return;
	}
	case EVanthexSelfTestStage::FriendlyFire:
	{
		AVanthexCharacter* Teammate = FindCombatant(EVanthexTeam::Alpha, 1);
		UHealthComponent* TeammateHealth = Teammate ? Teammate->GetHealthComponent() : nullptr;
		AHitscanWeapon* PlayerWeapon = Character ? Character->GetWeapon() : nullptr;
		APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (!Teammate || !TeammateHealth || !PlayerWeapon || !PlayerController)
		{
			SelfTestExpect(false, TEXT("friendly_fire"), TEXT("missing teammate or weapon"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		if (SelfTestTicks++ < 6)
		{
			return;
		}

		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexFriendlyFireTest), false, Character);
		const bool bSawTeammate = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 15000.f, ECC_Visibility, Params) && Hit.GetActor() == Teammate;
		const float HealthBefore = TeammateHealth->GetHealth();
		const int32 AmmoBefore = PlayerWeapon->GetAmmoInMagazine();
		PlayerWeapon->FireShot();
		const bool bBlocked = bSawTeammate && PlayerWeapon->GetAmmoInMagazine() == AmmoBefore - 1 && FMath::IsNearlyEqual(TeammateHealth->GetHealth(), HealthBefore, 0.1f);
		SelfTestExpect(bBlocked, TEXT("friendly_fire"), FString::Printf(TEXT("saw %d hp %.0f ammo %d"), bSawTeammate ? 1 : 0, TeammateHealth->GetHealth(), PlayerWeapon->GetAmmoInMagazine()));
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::EnemyAim;
		return;
	}
	case EVanthexSelfTestStage::EnemyAim:
	{
		if (SelfTestTicks++ < 6)
		{
			return;
		}
		AVanthexCharacter* Enemy = FindCombatant(EVanthexTeam::Bravo, 2);
		if (!Enemy)
		{
			SelfTestExpect(false, TEXT("enemy_damage"), TEXT("missing bravo"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		AimAt(Enemy->GetActorLocation());
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::EnemyDamage;
		return;
	}
	case EVanthexSelfTestStage::EnemyDamage:
	{
		AVanthexCharacter* Enemy = FindCombatant(EVanthexTeam::Bravo, 2);
		UHealthComponent* EnemyHealth = Enemy ? Enemy->GetHealthComponent() : nullptr;
		AHitscanWeapon* PlayerWeapon = Character ? Character->GetWeapon() : nullptr;
		if (!EnemyHealth || !PlayerWeapon)
		{
			SelfTestExpect(false, TEXT("enemy_damage"), TEXT("missing enemy or weapon"));
			SelfTestStage = EVanthexSelfTestStage::Finish;
			return;
		}
		const float HealthBefore = EnemyHealth->GetHealth();
		PlayerWeapon->FireShot();
		const bool bDamaged = EnemyHealth->GetHealth() < HealthBefore && !EnemyHealth->IsDead();
		SelfTestExpect(bDamaged, TEXT("enemy_damage"), FString::Printf(TEXT("hp %.0f last %.0f"), EnemyHealth->GetHealth(), EnemyHealth->GetLastDamage()));
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::Timeout;
		return;
	}
	case EVanthexSelfTestStage::Timeout:
	{
		const AVanthexGameState* Match = GetGameState<AVanthexGameState>();
		if (!Match || Match->GetPhase() != EVanthexRoundPhase::RoundActive)
		{
			if (++SelfTestTicks > 40)
			{
				SelfTestExpect(false, TEXT("timeout"), TEXT("round was not active"));
				SelfTestStage = EVanthexSelfTestStage::Finish;
			}
			return;
		}
		TimeoutRoundIndex = RoundIndex;
		ResolveTimeout();
		SelfTestTicks = 0;
		SelfTestStage = EVanthexSelfTestStage::TimeoutWait;
		return;
	}
	case EVanthexSelfTestStage::TimeoutWait:
	{
		const AVanthexGameState* Match = GetGameState<AVanthexGameState>();
		const bool bDraw = Match && Match->GetAlphaScore() == 0 && Match->GetBravoScore() == 0 && Match->GetRoundWinner() == EVanthexTeam::None;
		const bool bNext = Match && Match->GetPhase() == EVanthexRoundPhase::RoundActive && Match->GetRoundIndex() == TimeoutRoundIndex + 1;
		if (bDraw && bNext)
		{
			SelfTestExpect(true, TEXT("timeout"), FString::Printf(TEXT("draw score %d-%d round %d"), Match->GetAlphaScore(), Match->GetBravoScore(), Match->GetRoundIndex()));
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Elimination;
			return;
		}
		if (++SelfTestTicks > 80)
		{
			SelfTestExpect(false, TEXT("timeout"), FString::Printf(TEXT("phase %d score %d-%d round %d winner %d"),
				Match ? static_cast<int32>(Match->GetPhase()) : -1,
				Match ? Match->GetAlphaScore() : -1,
				Match ? Match->GetBravoScore() : -1,
				Match ? Match->GetRoundIndex() : -1,
				Match ? static_cast<int32>(Match->GetRoundWinner()) : -1));
			SelfTestStage = EVanthexSelfTestStage::Finish;
		}
		return;
	}
	case EVanthexSelfTestStage::Elimination:
	{
		const AVanthexGameState* Match = GetGameState<AVanthexGameState>();
		if (!Match || Match->GetPhase() != EVanthexRoundPhase::RoundActive)
		{
			if (++SelfTestTicks > 40)
			{
				SelfTestExpect(false, TEXT("elimination"), TEXT("round was not active"));
				SelfTestStage = EVanthexSelfTestStage::Finish;
			}
			return;
		}
		KillTeam(EVanthexTeam::Bravo);
		const bool bWon = Match->GetAlphaScore() == 1 && Match->GetBravoScore() == 0 && Match->GetRoundWinner() == EVanthexTeam::Alpha && CountAlive(EVanthexTeam::Bravo) == 0;
		SelfTestExpect(bWon, TEXT("elimination"), FString::Printf(TEXT("score %d-%d bravo alive %d"), Match->GetAlphaScore(), Match->GetBravoScore(), CountAlive(EVanthexTeam::Bravo)));
		SelfTestTicks = 0;
		SelfTestStage = bWon ? EVanthexSelfTestStage::RoundReset : EVanthexSelfTestStage::Finish;
		return;
	}
	case EVanthexSelfTestStage::RoundReset:
	{
		const AVanthexGameState* Match = GetGameState<AVanthexGameState>();
		const bool bActive = Match && Match->GetPhase() == EVanthexRoundPhase::RoundActive && Match->GetRoundIndex() > TimeoutRoundIndex + 1;
		if (bActive && AreCombatantsReady())
		{
			SelfTestExpect(true, TEXT("round_reset"), FString::Printf(TEXT("alive %d at spawns round %d"), Combatants.Num(), Match->GetRoundIndex()));
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Score;
			return;
		}
		if (++SelfTestTicks > 80)
		{
			SelfTestExpect(false, TEXT("round_reset"), FString::Printf(TEXT("ready %d phase %d round %d"), AreCombatantsReady() ? 1 : 0, Match ? static_cast<int32>(Match->GetPhase()) : -1, Match ? Match->GetRoundIndex() : -1));
			SelfTestStage = EVanthexSelfTestStage::Finish;
		}
		return;
	}
	case EVanthexSelfTestStage::Score:
	{
		const AVanthexGameState* Match = GetGameState<AVanthexGameState>();
		const bool bScore = Match && Match->GetAlphaScore() == 1 && Match->GetBravoScore() == 0;
		SelfTestExpect(bScore, TEXT("score"), FString::Printf(TEXT("%d-%d"), Match ? Match->GetAlphaScore() : -1, Match ? Match->GetBravoScore() : -1));
		SelfTestTicks = 0;
		SelfTestStage = bScore ? EVanthexSelfTestStage::CoreTests : EVanthexSelfTestStage::Finish;
		return;
	}
	case EVanthexSelfTestStage::CoreTests:
		AdvanceCoreSelfTest();
		return;
	case EVanthexSelfTestStage::Match:
	{
		const AVanthexGameState* MatchState = GetGameState<AVanthexGameState>();
		if (MatchState && MatchState->GetPhase() == EVanthexRoundPhase::MatchEnded && MatchState->GetAlphaScore() >= RoundsToWin && MatchState->GetMatchWinner() == EVanthexTeam::Alpha)
		{
			SelfTestExpect(true, TEXT("match"), FString::Printf(TEXT("alpha %d bravo %d"), MatchState->GetAlphaScore(), MatchState->GetBravoScore()));
			SelfTestTicks = 0;
			SelfTestStage = EVanthexSelfTestStage::Equipment;
			return;
		}
		if (MatchState && MatchState->GetPhase() == EVanthexRoundPhase::RoundActive)
		{
			KillTeam(EVanthexTeam::Bravo);
		}
		if (++SelfTestTicks > 400)
		{
			SelfTestExpect(false, TEXT("match"), FString::Printf(TEXT("phase %d score %d-%d"),
				MatchState ? static_cast<int32>(MatchState->GetPhase()) : -1,
				MatchState ? MatchState->GetAlphaScore() : -1,
				MatchState ? MatchState->GetBravoScore() : -1));
			SelfTestStage = EVanthexSelfTestStage::Finish;
		}
		return;
	}
	case EVanthexSelfTestStage::Equipment:
		AdvanceEquipmentSelfTest();
		return;
	case EVanthexSelfTestStage::Finish:
	default:
		FinishSelfTest();
		return;
	}
}


bool AVanthexGameMode::HandOverPlayerSlot(AController* Exiting)
{
 UWorld* World=GetWorld();
 if(!HasAuthority() || !World || World->bIsTearingDown || World->IsBeingCleanedUp() ||
  !Exiting || !Exiting->IsPlayerController() || Exiting->GetWorld()!=World) return false;
 auto* Pawn=Cast<AVanthexCharacter>(Exiting->GetPawn());
 auto* State=Exiting->GetPlayerState<AVanthexPlayerState>();
 // Successful handover unpossesses Exiting, making the later Logout call a no-op.
 if(!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !State || !Combatants.Contains(Pawn) || !Pawn->GetHealthComponent()) return false;
 const auto Team=State->GetTeam();
 const int32 Slot=State->GetSpawnSlot();
 if((Team!=EVanthexTeam::Alpha && Team!=EVanthexTeam::Bravo) || Slot<0 || Slot>=TeamSize()) return false;
 FActorSpawnParameters Params;
 Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Bot=World->SpawnActor<AVanthexBotController>(AVanthexBotController::StaticClass(),Pawn->GetActorTransform(),Params);
 if(!Bot) return false;
 if(!Bot->GetPlayerState<AVanthexPlayerState>()) Bot->InitPlayerState();
 auto* BotState=Bot->GetPlayerState<AVanthexPlayerState>();
 if(!BotState) { Bot->Destroy(); return false; }
 BotState->SetTeam(Team,Slot);
 BotState->SetPlayerName(FString::Printf(TEXT("%s %d"),Team==EVanthexTeam::Bravo?TEXT("Bravo"):TEXT("Alpha"),Slot+1));
 BotState->CopyEconomyFrom(State);
 BotState->SetAlive(!Pawn->GetHealthComponent()->IsDead());
 const FRotator Facing=Exiting->GetControlRotation();
 Exiting->UnPossess();
 Bot->Possess(Pawn);
 Bot->SetControlRotation(Facing);
 if(IsBuyPhase() || Pawn->GetHealthComponent()->IsDead()) {
  Pawn->GetCharacterMovement()->StopMovementImmediately();
  Pawn->GetCharacterMovement()->DisableMovement();
 }
 Pawn->ForceNetUpdate();
 UE_LOG(LogVanthex,Display,TEXT("Player slot handed to bot: team=%d slot=%d pawn=%s credits=%d"),int32(Team),Slot,*Pawn->GetName(),BotState->GetEconomy().Credits);
 return Pawn->GetController()==Bot;
}

void AVanthexGameMode::Logout(AController* Exiting)
{
 HandOverPlayerSlot(Exiting);
 Super::Logout(Exiting);
}

bool AVanthexGameMode::IsCTF() const { return VanthexMap::IsCTF(GetWorld()); }
void AVanthexGameMode::CaptureFlag(EVanthexTeam Team)
{
 if(!HasAuthority()||!IsCTF()||!IsRoundActive()||Team==EVanthexTeam::None)return;
 auto* Enemy=AVanthexFlag::Find(GetWorld(),Team==EVanthexTeam::Alpha?EVanthexTeam::Bravo:EVanthexTeam::Alpha);
 auto* Own=AVanthexFlag::Find(GetWorld(),Team);
 auto* Carrier=Enemy?Enemy->GetCarrier():nullptr;
 auto* State=Carrier?GetVanthexPlayerState(Carrier):nullptr;
 if(!Own||!Own->IsHome()||!State||State->GetTeam()!=Team||Carrier->GetHealthComponent()->IsDead())return;
 const FVector Position=Carrier->GetActorLocation(), Base=Own->Base();
 if(FVector::Dist2D(Position,Base)>=180.f||FMath::Abs(Position.Z-Base.Z)>=160.f)return;
 if(Team==EVanthexTeam::Alpha)++AlphaScore;else ++BravoScore;
 State->RecordFlagCapture();
 Enemy->ReturnHome();PublishMatch();
}

bool AVanthexGameMode::RespawnCTF(AVanthexCharacter* Pawn)
{
 UWorld* World = GetWorld();
 auto* State = Pawn ? GetVanthexPlayerState(Pawn) : nullptr;
 if(!World || !Arena || !State || State->GetTeam() == EVanthexTeam::None) return false;

 const EVanthexTeam Team = State->GetTeam();
 APlayerStart* Start = Arena->GetTeamSpawn(Team, State->GetSpawnSlot());
 if(IsRoundActive())
 {
  // Use the standing capsule since the eliminated pawn may have died crouched.
  const auto* Capsule = Pawn->GetClass()->GetDefaultObject<AVanthexCharacter>()->GetCapsuleComponent();
  const FCollisionShape Shape = Capsule->GetCollisionShape();
  const FCollisionQueryParams Query(SCENE_QUERY_STAT(CTFRespawn), false, Pawn);
  constexpr double DangerDistanceSquared = 1200.0 * 1200.0;
  APlayerStart* SaferStart = nullptr;
  double BestEnemyDistanceSquared = -1.0;
  for(int32 Slot = 0; Slot < TeamSize(); ++Slot)
  {
   auto* Candidate = Arena->GetTeamSpawn(Team, Slot);
   if(!Candidate) continue;

   const FVector Position = Candidate->GetActorLocation();
   if(World->OverlapBlockingTestByProfile(Position, Candidate->GetActorQuat(),
      Capsule->GetCollisionProfileName(), Shape, Query)) continue;

   double NearestEnemyDistanceSquared = TNumericLimits<double>::Max();
   for(const AVanthexCharacter* Other : Combatants)
   {
    if(!IsValid(Other) || Other == Pawn || Other->GetHealthComponent()->IsDead()) continue;
    const auto* OtherState = GetVanthexPlayerState(Other);
    if(!OtherState || OtherState->GetTeam() == Team || OtherState->GetTeam() == EVanthexTeam::None) continue;
    NearestEnemyDistanceSquared = FMath::Min(NearestEnemyDistanceSquared,
      FVector::DistSquared(Position, Other->GetActorLocation()));
   }

   if(Candidate == Start && NearestEnemyDistanceSquared >= DangerDistanceSquared)
   {
    SaferStart = Candidate;
    break;
   }
   if(NearestEnemyDistanceSquared > BestEnemyDistanceSquared
     || (NearestEnemyDistanceSquared == BestEnemyDistanceSquared && Candidate == Start))
   {
    SaferStart = Candidate;
    BestEnemyDistanceSquared = NearestEnemyDistanceSquared;
   }
  }
  Start = SaferStart;
 }
 if(!Start) return false;

 State->GiveCTFLoadout();
 State->SetRespawnTime(0);
 Pawn->PrepareForRound(Start->GetActorTransform());
 RecordPlaytestSpawn(Pawn);
 Pawn->SyncEconomyLoadout(false);
 return true;
}

void AVanthexGameMode::TickCTF()
{
 const float Now = GetWorld()->GetTimeSeconds();
 for(auto It = CTFRespawns.CreateIterator(); It; ++It)
 {
  auto* Pawn = It.Key().Get();
  if(!Pawn)
  {
   It.RemoveCurrent();
   continue;
  }
  if(Now < It.Value()) continue;

  if(RespawnCTF(Pawn))
   It.RemoveCurrent();
  else
   It.Value() = Now + .25f;
 }
}
