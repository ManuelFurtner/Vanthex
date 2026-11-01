#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Engine/NetSerialization.h"
#include "VanthexBulletAudioComponent.generated.h"

class UVanthexBulletAudioProfile;
class AVanthexPlayerController;
class APawn;
struct FVanthexBulletSoundSet;

namespace VanthexBulletAudio
{
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(NearClose);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(NearMedium);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(NearFar);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flyby);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crack);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(Impact);
 UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ricochet);
 enum class EBand : uint8 { None, Close, Medium, Far };
 struct FNearMiss
 {
  FVector Point = FVector::ZeroVector;
  float Distance = 0.f;
  float Gain = 0.f;
  EBand Band = EBand::None;
 };
 bool EvaluateNearMiss(const FVector& Start, const FVector& End, const FVector& Listener,
  const UVanthexBulletAudioProfile& Profile, FNearMiss& Out);
}

/** Contains only listener-relevant positions, never the hidden shooter's origin or full ray. */
USTRUCT()
struct FVanthexBulletAudioPacket
{
 GENERATED_BODY()
 UPROPERTY() uint32 Sequence = 0;
 UPROPERTY() FVector_NetQuantize NearPoint = FVector::ZeroVector;
 UPROPERTY() FVector_NetQuantize ImpactPoint = FVector::ZeroVector;
 UPROPERTY() FName Surface;
 UPROPERTY() uint8 Band = 0;
 UPROPERTY() bool bSupersonic = false;
 UPROPERTY() bool bImpact = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVanthexBulletAudioEvent, FGameplayTag, Event, FVector, Position, float, Gain);

/** One router per player controller, no tick, no replicated component and no per-bullet actors. */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class UVanthexBulletAudioComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexBulletAudioComponent();
 virtual void BeginPlay() override;
 static void DispatchShot(UWorld* World, const AActor* Shooter, const FVector& Start,
  const FVector& TraceEnd, const FHitResult& Hit, bool bSupersonic);
 static bool HasClearPath(UWorld* World, FVector Listener, FVector EventPoint,
  const AActor* ListenerActor, const AActor* Shooter);
 void Receive(const FVanthexBulletAudioPacket& Packet);
 FVector GetListenerLocation(const APawn* Pawn) const;
 APawn* GetListenerPawn() const;
 const UVanthexBulletAudioProfile* GetProfile() const { return Profile; }
 UPROPERTY(BlueprintAssignable, Category="Bullet Audio") FVanthexBulletAudioEvent OnAudioEvent;
 // Diagnostic counters also used by the opt-in network test; no logging in normal matches.
 uint32 SentPackets = 0, ReceivedPackets = 0, DroppedPackets = 0, PlayedSounds = 0;
 uint32 NearEvents = 0, ImpactEvents = 0;
 FVanthexBulletAudioPacket LastReceivedPacket;
private:
 UPROPERTY(Transient) TObjectPtr<UVanthexBulletAudioProfile> Profile;
 uint32 NextSequence = 0, LastSequence = 0;
 double LastSentTime = -1000., LastReceiveTime = -1000.;
 double SendWindowStart = -1000., ReceiveWindowStart = -1000.;
 int32 SendWindowCount = 0, ReceiveWindowCount = 0;
 bool Admit(double Now, double& Last, double& Window, int32& Count) const;
 FName ResolveSurface(const FHitResult& Hit) const;
 void PlayEvent(FGameplayTag Event, const FVanthexBulletSoundSet* Set, FVector Position, float Gain, uint32 Seed);
};
