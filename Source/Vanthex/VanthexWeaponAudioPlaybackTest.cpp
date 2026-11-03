#include "VanthexPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "VanthexCharacter.h"
#include "HitscanWeapon.h"
#include "AudioMixerBlueprintLibrary.h"
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
struct FProbe
{
 FTimerHandle Timer;
 double Start=-1., Deadline=0.; int32 Stage=0;
 uint32 PlayedBefore=0;
 FDateTime Prior;
 FVanthexWeaponAudioPacket First;
};
}
#endif

void VanthexStartWeaponAudioPlaybackTest(AVanthexPlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
 if(!Controller || !Controller->IsLocalController() || !Controller->HasAuthority()) return;
 TWeakObjectPtr<AVanthexPlayerController> Weak=Controller;
 auto S=MakeShared<FProbe>(); S->Deadline=FPlatformTime::Seconds()+45.;
 Controller->GetWorld()->GetTimerManager().SetTimer(S->Timer,FTimerDelegate::CreateLambda([Weak,S](){
  if(!Weak.IsValid()) return;
  auto* PC=Weak.Get(); auto* W=PC->GetWorld(); auto* Audio=PC->GetWeaponAudio();
  auto* Character=Cast<AVanthexCharacter>(PC->GetPawn());
  if(FPlatformTime::Seconds()>S->Deadline) {
   FFileHelper::SaveStringToFile(TEXT("FAIL weapon audio playback timed out\n"),*(FPaths::ProjectSavedDir()/TEXT("WeaponAudioPlayback.txt")));
   W->GetTimerManager().ClearTimer(S->Timer); FPlatformMisc::RequestExitWithStatus(false,1); return;
  }
  if(!Character || !Audio || !Audio->GetProfile()) return;
  const double Now=W->GetTimeSeconds();
  if(S->Start<0.) {
   for(TActorIterator<AVanthexCharacter> It(W); It; ++It) {
    It->GetCharacterMovement()->StopMovementImmediately(); It->GetCharacterMovement()->DisableMovement();
    if(It->GetWeapon()) It->GetWeapon()->StopFire();
   }
   for(TObjectIterator<UAudioComponent> It; It; ++It) if(It->GetWorld()==W) It->Stop();
   Character->SetActorLocation(FVector(50000.,0.,5000.),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation(FRotator::ZeroRotator); S->Start=Now;
   return;
  }
  const double Age=Now-S->Start;
  if(S->Stage==0 && Age>=1.) {
   // Exclude initial draw/buy-phase UI sounds emitted during the first startup ticks.
   S->PlayedBefore=Audio->PlayedSounds;
   S->Prior=IFileManager::Get().GetTimeStamp(*(FPaths::ProjectSavedDir()/TEXT("WeaponAudioPlayback.wav")));
   UAudioMixerBlueprintLibrary::StartRecordingOutput(PC,10.f); ++S->Stage;
  } else if(S->Stage>=1 && S->Stage<=7 && Age>=S->Stage+1.) {
   FVector Listener,Front,Right; PC->GetAudioListenerPosition(Listener,Front,Right);
   FVanthexWeaponAudioPacket P; P.SourceId=991; P.ActionId=S->Stage;
   P.WeaponId=S->Stage==2?TEXT("MP7"):(S->Stage==3?TEXT("Pistol"):TEXT("AUG"));
   P.Event=EVanthexWeaponAudioEvent::Shot; P.Gain=.75f;
   const FVector Directions[]={-Right,Right,Front,-Front,FVector::UpVector,-FVector::UpVector,Front};
   P.Position=Listener+Directions[S->Stage-1]*300.;
   if(S->Stage==7) { P.Gain=.075f; P.LowPassHz=850.f; }
   Audio->Receive(P); S->First=P;
   ++S->Stage;
  } else if(S->Stage==8 && Age>=10.) {
   UAudioMixerBlueprintLibrary::StopRecordingOutput(PC,EAudioRecordingExportType::WavFile,TEXT("WeaponAudioPlayback"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));
   ++S->Stage;
  } else if(S->Stage==9 && Age>=12.) {
   const uint32 Rendered=Audio->PlayedSounds-S->PlayedBefore;
   bool Pass=Rendered==7;
   FString Report=FString::Printf(TEXT("%s renderedEvents=%u expected=7\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Rendered);
   auto Check=[&](const TCHAR* Name,bool Ok){Pass&=Ok; Report+=FString::Printf(TEXT("%s %s\n"),Ok?TEXT("PASS"):TEXT("FAIL"),Name);};
   const uint32 DupBefore=Audio->DuplicateDropped; Audio->Receive(S->First);
   Check(TEXT("duplicate delivers no second sound"),Audio->DuplicateDropped==DupBefore+1 && Audio->PlayedSounds-S->PlayedBefore==Rendered);
   auto P=S->First; P.ActionId=100; P.Event=EVanthexWeaponAudioEvent::ReloadMagazineOut;
   Audio->Receive(P); const uint32 Canceled=Audio->CancelStopped;
   P.Event=EVanthexWeaponAudioEvent::ReloadCancel; Audio->Receive(P);
   Check(TEXT("reload cancel stops current phase voice"),Audio->CancelStopped>Canceled);
   const uint32 Played=Audio->PlayedSounds; P.Event=EVanthexWeaponAudioEvent::ReloadMagazineIn; Audio->Receive(P);
   Check(TEXT("late reload phase after cancel cannot play"),Audio->PlayedSounds==Played);
   const uint32 Missing=Audio->MissingAssets; P.ActionId=101; P.Event=EVanthexWeaponAudioEvent::Shot; P.WeaponId=TEXT("NoSuchWeapon"); Audio->Receive(P);
   Check(TEXT("missing asset is counted without fallback false shot"),Audio->MissingAssets==Missing+1);
   const uint32 MovementBefore=Audio->ReceivedMovementPackets, ShotBeforeMovement=Audio->ReceivedShotPackets, PlayedBeforeMovement=Audio->PlayedSounds;
   P.ActionId=102; P.Event=EVanthexWeaponAudioEvent::EquipmentMove; P.WeaponId=NAME_None; Audio->Receive(P);
   Check(TEXT("shared foot and gear action plays without a weapon or armor bank"),Audio->ReceivedMovementPackets==MovementBefore+1 && Audio->PlayedSounds==PlayedBeforeMovement+1);
   Check(TEXT("movement cannot satisfy the confirmed shot network counter"),Audio->ReceivedShotPackets==ShotBeforeMovement);
   P.WeaponId=TEXT("AUG"); P.Gain=.001f; P.Event=EVanthexWeaponAudioEvent::Mechanic; P.bOwnSource=true;
   const uint32 CapBefore=Audio->ConcurrencyDropped;
   for(uint32 I=200; I<248; ++I) {P.ActionId=I; Audio->Receive(P);}
   Check(TEXT("concurrent source budget is enforced"),Audio->ConcurrencyDropped>CapBefore && Audio->GetActiveVoiceCount()<=Audio->GetProfile()->MaxVoices);
   const uint32 PreemptBefore=Audio->PriorityPreemptions, BeforeEnemy=Audio->PlayedSounds;
   P.ActionId=250; P.Event=EVanthexWeaponAudioEvent::Shot; P.bOwnSource=false; Audio->Receive(P);
   Check(TEXT("opponent shot displaces own mechanics at full voice budget"),Audio->PriorityPreemptions==PreemptBefore+1 && Audio->PlayedSounds==BeforeEnemy+1 && Audio->GetActiveVoiceCount()<=Audio->GetProfile()->MaxVoices);
   const uint32 BeforeEqual=Audio->ConcurrencyDropped;
   P.ActionId=251; P.Event=EVanthexWeaponAudioEvent::Mechanic; P.bOwnSource=true; Audio->Receive(P);
   Check(TEXT("equal low priority stays capped after danger preemption"),Audio->ConcurrencyDropped==BeforeEqual+1 && Audio->PriorityPreemptions==PreemptBefore+1);
   AActor* Source=W->SpawnActor<AActor>();
   auto* SourceRoot=NewObject<USceneComponent>(Source); Source->SetRootComponent(SourceRoot); SourceRoot->RegisterComponent();
   Source->SetActorLocation(Character->GetActorLocation());
   const uint32 Sent=Audio->SentPackets,Received=Audio->ReceivedPackets;
   UVanthexWeaponAudioComponent::DispatchEvent(Source,TEXT("AUG"),EVanthexWeaponAudioEvent::Shot,333);
   Check(TEXT("server confirmed event reaches local owning listener"),Audio->SentPackets==Sent+1 && Audio->ReceivedPackets==Received+1);
   Source->SetActorLocation(FVector(-50000.,0.,5000.));
   UVanthexWeaponAudioComponent::DispatchEvent(Source,TEXT("AUG"),EVanthexWeaponAudioEvent::Shot,334);
   Check(TEXT("inaudible source does not send packet"),Audio->SentPackets==Sent+1);
   Source->Destroy();
   const FString Wav=FPaths::ProjectSavedDir()/TEXT("WeaponAudioPlayback.wav");
   Check(TEXT("fresh actual mixer recording exists"),IFileManager::Get().FileSize(*Wav)>44 && IFileManager::Get().GetTimeStamp(*Wav)!=S->Prior);
   Report+=TEXT("Analyze waveform with AudioSource/Weapons/analyze_playback.py; stereo proves left/right only, not HRTF elevation localization.\n");
   FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("WeaponAudioPlayback.txt")));
   W->GetTimerManager().ClearTimer(S->Timer); FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
  }
 }),.05f,true);
#endif
}
