#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "VanthexBulletAudioComponent.h"
#include "VanthexCharacter.h"
#include "HitscanWeapon.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Vanthex.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
struct FBulletPlaybackState
{
	FTimerHandle Timer;
	double ReadyTime = -1.;
	int32 Phase = 0;
	FDateTime PreviousWavTimestamp;
};
}
#endif

void VanthexStartBulletAudioPlaybackTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
	if (!Controller || !Controller->IsLocalController() || !Controller->HasAuthority() || !FParse::Param(FCommandLine::Get(), TEXT("BulletAudioPlaybackTest"))) return;
	const TWeakObjectPtr<AVanthexPlayerController> WeakPC(Controller);
	const TSharedPtr<FBulletPlaybackState> State = MakeShared<FBulletPlaybackState>();
	Controller->GetWorld()->GetTimerManager().SetTimer(State->Timer, FTimerDelegate::CreateLambda([WeakPC, State]()
	{
		if (!WeakPC.IsValid()) return;
		AVanthexPlayerController* PC = WeakPC.Get();
		UWorld* World = PC->GetWorld();
		UVanthexBulletAudioComponent* Audio = PC->GetBulletAudio();
		AVanthexCharacter* Character = Cast<AVanthexCharacter>(PC->GetPawn());
		if (!Audio || !Audio->GetProfile() || !Character) return;
		const double Now = World->GetTimeSeconds();
		if (State->ReadyTime < 0.)
		{
			for (TActorIterator<AVanthexCharacter> It(World); It; ++It)
			{
				It->GetCharacterMovement()->StopMovementImmediately();
				It->GetCharacterMovement()->DisableMovement();
				if (It->GetWeapon()) It->GetWeapon()->StopFire();
			}
			for (TObjectIterator<UAudioComponent> It; It; ++It) if (It->GetWorld() == World) It->Stop();
			Character->SetActorLocation(FVector(50000., 0., 5000.), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator::ZeroRotator);
			State->ReadyTime = Now;
			return;
		}
		const double Age = Now - State->ReadyTime;
		if (State->Phase == 0 && Age >= 1.)
		{
			State->PreviousWavTimestamp = IFileManager::Get().GetTimeStamp(*(FPaths::ProjectSavedDir() / TEXT("BulletAudioPlayback.wav")));
			UAudioMixerBlueprintLibrary::StartRecordingOutput(PC, 6.f);
			++State->Phase;
		}
		else if (State->Phase >= 1 && State->Phase <= 3 && Age >= State->Phase + 1.)
		{
			FVanthexBulletAudioPacket Packet;
			Packet.Sequence = uint32(State->Phase);
			const FVector Listener = Audio->GetListenerLocation(Character);
			FVector EngineListener, Front, Right;
			PC->GetAudioListenerPosition(EngineListener, Front, Right);
			UE_LOG(LogVanthex, Display, TEXT("BULLET_MIXER phase=%d router=%s listener=%s front=%s right=%s"), State->Phase, *Listener.ToString(), *EngineListener.ToString(), *Front.ToString(), *Right.ToString());
			if (State->Phase <= 2)
			{
				Packet.Band = uint8(VanthexBulletAudio::EBand::Close);
				Packet.NearPoint = Listener + FVector(0., State->Phase == 1 ? -30. : 30., 0.);
			}
			else
			{
				Packet.bImpact = true;
				Packet.ImpactPoint = Listener + FVector(100., 0., 0.);
				Packet.Surface = TEXT("Steel");
			}
			Audio->Receive(Packet);
			if (State->Phase == 2)
			{
				Audio->Receive(Packet); // Repeated delivery must not play again.
				Packet.Sequence = 1;
				Audio->Receive(Packet); // Late packet must not rewind the sequence.
			}
			++State->Phase;
		}
		else if (State->Phase == 4 && Age >= 6.)
		{
			UAudioMixerBlueprintLibrary::StopRecordingOutput(PC, EAudioRecordingExportType::WavFile, TEXT("BulletAudioPlayback"), FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));
			++State->Phase;
		}
		else if (State->Phase == 5 && Age >= 8.)
		{
			const FString WavPath = FPaths::ProjectSavedDir() / TEXT("BulletAudioPlayback.wav");
			const int64 Bytes = IFileManager::Get().FileSize(*WavPath);
			const bool bFresh = IFileManager::Get().GetTimeStamp(*WavPath) != State->PreviousWavTimestamp;
			const bool bPass = Audio->PlayedSounds == 3 && Audio->NearEvents == 2 && Audio->ImpactEvents == 1 && Audio->DroppedPackets == 2 && Bytes > 44 && bFresh;
			const FString Report = FString::Printf(TEXT("%s near=%u impact=%u played=%u duplicatesDropped=%u wavBytes=%lld\nMixer waveform must additionally be checked for nonzero samples and left/right direction.\n"), bPass ? TEXT("PASS") : TEXT("FAIL"), Audio->NearEvents, Audio->ImpactEvents, Audio->PlayedSounds, Audio->DroppedPackets, Bytes);
			FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("BulletAudioPlayback.txt")));
			World->GetTimerManager().ClearTimer(State->Timer);
			FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
		}
	}), .05f, true);
#endif
}
