#include "VanthexBulletAudioComponent.h"
#include "VanthexBulletAudioProfile.h"
#include "VanthexBulletAudioSettings.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexCharacter.h"
#include "HealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"

namespace VanthexBulletAudio
{
 UE_DEFINE_GAMEPLAY_TAG(NearClose, "Bullet.NearMiss.Close");
 UE_DEFINE_GAMEPLAY_TAG(NearMedium, "Bullet.NearMiss.Medium");
 UE_DEFINE_GAMEPLAY_TAG(NearFar, "Bullet.NearMiss.Far");
 UE_DEFINE_GAMEPLAY_TAG(Flyby, "Bullet.Flyby");
 UE_DEFINE_GAMEPLAY_TAG(Crack, "Bullet.Crack");
 UE_DEFINE_GAMEPLAY_TAG(Impact, "Bullet.Impact.Nearby");
 UE_DEFINE_GAMEPLAY_TAG(Ricochet, "Bullet.Ricochet");

 bool EvaluateNearMiss(const FVector& Start, const FVector& End, const FVector& Listener,
  const UVanthexBulletAudioProfile& Profile, FNearMiss& Out)
 {
  Out = FNearMiss();
  if (Start.ContainsNaN() || End.ContainsNaN() || Listener.ContainsNaN()) return false;
  const FVector Segment = End - Start;
  const double LengthSquared = Segment.SizeSquared();
  if (!FMath::IsFinite(LengthSquared) || LengthSquared < 1.) return false;
  const double RawT = FVector::DotProduct(Listener - Start, Segment) / LengthSquared;
  const double T = FMath::Clamp(RawT, 0., 1.);
  // An endpoint impact is not a passing bullet. Never extend the authoritative ray.
  if (!FMath::IsFinite(RawT) || RawT <= 0. || RawT >= 1.) return false;
  const float Maximum = Profile.MaxFlybyDistance;
  if (!FMath::IsFinite(Maximum) || Maximum <= 0.f) return false;
  const FVector Point = Start + Segment * T;
  const double Distance = FVector::Distance(Point, Listener);
  if (Distance >= Maximum) return false;
  Out.Point = Point;
  Out.Distance = float(Distance);
  const float Close = FMath::Clamp(Profile.CloseDistance, 0.f, Maximum);
  const float Medium = FMath::Clamp(Profile.MediumDistance, Close, Maximum);
  Out.Band = Distance <= Close ? EBand::Close : Distance <= Medium ? EBand::Medium : EBand::Far;
  Out.Gain = FMath::Square(1.f - Out.Distance / Maximum);
  return true;
 }
}

UVanthexBulletAudioComponent::UVanthexBulletAudioComponent()
{
 PrimaryComponentTick.bCanEverTick = false;
 SetIsReplicatedByDefault(false);
}

void UVanthexBulletAudioComponent::BeginPlay()
{
 Super::BeginPlay();
 Profile = GetDefault<UVanthexBulletAudioSettings>()->Profile.LoadSynchronous();
}

APawn* UVanthexBulletAudioComponent::GetListenerPawn() const
{
 const auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 if (!PC) return nullptr;
 APawn* Pawn = PC->GetPawn();
 if (!Pawn) return nullptr;
 const auto* Health = Pawn->FindComponentByClass<UHealthComponent>();
 if (!Health || !Health->IsDead()) return Pawn;
 // Shared with the camera: spawn slots are replicated, iteration order is not.
 const auto* OwnState = PC->GetPlayerState<AVanthexPlayerState>();
 APawn* Spectated = nullptr;
 int32 LowestSlot = MAX_int32;
 if (OwnState) for (TActorIterator<AVanthexCharacter> It(GetWorld()); It; ++It)
 {
  const auto* State = It->GetPlayerState<AVanthexPlayerState>();
  if (State && State->GetTeam() == OwnState->GetTeam() && !It->GetHealthComponent()->IsDead() && State->GetSpawnSlot() < LowestSlot)
  {
   Spectated = *It;
   LowestSlot = State->GetSpawnSlot();
  }
 }
 return Spectated;
}

FVector UVanthexBulletAudioComponent::GetListenerLocation(const APawn* Pawn) const
{
 if (!Pawn) return FVector::ZeroVector;
 const auto* Camera = Pawn->FindComponentByClass<UCameraComponent>();
 const FVector Location = Camera && Camera->IsActive() ? Camera->GetComponentLocation() : Pawn->GetPawnViewLocation();
 return Location + Pawn->GetViewRotation().RotateVector(Profile ? Profile->ListenerOffset : FVector::ZeroVector);
}

bool UVanthexBulletAudioComponent::HasClearPath(UWorld* World, FVector Listener, FVector EventPoint,
 const AActor* ListenerActor, const AActor* Shooter)
{
 if (!World || Listener.ContainsNaN() || EventPoint.ContainsNaN()) return false;
 if (FVector::DistSquared(Listener, EventPoint) < 1.) return true;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexBulletAudioOcclusion), false);
 if (ListenerActor) Params.AddIgnoredActor(ListenerActor);
 if (Shooter) Params.AddIgnoredActor(Shooter);
 return !World->LineTraceTestByChannel(Listener, EventPoint, ECC_Visibility, Params);
}

FName UVanthexBulletAudioComponent::ResolveSurface(const FHitResult& Hit) const
{
 if (!Profile) return NAME_None;
 if (Hit.PhysMaterial.IsValid())
 {
  if (const FName* Surface = Profile->PhysicalSurfaces.Find(int32(Hit.PhysMaterial->SurfaceType))) return *Surface;
 }
 for (const FName Tag : Profile->SurfaceTagPriority)
 {
  if ((Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(Tag)) || (Hit.GetActor() && Hit.GetActor()->ActorHasTag(Tag)))
   if (const FName* Surface = Profile->SurfaceTags.Find(Tag)) return *Surface;
 }
 return Profile->DefaultSurface;
}

bool UVanthexBulletAudioComponent::Admit(double Now, double& Last, double& Window, int32& Count) const
{
 if (!Profile || Now - Last < FMath::Max(.02f, Profile->MinEventInterval)) return false;
 if (Now - Window >= 1. || Now < Window) { Window = Now; Count = 0; }
 if (Count >= FMath::Clamp(Profile->MaxPacketsPerSecond, 1, 60)) return false;
 Last = Now;
 ++Count;
 return true;
}

void UVanthexBulletAudioComponent::DispatchShot(UWorld* World, const AActor* Shooter, const FVector& Start,
 const FVector& TraceEnd, const FHitResult& Hit, bool bSupersonic)
{
 if (!World || World->GetNetMode() == NM_Client || !Shooter || !Shooter->HasAuthority()) return;
 const FVector End = Hit.bBlockingHit ? Hit.ImpactPoint : TraceEnd;
 for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
 {
  auto* PC = Cast<AVanthexPlayerController>(It->Get());
  auto* Audio = PC ? PC->GetBulletAudio() : nullptr;
  if (!Audio || !Audio->Profile) continue;
  APawn* ListenerPawn = Audio->GetListenerPawn();
  if (!ListenerPawn) continue;
  const auto& Config = *Audio->Profile;
  const FVector Listener = Audio->GetListenerLocation(ListenerPawn);
  FVanthexBulletAudioPacket Packet;
  VanthexBulletAudio::FNearMiss Near;
  if (ListenerPawn != Shooter && Hit.GetActor() != ListenerPawn && VanthexBulletAudio::EvaluateNearMiss(Start, End, Listener, Config, Near)
   && HasClearPath(World, Listener, Near.Point, ListenerPawn, Shooter))
  {
   Packet.NearPoint = Near.Point;
   Packet.Band = uint8(Near.Band);
   Packet.bSupersonic = bSupersonic;
  }
  // Impact on the audible face only; do not expose a far face through a thin wall.
  if (Hit.bBlockingHit && Hit.GetActor() && !Hit.GetActor()->FindComponentByClass<UHealthComponent>()
   && FVector::DotProduct(Listener - Hit.ImpactPoint, Hit.ImpactNormal) > 0.
   && FVector::DistSquared(Listener, Hit.ImpactPoint) < FMath::Square(FMath::Max(0.f, Config.MaxImpactDistance)))
  {
   const FVector ImpactPoint = Hit.ImpactPoint + Hit.ImpactNormal * 2.f;
   if (HasClearPath(World, Listener, ImpactPoint, ListenerPawn, Shooter))
   {
    Packet.bImpact = true;
    Packet.ImpactPoint = ImpactPoint;
    Packet.Surface = Audio->ResolveSurface(Hit);
   }
  }
  if (!Packet.Band && !Packet.bImpact) continue;
  if (!Audio->Admit(World->GetTimeSeconds(), Audio->LastSentTime, Audio->SendWindowStart, Audio->SendWindowCount)) continue;
  Packet.Sequence = ++Audio->NextSequence;
  ++Audio->SentPackets;
  PC->ClientBulletAudio(Packet);
 }
}

void UVanthexBulletAudioComponent::PlayEvent(FGameplayTag Event, const FVanthexBulletSoundSet* Set,
 FVector Position, float Gain, uint32 Seed)
{
 OnAudioEvent.Broadcast(Event, Position, Gain);
 if (!Profile || !Set || Set->Variants.IsEmpty() || !Profile->Attenuation || !Profile->Concurrency) return;
 FRandomStream Random(int32(Seed ^ GetTypeHash(Event)));
 const int32 First = Random.RandRange(0, Set->Variants.Num() - 1);
 USoundBase* Sound = nullptr;
 for (int32 Index = 0; Index < Set->Variants.Num(); ++Index)
 {
  Sound = Set->Variants[(First + Index) % Set->Variants.Num()];
  if (Sound) break;
 }
 if (!Sound || !FMath::IsFinite(Gain) || Gain <= 0.f) return;
 const float Volume = FMath::Clamp(Set->Volume, 0.f, 1.f) * FMath::Clamp(Gain, 0.f, 1.f);
 const float LowPitch = FMath::Clamp(Set->PitchMin, .9f, 1.1f);
 const float HighPitch = FMath::Clamp(Set->PitchMax, LowPitch, 1.1f);
 FVector PlaybackPosition = Position;
 if (const auto* PC = Cast<APlayerController>(GetOwner()))
 {
  FVector Listener, Front, Right;
  PC->GetAudioListenerPosition(Listener, Front, Right);
  // UE 5.8 FAudioDevice::GetAzimuth treats exactly X=0 as +90 degrees,
  // including the left and vertical axes. One millimetre avoids that singularity.
  // The authoritative geometry and published event position remain unchanged.
  if (FMath::Abs(FVector::DotProduct(Position - Listener, Front)) < .001)
   PlaybackPosition += Front * .1;
 }
 UGameplayStatics::PlaySoundAtLocation(this, Sound, PlaybackPosition, FRotator::ZeroRotator, Volume,
  Random.FRandRange(LowPitch, HighPitch), 0.f, Profile->Attenuation, Profile->Concurrency, GetOwner());
 ++PlayedSounds;
}

void UVanthexBulletAudioComponent::Receive(const FVanthexBulletAudioPacket& Packet)
{
 auto* PC = Cast<AVanthexPlayerController>(GetOwner());
 if (!PC || !PC->IsLocalController() || !GetWorld() || GetNetMode() == NM_DedicatedServer || !Profile) return;
 // Discard duplicate/out-of-order unreliable packets; no predicted playback exists.
 if (!Packet.Sequence || int32(Packet.Sequence - LastSequence) <= 0) { ++DroppedPackets; return; }
 LastSequence = Packet.Sequence;
 if (!Admit(GetWorld()->GetTimeSeconds(), LastReceiveTime, ReceiveWindowStart, ReceiveWindowCount)) { ++DroppedPackets; return; }
 ++ReceivedPackets;
 LastReceivedPacket = Packet;
 APawn* Pawn = GetListenerPawn();
 if (!Pawn) return;
 const FVector Listener = GetListenerLocation(Pawn);
 if (Packet.Band >= uint8(VanthexBulletAudio::EBand::Close) && Packet.Band <= uint8(VanthexBulletAudio::EBand::Far))
 {
  const float Distance = FVector::Distance(Listener, Packet.NearPoint);
  if (Distance < Profile->MaxFlybyDistance && HasClearPath(GetWorld(), Listener, Packet.NearPoint, Pawn, nullptr))
  {
   ++NearEvents;
   const float Gain = FMath::Square(1.f - Distance / FMath::Max(1.f, Profile->MaxFlybyDistance));
   const FGameplayTag Tag = Packet.Band == 1 ? VanthexBulletAudio::NearClose : Packet.Band == 2 ? VanthexBulletAudio::NearMedium : VanthexBulletAudio::NearFar;
   const FVanthexBulletSoundSet* Set = Profile->Events.Find(Tag);
   OnAudioEvent.Broadcast(VanthexBulletAudio::Flyby, Packet.NearPoint, Gain);
   if (!Set || Set->Variants.IsEmpty()) Set = Profile->Events.Find(VanthexBulletAudio::Flyby);
   PlayEvent(Tag, Set, Packet.NearPoint, Gain, Packet.Sequence);
   if (Packet.bSupersonic) PlayEvent(VanthexBulletAudio::Crack, Profile->Events.Find(VanthexBulletAudio::Crack), Packet.NearPoint, Gain, Packet.Sequence);
  }
 }
 if (Packet.bImpact)
 {
  const float Distance = FVector::Distance(Listener, Packet.ImpactPoint);
  if (Distance < Profile->MaxImpactDistance && HasClearPath(GetWorld(), Listener, Packet.ImpactPoint, Pawn, nullptr))
  {
   ++ImpactEvents;
   const float Gain = FMath::Square(1.f - Distance / FMath::Max(1.f, Profile->MaxImpactDistance));
   const auto* Set = Profile->Impacts.Find(Packet.Surface);
   if (!Set) Set = Profile->Events.Find(VanthexBulletAudio::Impact);
   PlayEvent(VanthexBulletAudio::Impact, Set, Packet.ImpactPoint, Gain, Packet.Sequence);
  }
 }
}
