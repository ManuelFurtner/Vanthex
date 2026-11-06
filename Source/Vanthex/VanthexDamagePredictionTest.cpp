#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Vanthex.h"
#include "VanthexBotController.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

namespace
{
const FVector ShooterPosition(50000, 0, 5000);
const FVector TargetPosition(50400, 0, 5000);

enum class EPredictionTestStage
{
	Setup,
	ConfirmHit,
	PrepareMiss,
	ConfirmMiss,
	CheckMissHealth
};

struct FDamagePredictionProbe
{
	FTimerHandle Timer;
	TWeakObjectPtr<AVanthexCharacter> Target;
	EPredictionTestStage Stage = EPredictionTestStage::Setup;
	double Started = 0.0;
	double ContinueAt = 0.0;
	float InitialHealth = 0.f;
	float HealthAfterHit = 0.f;
	uint32 Burst = 0;
	int32 AmmoBeforeMiss = 0;
	bool bConfirmedHit = false;
	bool bConfirmedMiss = false;
};

bool AimAtTorso(AVanthexPlayerController* PC, const AVanthexCharacter* Shooter,
	const AVanthexCharacter* Target)
{
	const auto* Body = Target->GetMesh()->GetBodyInstance(TEXT("pelvis"));
	if (!Body)
	{
		return false;
	}
	PC->SetControlRotation((Body->GetCOMPosition() - Shooter->GetPawnViewLocation()).Rotation());
	return true;
}

AVanthexCharacter* PrepareServerRange(AVanthexCharacter* Shooter)
{
	auto* World = Shooter->GetWorld();
	auto* Mode = World->GetAuthGameMode<AVanthexGameMode>();
	if (!Mode)
	{
		return nullptr;
	}
	AVanthexCharacter* Target = nullptr;
	for (TActorIterator<AVanthexCharacter> It(World); It; ++It)
	{
		if (It->GetHealthComponent() && !It->GetHealthComponent()->IsDead()
			&& Mode->CanDamage(Shooter, *It))
		{
			Target = *It;
			break;
		}
	}
	if (!Target)
	{
		return nullptr;
	}

	int32 Parked = 0;
	for (TActorIterator<AVanthexCharacter> It(World); It; ++It)
	{
		if (auto* Bot = Cast<AVanthexBotController>(It->GetController()))
		{
			Bot->SetActorTickEnabled(false);
		}
		if (auto* Weapon = It->GetWeapon())
		{
			Weapon->StopFire();
		}
		It->GetCharacterMovement()->StopMovementImmediately();
		It->GetCharacterMovement()->DisableMovement();
		const FVector Position = *It == Shooter ? ShooterPosition
			: *It == Target ? TargetPosition : FVector(48000, 1500 + 350 * Parked++, 5000);
		It->SetActorLocationAndRotation(Position, FRotator::ZeroRotator, false, nullptr,
			ETeleportType::TeleportPhysics);
		It->ForceNetUpdate();
	}
	Target->GetHealthComponent()->ResetHealth();
	return Target;
}

bool AddServerBlocker(UWorld* World)
{
	auto* Wall = World->SpawnActor<AActor>();
	if (!Wall)
	{
		return false;
	}
	Wall->SetReplicates(false);
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(25, 120, 160));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation((ShooterPosition + TargetPosition) * .5f);
	return true;
}
}
#endif

void VanthexStartDamagePredictionTest(AVanthexPlayerController* PC)
{
#if !UE_BUILD_SHIPPING
	if (!PC || !PC->GetWorld() || !FParse::Param(FCommandLine::Get(), TEXT("DamagePredictionTest")))
	{
		return;
	}
	const bool bServer = PC->HasAuthority();
	if (bServer == PC->IsLocalController())
	{
		return;
	}

	auto State = MakeShared<FDamagePredictionProbe>();
	State->Started = FPlatformTime::Seconds();
	PC->GetWorld()->GetTimerManager().SetTimer(State->Timer,
		FTimerDelegate::CreateWeakLambda(PC, [PC, State, bServer]()
		{
			const auto KeepAlive = State;
			auto& Probe = *KeepAlive;
			auto* World = PC->GetWorld();
			const double Now = FPlatformTime::Seconds();
			auto Finish = [&](bool bPass, const TCHAR* Detail)
			{
				const FString Report = FString::Printf(
					TEXT("%s %s stage=%d initial_hp=%.0f hit_hp=%.0f\n"),
					bPass ? TEXT("PASS") : TEXT("FAIL"), Detail, int32(Probe.Stage),
					Probe.InitialHealth, Probe.HealthAfterHit);
				UE_LOG(LogVanthex, Display, TEXT("DAMAGE_PREDICTION %s"), *Report);
				World->GetTimerManager().ClearTimer(Probe.Timer);
				if (!bServer)
				{
					const bool bSaved = FFileHelper::SaveStringToFile(Report,
						*(FPaths::ProjectSavedDir() / TEXT("DamagePredictionTest.txt")));
					FPlatformMisc::RequestExitWithStatus(false, bPass && bSaved ? 0 : 1);
				}
			};
			if (Now - Probe.Started > 45.0)
			{
				Finish(false, TEXT("timed out awaiting test state"));
				return;
			}

			auto* Shooter = Cast<AVanthexCharacter>(PC->GetPawn());
			auto* Weapon = Shooter ? Shooter->GetWeapon() : nullptr;
			if (!Weapon || !Shooter->CanUseRifle() || Weapon->GetWeaponId() != TEXT("Pistol"))
			{
				return;
			}
			if (bServer)
			{
				if (!Probe.Target.IsValid())
				{
					Probe.Target = PrepareServerRange(Shooter);
					if (auto* Target = Probe.Target.Get())
					{
						Probe.InitialHealth = Target->GetHealthComponent()->GetHealth();
					}
				}
				if (auto* Target = Probe.Target.Get())
				{
					AimAtTorso(PC, Shooter, Target);
					if (Probe.Stage == EPredictionTestStage::Setup
						&& Target->GetHealthComponent()->GetHealth() < Probe.InitialHealth)
					{
						if (!AddServerBlocker(World))
						{
							Finish(false, TEXT("failed to create authority-only blocker"));
							return;
						}
						Probe.Stage = EPredictionTestStage::PrepareMiss;
						UE_LOG(LogVanthex, Display, TEXT("DAMAGE_PREDICTION server applied first hit and placed blocker"));
					}
				}
				return;
			}

			if (!PC->CanPredictDamageFeedback() || !Shooter->GetActorLocation().Equals(ShooterPosition, 10.f))
			{
				return;
			}
			if (!Probe.Target.IsValid())
			{
				for (TActorIterator<AVanthexCharacter> It(World); It; ++It)
				{
					if (It->GetActorLocation().Equals(TargetPosition, 10.f))
					{
						Probe.Target = *It;
						break;
					}
				}
			}
			auto* Target = Probe.Target.Get();
			auto* Health = Target ? Target->GetHealthComponent() : nullptr;
			if (!Health)
			{
				return;
			}
			if (Probe.Stage == EPredictionTestStage::Setup || Probe.Stage == EPredictionTestStage::PrepareMiss)
			{
				const auto* ShooterState = Shooter->GetPlayerState<AVanthexPlayerState>();
				const auto* TargetState = Target->GetPlayerState<AVanthexPlayerState>();
				if (Now < Probe.ContinueAt || !ShooterState || !TargetState || !TargetState->IsAlive()
					|| ShooterState->GetTeam() == EVanthexTeam::None || TargetState->GetTeam() == EVanthexTeam::None
					|| ShooterState->GetTeam() == TargetState->GetTeam() || !AimAtTorso(PC, Shooter, Target))
				{
					return;
				}
				const bool bMiss = Probe.Stage == EPredictionTestStage::PrepareMiss;
				const float HealthBefore = Health->GetHealth();
				if (bMiss)
				{
					Probe.AmmoBeforeMiss = Weapon->GetAmmoInMagazine();
				}
				else
				{
					Probe.InitialHealth = HealthBefore;
				}

				Probe.Burst = Weapon->StartPredictedFire();
				if (!Probe.Burst || !PC->IsShowingPredictedHit() || PC->IsShowingConfirmedHit()
					|| Health->GetHealth() != HealthBefore)
				{
					Finish(false, TEXT("expected immediate cosmetic prediction without damage before the RPC"));
					return;
				}
				Probe.Stage = bMiss ? EPredictionTestStage::ConfirmMiss : EPredictionTestStage::ConfirmHit;
				Shooter->ServerStartPredictedFire(Probe.Burst);
				Weapon->StopPredictedFire();
				Shooter->ServerAction(1, false);
				return;
			}

			if (Probe.Stage == EPredictionTestStage::ConfirmHit)
			{
				if (!Probe.bConfirmedHit && !PC->CanPredictShot(Probe.Burst, 1))
				{
					if (PC->IsShowingPredictedHit() || !PC->IsShowingConfirmedHit())
					{
						Finish(false, TEXT("authority did not replace the first prediction with a hit"));
						return;
					}
					Probe.bConfirmedHit = true;
				}
				if (Probe.bConfirmedHit && Health->GetHealth() < Probe.InitialHealth)
				{
					if (Health->IsDead())
					{
						Finish(false, TEXT("first shot killed the target before the miss check"));
						return;
					}
					Probe.HealthAfterHit = Health->GetHealth();
					Probe.ContinueAt = Now + .5;
					Probe.Stage = EPredictionTestStage::PrepareMiss;
				}
				return;
			}

			if (Health->GetHealth() != Probe.HealthAfterHit)
			{
				Finish(false, TEXT("blocked shot changed replicated health"));
				return;
			}
			if (Probe.Stage == EPredictionTestStage::ConfirmMiss)
			{
				if (!Probe.bConfirmedMiss && !PC->CanPredictShot(Probe.Burst, 1))
				{
					if (PC->IsShowingPredictedHit() || PC->IsShowingConfirmedHit())
					{
						Finish(false, TEXT("authority miss did not clear the predicted marker"));
						return;
					}
					Probe.bConfirmedMiss = true;
				}
				if (Probe.bConfirmedMiss && Weapon->GetAmmoInMagazine() == Probe.AmmoBeforeMiss - 1)
				{
					Probe.ContinueAt = Now + .5;
					Probe.Stage = EPredictionTestStage::CheckMissHealth;
				}
			}
			else if (Now >= Probe.ContinueAt)
			{
				Finish(true, TEXT("immediate client prediction, confirmed damage, and blocked shot correction"));
			}
		}), .02f, true, .2f);
#endif
}
