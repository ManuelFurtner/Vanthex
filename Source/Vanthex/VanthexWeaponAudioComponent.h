#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "TimerManager.h"
#include "VanthexWeaponAudioProfile.h"
#include "VanthexWeaponAudioComponent.generated.h"

class UAudioComponent;

USTRUCT()
struct FVanthexWeaponAudioPacket
{
 GENERATED_BODY()
 UPROPERTY() uint32 SourceId = 0;
 UPROPERTY() uint32 ActionId = 0;
 UPROPERTY() FName WeaponId;
 UPROPERTY() EVanthexWeaponAudioEvent Event = EVanthexWeaponAudioEvent::Shot;
 UPROPERTY() FVector_NetQuantize Position = FVector::ZeroVector;
 UPROPERTY() float Gain = 1.f;
 UPROPERTY() float LowPassHz = 20000.f;
 UPROPERTY() float ReflectionDelay = 0.f;
 /** Server compares the source owner chain with the active listening/spectated pawn. */
 UPROPERTY() bool bOwnSource = false;
};

/** One on each owning controller. Only server-confirmed gameplay generates world packets. */
UCLASS(ClassGroup=(Audio))
class UVanthexWeaponAudioComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexWeaponAudioComponent();
 static void DispatchEvent(AActor* Source, FName WeaponId, EVanthexWeaponAudioEvent Event, uint32 ActionId);
 void Receive(const FVanthexWeaponAudioPacket& Packet);
 void PlayUI(EVanthexWeaponAudioEvent Event);
 const UVanthexWeaponAudioProfile* GetProfile() const { return Profile; }
 int32 GetActiveVoiceCount();
 static int32 EventPriority(EVanthexWeaponAudioEvent Event, bool bOwnSource, bool bRoomTail=false);
 uint32 SentPackets=0, ReceivedPackets=0, DuplicateDropped=0, RateDropped=0;
 uint32 SentShotPackets=0, ReceivedShotPackets=0, SentMovementPackets=0, ReceivedMovementPackets=0;
 uint32 PlayedSounds=0, MissingAssets=0, ConcurrencyDropped=0, CancelStopped=0, PortalRouted=0;
 uint32 PriorityPreemptions=0;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY() TObjectPtr<UVanthexWeaponAudioProfile> Profile;
private:
 struct FVoice { TWeakObjectPtr<UAudioComponent> Audio; uint32 SourceId=0, ActionId=0; EVanthexWeaponAudioEvent Event=EVanthexWeaponAudioEvent::Shot; int32 Priority=0; };
 TArray<FVoice> Voices;
 TMap<FString, float> Recent;
 TMap<uint64, float> CanceledReloads;
 TArray<FTimerHandle> ReflectionTimers;
 float SendWindow=-1.f;
 int32 WindowPackets=0;
 bool AdmitPacket();
 void Play(const FVanthexWeaponAudioPacket& Packet, USoundBase* Sound, float Volume, bool bUI=false, bool bRoomTail=false);
};
