#include "VanthexWeaponAudioComponent.h"
#include "VanthexWeaponAudioAcoustics.h"
#include "VanthexPlayerController.h"
#include "Components/AudioComponent.h"
#include "AudioDevice.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"

namespace
{
bool IsReload(EVanthexWeaponAudioEvent E) { return E>=EVanthexWeaponAudioEvent::ReloadStart && E<=EVanthexWeaponAudioEvent::ReloadEnd; }
bool IsUI(EVanthexWeaponAudioEvent E) { return E>=EVanthexWeaponAudioEvent::PurchaseAccepted && E<=EVanthexWeaponAudioEvent::RoundStart; }
uint64 ActionKey(const FVanthexWeaponAudioPacket& P) { return (uint64(P.SourceId)<<32)|P.ActionId; }
}

UVanthexWeaponAudioComponent::UVanthexWeaponAudioComponent()
{
 PrimaryComponentTick.bCanEverTick=false;
}

void UVanthexWeaponAudioComponent::BeginPlay()
{
 Super::BeginPlay();
 Profile=LoadObject<UVanthexWeaponAudioProfile>(nullptr,TEXT("/Game/Vanthex/Audio/Weapons/DA_WeaponAudio.DA_WeaponAudio"));
}

void UVanthexWeaponAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(GetWorld()) for(auto& Timer:ReflectionTimers) GetWorld()->GetTimerManager().ClearTimer(Timer);
 for(auto& V:Voices) if(auto* Audio=V.Audio.Get()) Audio->Stop();
 ReflectionTimers.Reset(); Voices.Reset(); Recent.Reset(); CanceledReloads.Reset();
 Super::EndPlay(Reason);
}

bool UVanthexWeaponAudioComponent::AdmitPacket()
{
 const float Now=GetWorld()->GetTimeSeconds();
 if(Now-SendWindow>=1.f || Now<SendWindow) { SendWindow=Now; WindowPackets=0; }
 if(WindowPackets>=FMath::Clamp(Profile?Profile->MaxPacketsPerSecond:128,16,256)) { ++RateDropped; return false; }
 ++WindowPackets; return true;
}

void UVanthexWeaponAudioComponent::DispatchEvent(AActor* Source,FName WeaponId,EVanthexWeaponAudioEvent Event,uint32 ActionId)
{
 if(!IsInGameThread() || !IsValid(Source) || !Source->HasAuthority() || !Source->GetWorld() || IsUI(Event)
  || uint8(Event)>uint8(EVanthexWeaponAudioEvent::RoundStart) || !ActionId) return;
 UWorld* World=Source->GetWorld();
 const bool bCancel=Event==EVanthexWeaponAudioEvent::ReloadCancel;
 const bool bMovement=Event==EVanthexWeaponAudioEvent::EquipmentMove;
 const auto* MovingCharacter=bMovement?Cast<ACharacter>(Source):nullptr;
 if(bMovement && !MovingCharacter) return;
 const float MaxDistance=Event==EVanthexWeaponAudioEvent::Shot?16000.f:(bMovement?2200.f:2600.f);
 FVector Position=Source->GetActorLocation();
 if(MovingCharacter) Position.Z-=MovingCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-14.f;
 for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator(); It; ++It) {
  auto* PC=Cast<AVanthexPlayerController>(It->Get());
  if(!PC || !PC->GetWeaponAudio() || !PC->GetBulletAudio()) continue;
  auto* Audio=PC->GetWeaponAudio();
  APawn* ListenerPawn=PC->GetBulletAudio()->GetListenerPawn();
  if(!ListenerPawn) continue;
  const FVector Listener=PC->GetBulletAudio()->GetListenerLocation(ListenerPawn);
  if(!bCancel && FVector::DistSquared(Position,Listener)>FMath::Square(MaxDistance)) continue;
  if(!bCancel && !Audio->AdmitPacket()) continue;
  FVanthexWeaponAudioPacket Packet;
  Packet.SourceId=Source->GetUniqueID(); Packet.ActionId=ActionId; Packet.WeaponId=WeaponId; Packet.Event=Event;
  Packet.bOwnSource=Source==ListenerPawn || Source->IsOwnedBy(ListenerPawn);
  if(!bCancel) {
   const auto Route=VanthexWeaponAcoustics::Route(World,Position,Listener,Source);
   if(Route.Distance>MaxDistance) continue;
   Packet.Position=Route.ApparentPosition;
   const float Reference=Event==EVanthexWeaponAudioEvent::Shot?1500.f:450.f;
   Packet.Gain=Route.Transmission/(1.f+FMath::Pow(Route.Distance/Reference,1.35f));
   if(MovingCharacter && MovingCharacter->bIsCrouched) Packet.Gain*=.32f;
   Packet.LowPassHz=FMath::Min(Route.LowPassHz,FMath::Lerp(20000.f,4800.f,FMath::Clamp(Route.Distance/16000.f,0.f,1.f)));
   Packet.ReflectionDelay=Event==EVanthexWeaponAudioEvent::Shot?Route.ReflectionDelay:0.f;
   if(Route.bPortal) ++Audio->PortalRouted;
  }
  ++Audio->SentPackets;
  if(Event==EVanthexWeaponAudioEvent::Shot) ++Audio->SentShotPackets;
  if(bMovement) ++Audio->SentMovementPackets;
  PC->ClientWeaponAudio(Packet);
 }
}

int32 UVanthexWeaponAudioComponent::GetActiveVoiceCount()
{
 Voices.RemoveAll([](const FVoice& V){return !V.Audio.IsValid() || !V.Audio->IsPlaying();});
 return Voices.Num();
}

int32 UVanthexWeaponAudioComponent::EventPriority(EVanthexWeaponAudioEvent Event,bool bOwnSource,bool bRoomTail)
{
 if(bRoomTail) return 1;
 if(IsUI(Event)) return 2;
 if(Event==EVanthexWeaponAudioEvent::Shot) return bOwnSource?8:12;
 if(Event==EVanthexWeaponAudioEvent::KnifeSwing) return bOwnSource?7:11;
 if(Event==EVanthexWeaponAudioEvent::EquipmentMove) return bOwnSource?6:7;
 if(Event==EVanthexWeaponAudioEvent::Mechanic) return 3;
 if(IsReload(Event)) return bOwnSource?4:5;
 return bOwnSource?4:6; // Draw, holster and dry fire carry opponent action information.
}

void UVanthexWeaponAudioComponent::Play(const FVanthexWeaponAudioPacket& P,USoundBase* Sound,float Volume,bool bUI,bool bRoomTail)
{
 if(!Sound || !Profile || (!bUI && (!Profile->Attenuation || !Profile->Concurrency))) { ++MissingAssets; return; }
 if(!FMath::IsFinite(Volume) || Volume<=0.f) return;
 const int32 Priority=EventPriority(P.Event,P.bOwnSource,bRoomTail);
 if(GetActiveVoiceCount()>=FMath::Clamp(Profile->MaxVoices,4,48)) {
  int32 Lowest=0;
  for(int32 I=1; I<Voices.Num(); ++I) if(Voices[I].Priority<Voices[Lowest].Priority) Lowest=I;
  if(Priority<=Voices[Lowest].Priority) { ++ConcurrencyDropped; return; }
  if(auto* Old=Voices[Lowest].Audio.Get()) Old->Stop();
  Voices.RemoveAtSwap(Lowest); ++PriorityPreemptions;
 }
 FRandomStream Random(int32(P.ActionId^P.SourceId^(uint32(P.Event)<<24)));
 UAudioComponent* Audio=nullptr;
 if(bUI) Audio=UGameplayStatics::CreateSound2D(this,Sound,FMath::Clamp(Volume,0.f,1.f),1.f,0.f,Profile->Concurrency,false,true);
 else {
  FVector Position=P.Position;
  if(const auto* PC=Cast<APlayerController>(GetOwner())) {
   FVector Listener,Front,Right; PC->GetAudioListenerPosition(Listener,Front,Right);
   // Avoid UE 5.8's exact X=0 azimuth singularity; authoritative event geometry is unchanged.
   if(FMath::Abs(FVector::DotProduct(Position-Listener,Front))<.001) Position+=Front*.1;
  }
  FAudioDevice::FCreateComponentParams Params(GetWorld(),GetOwner());
  Params.SetLocation(Position); Params.AttenuationSettings=Profile->Attenuation;
  Params.ConcurrencySet.Add(Profile->Concurrency);
  Audio=FAudioDevice::CreateComponent(Sound,Params);
  if(Audio) {
   Audio->SetWorldLocation(Position); Audio->SetVolumeMultiplier(FMath::Clamp(Volume,0.f,1.f));
   Audio->SetPitchMultiplier(Random.FRandRange(.98f,1.02f));
   Audio->bAllowSpatialization=Params.ShouldUseAttenuation(); Audio->bIsUISound=false;
   Audio->bAutoDestroy=true; Audio->bStopWhenOwnerDestroyed=true;
  }
 }
 if(!Audio) { ++ConcurrencyDropped; return; }
 Audio->SetLowPassFilterEnabled(!bUI && P.LowPassHz<19999.f);
 Audio->SetLowPassFilterFrequency(FMath::Clamp(P.LowPassHz,400.f,20000.f));
 // Must be assigned before Play; post-spawn overrides miss the initial engine concurrency decision.
 Audio->bOverridePriority=true; Audio->Priority=float(Priority);
 Audio->Play();
 FVoice Voice; Voice.Audio=Audio; Voice.SourceId=P.SourceId; Voice.ActionId=P.ActionId; Voice.Event=P.Event;
 Voice.Priority=Priority;
 Voices.Add(Voice); ++PlayedSounds;
}

void UVanthexWeaponAudioComponent::Receive(const FVanthexWeaponAudioPacket& P)
{
 const auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 if(!PC || !PC->IsLocalController() || !GetWorld() || GetNetMode()==NM_DedicatedServer) return;
 ++ReceivedPackets;
 if(!P.SourceId || !P.ActionId || IsUI(P.Event) || uint8(P.Event)>uint8(EVanthexWeaponAudioEvent::RoundStart)
  || FVector(P.Position).ContainsNaN() || !FMath::IsFinite(P.Gain) || !FMath::IsFinite(P.LowPassHz)) { ++RateDropped; return; }
 const float Now=GetWorld()->GetTimeSeconds();
 for(auto It=Recent.CreateIterator(); It; ++It) if(Now-It.Value()>8.f) It.RemoveCurrent();
 for(auto It=CanceledReloads.CreateIterator(); It; ++It) if(Now-It.Value()>8.f) It.RemoveCurrent();
 const FString Key=FString::Printf(TEXT("%u:%u:%u"),P.SourceId,P.ActionId,uint32(P.Event));
 if(Recent.Contains(Key)) { ++DuplicateDropped; return; }
 if(Recent.Num()>=1024) { ++RateDropped; return; }
 Recent.Add(Key,Now);
 if(P.Event==EVanthexWeaponAudioEvent::Shot) ++ReceivedShotPackets;
 if(P.Event==EVanthexWeaponAudioEvent::EquipmentMove) ++ReceivedMovementPackets;
 const uint64 ReloadKey=ActionKey(P);
 if(P.Event==EVanthexWeaponAudioEvent::ReloadCancel) {
  CanceledReloads.Add(ReloadKey,Now);
  for(auto& Voice:Voices) if(Voice.SourceId==P.SourceId && Voice.ActionId==P.ActionId && IsReload(Voice.Event))
   if(auto* Audio=Voice.Audio.Get()) { Audio->Stop(); ++CancelStopped; }
  return;
 }
 if(IsReload(P.Event) && CanceledReloads.Contains(ReloadKey)) { ++DuplicateDropped; return; }
 if(!Profile) { ++MissingAssets; return; }
 const FName Weapon=P.WeaponId==TEXT("LegacyRifle")?FName(TEXT("M4")):P.WeaponId;
 const auto* Bank=Profile->Weapons.Find(Weapon);
 const auto* Set=P.Event==EVanthexWeaponAudioEvent::EquipmentMove?&Profile->Movement:(Bank?Bank->Events.Find(P.Event):nullptr);
 if(!Set || Set->Variants.IsEmpty()) { ++MissingAssets; return; }
 FRandomStream Random(int32(P.ActionId^P.SourceId^uint32(P.Event)));
 USoundBase* Sound=Set->Variants[Random.RandRange(0,Set->Variants.Num()-1)];
 Play(P,Sound,Set->Volume*FMath::Clamp(P.Gain,0.f,1.f));
 if(P.Event==EVanthexWeaponAudioEvent::Shot && P.ReflectionDelay>0.f && FMath::IsFinite(P.ReflectionDelay) && Profile->RoomTail) {
  ReflectionTimers.RemoveAll([this](const FTimerHandle& H){return !GetWorld()->GetTimerManager().IsTimerActive(H);});
  if(ReflectionTimers.Num()<16) {
   FTimerHandle Timer;
   GetWorld()->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(this,[this,P](){
    if(Profile) Play(P,Profile->RoomTail,P.Gain*.065f,false,true);
   }),FMath::Clamp(P.ReflectionDelay,.012f,.045f),false);
   ReflectionTimers.Add(Timer);
  }
 }
}

void UVanthexWeaponAudioComponent::PlayUI(EVanthexWeaponAudioEvent Event)
{
 const auto* PC=Cast<AVanthexPlayerController>(GetOwner());
 if(!PC || !PC->IsLocalController() || !GetWorld() || GetNetMode()==NM_DedicatedServer || !IsUI(Event)) return;
 const auto* Set=Profile?Profile->UI.Find(Event):nullptr;
 if(!Set || Set->Variants.IsEmpty()) { ++MissingAssets; return; }
 FVanthexWeaponAudioPacket Packet; Packet.Event=Event;
 Play(Packet,Set->Variants[0],Set->Volume,true);
}
