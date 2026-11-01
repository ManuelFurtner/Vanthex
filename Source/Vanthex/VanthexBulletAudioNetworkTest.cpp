#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "VanthexBulletAudioComponent.h"
#include "VanthexBulletAudioProfile.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexBotController.h"
#include "HitscanWeapon.h"
#include "HealthComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
struct FBulletAudioNetworkState
{
	FTimerHandle Timer;
	double ReadyTime = -1.;
	double FirstReceivedTime = -1.;
	int32 Phase = 0;
	int32 BurstSteps = 0;
	bool bMissingSoundsSafe = false;
	TArray<TWeakObjectPtr<AVanthexPlayerController>> Humans;
	TArray<TWeakObjectPtr<AVanthexCharacter>> Bots;
};

void SaveAudioNetworkReport(const FString& Suffix, const FString& Report)
{
	FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / (TEXT("BulletAudioNetwork-") + Suffix + TEXT(".txt"))));
}

AActor* MakeAudioTestWall(UWorld* World, FVector Location, FVector Extent)
{
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->RegisterComponent();
	Wall->SetActorLocation(Location);
	return Wall;
}

FVector TestListenerLocation(AVanthexCharacter* Listener)
{
	if (const auto* PC = Cast<AVanthexPlayerController>(Listener->GetController()))
		if (const auto* Audio = PC->GetBulletAudio()) return Audio->GetListenerLocation(Listener);
	return Listener->GetPawnViewLocation();
}

void ShootNearListener(AVanthexCharacter* Shooter, AVanthexCharacter* Listener, int32 Offset = 0)
{
	if (!Shooter || !Listener || !Shooter->GetWeapon() || !Shooter->GetController()) return;
	const FVector ViewOffset = Shooter->GetPawnViewLocation() - Shooter->GetActorLocation();
	const FVector ShotStart = TestListenerLocation(Listener) + FVector(-1200. - 150. * Offset, 80. * Offset, 100.);
	Shooter->SetActorLocation(ShotStart - ViewOffset, false, nullptr, ETeleportType::TeleportPhysics);
	Shooter->GetController()->SetControlRotation(FRotator::ZeroRotator);
	Shooter->ForceNetUpdate();
	Shooter->GetWeapon()->FireShot();
}
}
#endif

// Explicitly opt-in, event/timer driven, and excluded from Shipping builds.
void VanthexStartBulletAudioNetworkTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
	if (!Controller || !FParse::Param(FCommandLine::Get(), TEXT("BulletAudioNetworkTest")) || !Controller->IsLocalController()) return;
	UWorld* World = Controller->GetWorld();
	const TWeakObjectPtr<AVanthexPlayerController> WeakController(Controller);
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	const TSharedPtr<FBulletAudioNetworkState> State = MakeShared<FBulletAudioNetworkState>();
	World->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([WeakController, WeakWorld, State]()
	{
		if (!WeakController.IsValid() || !WeakWorld.IsValid()) return;
		AVanthexPlayerController* PC = WeakController.Get();
		UWorld* W = WeakWorld.Get();
		UVanthexBulletAudioComponent* Audio = PC->GetBulletAudio();
		AVanthexCharacter* OwnCharacter = Cast<AVanthexCharacter>(PC->GetPawn());
		if (!Audio || !OwnCharacter || !Audio->GetProfile()) return;
		const double Now = W->GetTimeSeconds();
		if (!PC->HasAuthority())
		{
			if (Audio->NearEvents && State->FirstReceivedTime < 0.) State->FirstReceivedTime = Now;
			if (State->FirstReceivedTime < 0. || Now - State->FirstReceivedTime < 12.) return;
			int32 Characters = 0;
			for (TActorIterator<AVanthexCharacter> It(W); It; ++It) ++Characters;
			const auto* PS = PC->GetPlayerState<AVanthexPlayerState>();
			const bool bPass = Characters == 6 && Audio->NearEvents == 1 && Audio->ReceivedPackets == 1 && Audio->ImpactEvents == 0 && OwnCharacter->GetHealthComponent()->GetHealth() == 100.f;
			int32 Role = 0;
			FParse::Value(FCommandLine::Get(), TEXT("BulletAudioTestRole="), Role);
			SaveAudioNetworkReport(FString::Printf(TEXT("Client%d"), Role), FString::Printf(TEXT("%s role=client%d characters=%d team=%d near=%u received=%u impact=%u health=%.0f\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Role, Characters, PS ? int32(PS->GetTeam()) : -1, Audio->NearEvents, Audio->ReceivedPackets, Audio->ImpactEvents, OwnCharacter->GetHealthComponent()->GetHealth()));
			W->GetTimerManager().ClearTimer(State->Timer);
			return;
		}
		if (State->ReadyTime < 0.)
		{
			State->Humans.Reset();
			State->Bots.Reset();
			State->Humans.Add(PC);
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				AVanthexPlayerController* Other = Cast<AVanthexPlayerController>(It->Get());
				if (Other && Other != PC && Other->GetPawn()) State->Humans.Add(Other);
			}
			if (State->Humans.Num() != 3) return;
			for (TActorIterator<AVanthexCharacter> It(W); It; ++It)
			{
				if (Cast<AVanthexBotController>(It->GetController())) State->Bots.Add(*It);
				It->GetCharacterMovement()->StopMovementImmediately();
				It->GetCharacterMovement()->DisableMovement();
				if (It->GetWeapon()) It->GetWeapon()->StopFire();
				It->SetActorLocation(FVector(50000., -5000. - State->Bots.Num() * 1000., 5000.), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (State->Bots.Num() != 3) return;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				APawn* Pawn = State->Humans[Index]->GetPawn();
				Pawn->SetActorLocation(FVector(50000., Index * 2500., 5000.), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->ForceNetUpdate();
			}
			State->ReadyTime = Now;
			return;
		}
		const double Age = Now - State->ReadyTime;
		if (State->Phase < 3 && Age >= State->Phase + 1.)
		{
			ShootNearListener(State->Bots[0].Get(), Cast<AVanthexCharacter>(State->Humans[State->Phase]->GetPawn()));
			++State->Phase;
		}
		else if (State->Phase == 3 && Age >= 4.)
		{
			AVanthexCharacter* Listener = Cast<AVanthexCharacter>(State->Humans[1]->GetPawn());
			AActor* Wall = MakeAudioTestWall(W, TestListenerLocation(Listener) + FVector(0., 0., 50.), FVector(300., 300., 5.));
			ShootNearListener(State->Bots[0].Get(), Listener);
			Wall->Destroy();
			++State->Phase;
		}
		else if (State->Phase == 4 && Age >= 5.)
		{
			AVanthexCharacter* Listener = Cast<AVanthexCharacter>(State->Humans[2]->GetPawn());
			AActor* Wall = MakeAudioTestWall(W, TestListenerLocation(Listener) + FVector(-100., 0., 100.), FVector(5., 300., 200.));
			ShootNearListener(State->Bots[0].Get(), Listener);
			Wall->Destroy();
			++State->Phase;
		}
		else if (State->Phase == 5 && Age >= 6. && State->BurstSteps < 40)
		{
			for (int32 Index = 0; Index < State->Bots.Num(); ++Index) ShootNearListener(State->Bots[Index].Get(), OwnCharacter, Index);
			++State->BurstSteps;
		}
		else if (State->Phase == 5 && State->BurstSteps == 40 && Age >= 10.)
		{
			// H: use the actual router with an empty sound library; semantic events and gameplay remain valid.
			UVanthexBulletAudioProfile* Profile = const_cast<UVanthexBulletAudioProfile*>(Audio->GetProfile());
			const auto SavedEvents = Profile->Events;
			const auto SavedImpacts = Profile->Impacts;
			Profile->Events.Empty();
			Profile->Impacts.Empty();
			const uint32 Before = Audio->NearEvents;
			const uint32 PlayedBefore = Audio->PlayedSounds;
			ShootNearListener(State->Bots[0].Get(), OwnCharacter);
			State->bMissingSoundsSafe = Audio->NearEvents == Before + 1 && Audio->PlayedSounds == PlayedBefore;
			Profile->Events = SavedEvents;
			Profile->Impacts = SavedImpacts;
			++State->Phase;
		}
		else if (State->Phase == 6 && Age >= 11.)
		{
			const uint32 Client1Sent = State->Humans[1]->GetBulletAudio()->SentPackets;
			const uint32 Client2Sent = State->Humans[2]->GetBulletAudio()->SentPackets;
			bool bHealthy = true;
			int32 Characters = 0;
			for (TActorIterator<AVanthexCharacter> It(W); It; ++It) { ++Characters; bHealthy &= It->GetHealthComponent()->GetHealth() == 100.f; }
			const bool bPass = Characters == 6 && bHealthy && State->bMissingSoundsSafe && Audio->NearEvents > 2 && Audio->NearEvents <= 25 && Audio->SentPackets == Audio->ReceivedPackets && Client1Sent == 1 && Client2Sent == 1 && Audio->ImpactEvents == 0;
			SaveAudioNetworkReport(TEXT("Host"), FString::Printf(TEXT("%s characters=%d humanClients=3 bots=3 near=%u received=%u sent=%u impact=%u farClient1Sent=%u farClient2Sent=%u burstSteps=%d missingSoundsSafe=%d healthy=%d\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Characters, Audio->NearEvents, Audio->ReceivedPackets, Audio->SentPackets, Audio->ImpactEvents, Client1Sent, Client2Sent, State->BurstSteps, State->bMissingSoundsSafe, bHealthy));
			W->GetTimerManager().ClearTimer(State->Timer);
		}
	}), .05f, true);
#endif
}
