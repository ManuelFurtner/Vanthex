#include "VanthexGadgetVisualComponent.h"
#include "VanthexGadgetPresentationProfile.h"
#include "VanthexArt.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"

namespace
{
FLinearColor Accent(FName Id)
{
 if(Id==TEXT("Flash")) return FLinearColor(.86f,.91f,1.f);
 if(Id==TEXT("Smoke")) return FLinearColor(.9f,.57f,.12f);
 if(Id==TEXT("Frag")) return FLinearColor(.55f,.69f,.21f);
 if(Id==TEXT("EMP")) return FLinearColor(.15f,.48f,1.f);
 return FLinearColor(.15f,1.f,.63f);
}
}

UVanthexGadgetVisualComponent::UVanthexGadgetVisualComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bStartWithTickEnabled=false;
}

UStaticMeshComponent* UVanthexGadgetVisualComponent::Part(UStaticMesh* Mesh,FVector Location,FRotator Rotation,FVector Scale,UMaterialInterface* Material)
{
 auto* C=NewObject<UStaticMeshComponent>(GetOwner());
 C->SetStaticMesh(Mesh); C->SetupAttachment(BodyRoot);
 C->SetRelativeLocationAndRotation(Location,Rotation); C->SetRelativeScale3D(Scale);
 C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetGenerateOverlapEvents(false);
 C->SetCanEverAffectNavigation(false); C->SetCastShadow(true);
 if(Material) C->SetMaterial(0,Material);
 C->RegisterComponent(); Parts.Add(C); return C;
}

UInstancedStaticMeshComponent* UVanthexGadgetVisualComponent::Instances(UStaticMesh* Mesh,UMaterialInterface* Material)
{
 auto* C=NewObject<UInstancedStaticMeshComponent>(GetOwner());
 C->SetupAttachment(EffectRoot); C->SetStaticMesh(Mesh);
 C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetGenerateOverlapEvents(false);
 C->SetCanEverAffectNavigation(false); C->SetCastShadow(false);
 if(Material) C->SetMaterial(0,Material);
 C->RegisterComponent(); return C;
}

void UVanthexGadgetVisualComponent::Configure(FName GadgetId)
{
 if(bConfigured || !GetOwner() || !GetOwner()->GetRootComponent()) return;
 Id=GadgetId; bConfigured=true;
 if(GetNetMode()==NM_DedicatedServer) return;
 Profile=LoadObject<UVanthexGadgetPresentationProfile>(nullptr,TEXT("/Game/Vanthex/Gadgets/DA_GadgetPresentation.DA_GadgetPresentation"));
 if(!Profile) ++MissingAssets;
 BodyRoot=NewObject<USceneComponent>(GetOwner()); BodyRoot->SetupAttachment(GetOwner()->GetRootComponent()); BodyRoot->RegisterComponent();
 EffectRoot=NewObject<USceneComponent>(GetOwner()); EffectRoot->SetupAttachment(GetOwner()->GetRootComponent());
 EffectRoot->SetAbsolute(true,true,false); EffectRoot->RegisterComponent();
 EffectRoot->SetWorldLocationAndRotation(GetOwner()->GetActorLocation(),FRotator::ZeroRotator);
 UStaticMesh* Cylinder=Profile?Profile->Cylinder.Get():VanthexArt::LoadCylinder();
 UStaticMesh* Sphere=Profile?Profile->Sphere.Get():VanthexArt::LoadSphere();
 UStaticMesh* Cube=Profile?Profile->Cube.Get():VanthexArt::LoadCube();
 const FLinearColor Color=Accent(Id);
 auto* Shell=VanthexArt::MakeSurface(this,Id==TEXT("Frag")?FLinearColor(.16f,.22f,.08f):FLinearColor(.055f,.071f,.085f),.4f,.7f,FLinearColor::Black,0.f,EVanthexSurface::PaintedSteel);
 auto* Steel=VanthexArt::MakeSurface(this,FLinearColor(.32f,.36f,.40f),.25f,.85f,FLinearColor::Black,0.f,EVanthexSurface::BareSteel);
 auto* Paint=VanthexArt::MakeSurface(this,Color*.42f,.38f,.3f);
 auto* Rubber=VanthexArt::MakeSurface(this,FLinearColor(.014f,.019f,.025f),.8f,0.f,FLinearColor::Black,0.f,EVanthexSurface::Rubber);
 auto* LED=VanthexArt::MakeEnergy(this,Color,2.f);
 const bool Puck=Id==TEXT("EMP") || Id==TEXT("Sensor");
 const float Radius=Puck?(Id==TEXT("EMP")?6.8f:5.5f):4.f;
 const float Height=Puck?4.f:12.f;
 Part(Id==TEXT("Frag")?Sphere:Cylinder,FVector::ZeroVector,FRotator::ZeroRotator,
  Id==TEXT("Frag")?FVector(.105f):FVector(Radius/50.f,Radius/50.f,Height/100.f),Shell);
 for(float Z:{-Height*.42f,Height*.42f}) Part(Cylinder,FVector(0,0,Z),FRotator::ZeroRotator,FVector((Radius+.45f)/50.f,(Radius+.45f)/50.f,.012f),Steel);
 Part(Cylinder,FVector(0,0,Height*.12f),FRotator::ZeroRotator,FVector((Radius+.12f)/50.f,(Radius+.12f)/50.f,.022f),Paint);
 Part(Cylinder,FVector(0,0,Height*.5f+.25f),FRotator::ZeroRotator,FVector(Radius*.62f/50.f,Radius*.62f/50.f,.012f),Rubber);
 if(Puck) {
  Part(Sphere,FVector(0,0,2.4),FRotator::ZeroRotator,FVector(.067f,.067f,.022f),Steel);
  for(int32 I=0; I<6; ++I) {
   const double A=I*UE_TWO_PI/6.; const FVector P(FMath::Cos(A)*Radius*.76f,FMath::Sin(A)*Radius*.76f,2.2f);
   Part(Cube,P,FRotator(0,float(FMath::RadiansToDegrees(A)),0),FVector(.012f,.026f,.008f),LED);
  }
  if(Id==TEXT("Sensor")) Part(Cylinder,FVector(0,0,5.4),FRotator::ZeroRotator,FVector(.009f,.009f,.065f),Steel);
 } else {
  for(int32 I=0; I<8; ++I) {
   const double A=I*UE_TWO_PI/8.; const FVector P(FMath::Cos(A)*(Radius+.08f),FMath::Sin(A)*(Radius+.08f),0);
   Part(Cube,P,FRotator(0,float(FMath::RadiansToDegrees(A)),0),FVector(.008f,.012f,.064f),Id==TEXT("Flash")?Rubber:Steel);
  }
  Part(Cube,FVector(1.5,0,Height*.5f+1.2f),FRotator(0,0,-12),FVector(.06f,.018f,.012f),Steel);
  Part(Cube,FVector(4.1,0,Height*.2f),FRotator(0,0,-8),FVector(.012f,.018f,.065f),Steel);
  Part(Sphere,FVector(0,-Radius-.15f,2.5f),FRotator::ZeroRotator,FVector(.012f),LED);
 }
 if(Profile && Profile->EnergyMaterial) {
  Energy=UMaterialInstanceDynamic::Create(Profile->EnergyMaterial,this);
  Energy->SetVectorParameterValue(TEXT("Color"),Color); Energy->SetScalarParameterValue(TEXT("Strength"),3.f);
 }
 SetComponentTickEnabled(true);
}

UAudioComponent* UVanthexGadgetVisualComponent::Sound(FName Event,float Gain,bool bFar,bool bLoop)
{
 if(GetNetMode()==NM_DedicatedServer || !EffectRoot) return nullptr;
 const auto* Found=Profile?Profile->Sounds.Find(Event):nullptr;
 if(!Found || !Found->Get()) { ++MissingAssets; return nullptr; }
 OneShots.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& A){return !A.IsValid() || !A->IsPlaying();});
 if(!bLoop && OneShots.Num()>=5) return nullptr;
 auto* Audio=UGameplayStatics::SpawnSoundAttached(Found->Get(),Event==TEXT("FlightLoop")?BodyRoot.Get():EffectRoot.Get(),NAME_None,
  FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,Gain,1.f,0.f,
  bFar?Profile->FarAttenuation.Get():Profile->NearAttenuation.Get(),Profile->Concurrency,true);
 if(Audio) { ++PlayedSounds; if(!bLoop) OneShots.Add(Audio); }
 return Audio;
}

void UVanthexGadgetVisualComponent::PlayBounce()
{
 if(!bConfigured || bEffectActive || !GetWorld() || GetWorld()->GetTimeSeconds()-LastBounce<.12f) return;
 LastBounce=GetWorld()->GetTimeSeconds();
 const FName Key(*FString::Printf(TEXT("Bounce%02d"),1+BounceSounds%3));
 if(Sound(Key,.20f)) ++BounceSounds;
}

void UVanthexGadgetVisualComponent::BuildSmoke()
{
 if(!Profile || !Profile->SmokeMaterial || !Profile->Sphere) { ++MissingAssets; return; }
 if(Smoke) { Smoke->DestroyComponent(); Smoke=nullptr; }
 Smoke=Instances(Profile->Sphere,Profile->SmokeMaterial);
 SmokeTransforms.Reset();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(GadgetSmokeLobes),false,GetOwner());
 for(TActorIterator<APawn> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
 const FVector Center=EffectRoot->GetComponentLocation();
 const FVector Axes[]={FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),FVector(0,0,1),FVector(0,0,-1)};
 auto Add=[&](FVector Offset,float Desired) {
  const FVector P=Center+Offset;
  if(GetWorld()->LineTraceTestByChannel(Center,P,ECC_Visibility,Q)) return;
  float Radius=FMath::Min(Desired,FMath::Max(0.f,EffectRadius-float(Offset.Size())));
  for(const FVector& Axis:Axes) {
   FHitResult Hit;
   if(GetWorld()->LineTraceSingleByChannel(Hit,P,P+Axis*Radius,ECC_Visibility,Q)) Radius=FMath::Min(Radius,FMath::Max(0.f,Hit.Distance-3.f));
  }
  if(Radius<22.f) return;
  FTransform T(FQuat::Identity,Offset,FVector(Radius/50.f));
  Smoke->AddInstance(T); SmokeTransforms.Add(T);
 };
 // Overlapping, wall-clipped volumes form a dense two-sided interior rather than a see-through shell.
 Add(FVector::ZeroVector,EffectRadius*.80f);
 const float Spacing=EffectRadius*.27f;
 for(int32 Z=-2; Z<=3; ++Z) for(int32 X=-3; X<=3; ++X) for(int32 Y=-3; Y<=3; ++Y) {
  const FVector Offset(X*Spacing,Y*Spacing,Z*Spacing);
  if(Offset.IsNearlyZero() || Offset.Size()>EffectRadius*.76f) continue;
  Add(Offset,EffectRadius*.245f);
 }
}

void UVanthexGadgetVisualComponent::BuildPulse()
{
 if(!Profile || !Energy) return;
 if(!Rings) Rings=Instances(Profile->Cube,Energy);
 Rings->ClearInstances();
 for(int32 I=0; I<48; ++I) Rings->AddInstance(FTransform::Identity);
}

void UVanthexGadgetVisualComponent::BuildBurst()
{
 if(!Profile || !Energy) return;
 if(!Sparks) Sparks=Instances(Profile->Cube,Energy);
 Sparks->ClearInstances(); SparkDirections.Reset();
 FRandomStream Random(41491);
 for(int32 I=0; I<22; ++I) {
  FVector D=Random.VRand(); D.Z=FMath::Abs(D.Z)*.8+.15; D.Normalize();
  SparkDirections.Add(D); Sparks->AddInstance(FTransform::Identity);
 }
}

void UVanthexGadgetVisualComponent::ActivateEffect(float Duration,float Radius,float ElapsedSeconds)
{
 if(!bConfigured || GetNetMode()==NM_DedicatedServer || bEffectActive || !EffectRoot) return;
 if(!FMath::IsFinite(Duration) || !FMath::IsFinite(Radius) || !FMath::IsFinite(ElapsedSeconds)) return;
 bEffectActive=true; bFlightStarted=true;
 EffectAge=FMath::Max(0.f,ElapsedSeconds); EffectDuration=FMath::Max(Duration,Id==TEXT("Frag")?.85f:.25f);
 EffectRadius=FMath::Clamp(Radius,50.f,3000.f); PulseAge=Id==TEXT("EMP")?EffectAge:100.f;
 if(EffectAge>=EffectDuration) { EndEffect(); return; }
 if(FlightAudio) { FlightAudio->Stop(); FlightAudio=nullptr; }
 EffectRoot->SetWorldLocation(GetOwner()->GetActorLocation()+(Id==TEXT("Smoke")?FVector(0,0,140):FVector::ZeroVector));
 EffectRoot->SetVisibility(true,true); BodyRoot->SetVisibility(Id==TEXT("EMP") || Id==TEXT("Sensor"),true);
 if(Id==TEXT("Smoke")) { BuildSmoke(); if(EffectAge<.25f) Sound(TEXT("SmokeStart"),.5f); LoopAudio=Sound(TEXT("SmokeLoop"),.12f,false,true); }
 else if(Id==TEXT("EMP") || Id==TEXT("Sensor")) {
  BuildPulse();
  if(EffectAge<.25f) Sound(Id==TEXT("EMP")?FName(TEXT("EMP")):FName(TEXT("SensorStart")),.55f,Id==TEXT("EMP"));
  if(Id==TEXT("EMP")) LoopAudio=Sound(TEXT("EMPHumLoop"),.075f,false,true);
 } else {
  BuildBurst();
  if(Energy) Energy->SetVectorParameterValue(TEXT("Color"),Id==TEXT("Flash")?FLinearColor(.85f,.94f,1.f):FLinearColor(1.f,.24f,.035f));
  if(EffectAge<.25f) Sound(Id==TEXT("Flash")?FName(TEXT("Flash")):FName(TEXT("Frag")),.8f,true);
 }
 if(!Light) {
  Light=NewObject<UPointLightComponent>(GetOwner()); Light->SetupAttachment(EffectRoot); Light->SetCastShadows(false);
  Light->SetLightColor(Accent(Id)); Light->SetIntensity(0.f); Light->SetAttenuationRadius(Id==TEXT("Frag")?650.f:450.f); Light->RegisterComponent();
 }
 SetComponentTickEnabled(true);
}

void UVanthexGadgetVisualComponent::PulseEffect()
{
 if(!bEffectActive || Id!=TEXT("Sensor")) return;
 ++PulseCount; PulseAge=0.f;
 Sound(TEXT("SensorPing"),.24f);
}

void UVanthexGadgetVisualComponent::EndEffect()
{
 const bool WasActive=bEffectActive; bEffectActive=false;
 if(FlightAudio) { FlightAudio->Stop(); FlightAudio=nullptr; }
 if(LoopAudio) { LoopAudio->Stop(); LoopAudio=nullptr; }
 if(EffectRoot) EffectRoot->SetVisibility(false,true);
 if(Light) Light->SetIntensity(0.f);
 if(WasActive && Id==TEXT("Smoke")) Sound(TEXT("SmokeEnd"),.16f);
 if(WasActive && Id==TEXT("Sensor")) Sound(TEXT("SensorEnd"),.12f);
 SetComponentTickEnabled(false);
}

void UVanthexGadgetVisualComponent::StopAudio()
{
 if(FlightAudio) FlightAudio->Stop();
 if(LoopAudio) LoopAudio->Stop();
 for(auto& A:OneShots) if(A.IsValid()) A->Stop();
 FlightAudio=nullptr; LoopAudio=nullptr; OneShots.Reset();
}

void UVanthexGadgetVisualComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 StopAudio(); Super::EndPlay(Reason);
}

int32 UVanthexGadgetVisualComponent::GetSmokeInstanceCount() const { return Smoke?Smoke->GetInstanceCount():0; }

void UVanthexGadgetVisualComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,TickType,Function);
 if(!bConfigured || GetNetMode()==NM_DedicatedServer) return;
 if(!bEffectActive) {
  if(EffectRoot) EffectRoot->SetWorldLocation(GetOwner()->GetActorLocation());
  const float Speed=float(GetOwner()->GetVelocity().Size());
  if(Speed>80.f && !bFlightStarted) { bFlightStarted=true; Sound(TEXT("Throw"),.15f); FlightAudio=Sound(TEXT("FlightLoop"),.065f,false,true); }
  if(Speed>30.f && BodyRoot) BodyRoot->AddLocalRotation(FRotator(150.f*Delta,80.f*Delta,110.f*Delta));
  if(Speed<25.f && FlightAudio) { FlightAudio->FadeOut(.12f,0.f); FlightAudio=nullptr; }
  return;
 }
 EffectAge+=Delta; PulseAge+=Delta;
 if(EffectAge>=EffectDuration) { EndEffect(); return; }
 if(Smoke) return; // Animated material supplies surface turbulence; opaque volume remains stable for LOS.
 if(Rings) {
  const float Life=Id==TEXT("EMP")?1.1f:.8f;
  const float T=FMath::Clamp(PulseAge/Life,0.f,1.f);
  Rings->SetVisibility(T<1.f);
  if(T<1.f) {
   const float Radius=FMath::Lerp(8.f,EffectRadius,T);
   for(int32 I=0; I<48; ++I) {
    const double A=I*UE_TWO_PI/48.;
    const FVector P(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,8.f+T*18.f);
    const FTransform Transform(FRotator(0,float(FMath::RadiansToDegrees(A))+90.f,0),P,FVector((Radius*UE_TWO_PI/48.f+1.f)/100.f,.018f,.012f));
    Rings->UpdateInstanceTransform(I,Transform,false,I==47,true);
   }
  }
  if(Energy) Energy->SetScalarParameterValue(TEXT("Strength"),(1.f-T)*4.f+.15f);
 }
 if(Sparks) {
  const float T=EffectAge/.65f;
  Sparks->SetVisibility(T<1.f);
  if(T<1.f) for(int32 I=0; I<SparkDirections.Num(); ++I) {
   const FVector D=SparkDirections[I];
   const FVector P=D*(40.f+EffectAge*560.f)+FVector(0,0,-180.f*EffectAge*EffectAge);
   Sparks->UpdateInstanceTransform(I,FTransform(D.Rotation(),P,FVector(.28f*(1.f-T)+.015f,.008f,.008f)),false,I==SparkDirections.Num()-1,true);
  }
  if(Energy) Energy->SetScalarParameterValue(TEXT("Strength"),FMath::Max(0.f,1.f-T)*7.f);
 }
 if(Light) {
  const float Intensity=(Id==TEXT("Flash")?65000.f:(Id==TEXT("Frag")?28000.f:150.f))*FMath::Max(0.f,1.f-EffectAge/.22f);
  Light->SetIntensity(Intensity);
 }
}
