#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VanthexGadgetVisualComponent.generated.h"

class UVanthexGadgetPresentationProfile;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAudioComponent;
class UPointLightComponent;

/** Cosmetic only. The owning replicated gadget controls activation, damage, LOS and scan timing. */
UCLASS(ClassGroup=(Vanthex))
class UVanthexGadgetVisualComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UVanthexGadgetVisualComponent();
 void Configure(FName GadgetId);
 void ActivateEffect(float Duration,float Radius,float ElapsedSeconds=0.f);
 void EndEffect();
 void PlayBounce();
 /** Called only when the authoritative sensor actually scans. No independent cosmetic scan timer. */
 void PulseEffect();
 bool IsEffectVisible() const { return bEffectActive; }
 int32 GetSmokeInstanceCount() const;
 int32 GetProjectilePartCount() const { return Parts.Num(); }
 const UVanthexGadgetPresentationProfile* GetProfile() const { return Profile; }
 uint32 PlayedSounds=0, MissingAssets=0, BounceSounds=0, PulseCount=0;
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 FName Id;
 bool bConfigured=false,bEffectActive=false,bFlightStarted=false;
 float EffectAge=0.f,EffectDuration=0.f,EffectRadius=0.f,PulseAge=100.f,LastBounce=-100.f;
 UPROPERTY() TObjectPtr<UVanthexGadgetPresentationProfile> Profile;
 UPROPERTY() TObjectPtr<USceneComponent> BodyRoot;
 UPROPERTY() TObjectPtr<USceneComponent> EffectRoot;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Smoke;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Sparks;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rings;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Energy;
 UPROPERTY() TObjectPtr<UPointLightComponent> Light;
 UPROPERTY() TObjectPtr<UAudioComponent> FlightAudio;
 UPROPERTY() TObjectPtr<UAudioComponent> LoopAudio;
 TArray<TWeakObjectPtr<UAudioComponent>> OneShots;
 TArray<FTransform> SmokeTransforms;
 TArray<FVector> SparkDirections;
 UStaticMeshComponent* Part(UStaticMesh* Mesh,FVector Location,FRotator Rotation,FVector Scale,UMaterialInterface* Material);
 UInstancedStaticMeshComponent* Instances(UStaticMesh* Mesh,UMaterialInterface* Material);
 UAudioComponent* Sound(FName Event,float Gain,bool bFar=false,bool bLoop=false);
 void BuildSmoke();
 void BuildPulse();
 void BuildBurst();
 void StopAudio();
};
