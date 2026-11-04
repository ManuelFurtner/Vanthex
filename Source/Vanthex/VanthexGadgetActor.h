#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexTypes.h"
#include "VanthexGadgetActor.generated.h"
class AVanthexCharacter;
class USphereComponent;
class UProjectileMovementComponent;
class UVanthexGadgetVisualComponent;
UCLASS()
class AVanthexGadgetActor:public AActor
{
 GENERATED_BODY()
public:
 AVanthexGadgetActor();
 void Configure(FName Id,AVanthexCharacter* Thrower);
 void Launch(FVector Velocity);
 void Detonate();
 bool IsSmokeActive() const;
 bool IsSensorActive() const;
 float GetEffectRadius() const;
 static bool SmokeBlocks(UWorld* World,const FVector& A,const FVector& B);
 static void ResetAll(UWorld* World);
 void ApplyEMP(float Duration);
 UPROPERTY(ReplicatedUsing=OnRep_Effect) FName GadgetId;
 UPROPERTY(ReplicatedUsing=OnRep_Effect) bool bActivated=false;
 UPROPERTY(ReplicatedUsing=OnRep_Effect) float ActivationServerTime=0.f;
 UPROPERTY(ReplicatedUsing=OnRep_Effect) float JamUntil=0.f;
 virtual void Tick(float DeltaSeconds)override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const override;
 UFUNCTION()void OnRep_Effect();
 UFUNCTION(NetMulticast,Unreliable)void MulticastPulse();
 UFUNCTION(NetMulticast,Unreliable)void MulticastBounce();
protected:
 virtual void BeginPlay()override;
 UFUNCTION()void OnBounce(const FHitResult& Hit,const FVector& Velocity);
 UPROPERTY(VisibleAnywhere)TObjectPtr<USphereComponent> Collision;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UProjectileMovementComponent> Movement;
 UPROPERTY(VisibleAnywhere)TObjectPtr<UVanthexGadgetVisualComponent> Visual;
 UPROPERTY()TObjectPtr<AVanthexCharacter> InstigatorCharacter;
 EVanthexTeam ThrowTeam=EVanthexTeam::None;
 float SpawnTime=0,NextScan=0,NextBounce=0;
 bool bVisualActive=false;
 void Scan();
 float ServerTime()const;
 bool CanReach(const FVector& End,const AActor* Target)const;
};
