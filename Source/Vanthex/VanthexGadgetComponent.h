#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexGadgetComponent.generated.h"

USTRUCT()
struct FVanthexSensorMark
{
 GENERATED_BODY()
 UPROPERTY() FVector_NetQuantize Position=FVector::ZeroVector;
 UPROPERTY() float Expires=0.f;
};

UCLASS()
class UVanthexGadgetComponent:public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexGadgetComponent();
 void UseSlot(int32 Slot);
 UFUNCTION(Server,Reliable) void ServerThrow(int32 Slot,uint32 RequestId);
 bool TryThrow(int32 Slot,uint32 RequestId);
 void ResetForRound();
 void ApplyFlash(float Strength);
 void ApplyEMP(float Duration);
 bool IsBlinded() const;
 bool IsJammed() const;
 float GetFlashOpacity() const;
 void AddSensorMark(FVector Position);
 const TArray<FVanthexSensorMark>& GetMarks() const {return Marks;}
 float GetServerTime() const;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
 UPROPERTY(Replicated) float FlashEnd=0.f;
 UPROPERTY(Replicated) float FlashPower=0.f;
 UPROPERTY(Replicated) float JamEnd=0.f;
 UPROPERTY(Replicated) TArray<FVanthexSensorMark> Marks;
 uint32 NextRequest=0,LastRequest=0;
 float NextThrow=0.f;
 TWeakObjectPtr<class AController> RequestOwner;
};
