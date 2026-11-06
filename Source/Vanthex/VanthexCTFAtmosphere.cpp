#include "VanthexTestArena.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"

void AVanthexTestArena::BuildCTFAtmosphere()
{
 UWorld* World=GetWorld();
 if(!World)return;
 FActorSpawnParameters Params;
 Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

 // A low Mediterranean afternoon sun makes columns and arcade shadows readable.
 // Keep intensity compatible with the character camera's fixed exposure.
 const FTransform SunTransform(FRotator(-35.f,-42.f,0.f));
 if(auto* Sun=World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(),SunTransform)) {
  Sun->Tags.Add(TEXT("CTF_Roman_Sun"));
  if(auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) {
   Light->SetMobility(EComponentMobility::Movable);
   Light->SetIntensity(8.f);
   Light->SetLightColor(FLinearColor(1.f,.91f,.76f));
   Light->SetAtmosphereSunLight(true);
   Light->SetAtmosphereSunLightIndex(0);
   Light->SetAtmosphereSunDiskColorScale(FLinearColor(1.f,.96f,.85f));
   Light->SetLightSourceAngle(.65f);
   Light->SetCastShadows(true);
   Light->SetDynamicShadowDistanceMovableLight(12000.f);
   Light->SetDynamicShadowCascades(4);
   Light->SetCascadeDistributionExponent(2.f);
   Light->ShadowBias=.35f;
   Light->ShadowSlopeBias=.35f;
   Light->ContactShadowLength=.035f;
  }
  Sun->FinishSpawning(SunTransform);
 }

 if(auto* Atmosphere=World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector,FRotator::ZeroRotator,Params))
  Atmosphere->Tags.Add(TEXT("CTF_Roman_Sky"));

 const FTransform SkyTransform=FTransform::Identity;
 SkyLight=World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(),SkyTransform);
 if(SkyLight) {
  SkyLight->Tags.Add(TEXT("CTF_Roman_SkyLight"));
  if(auto* Light=SkyLight->GetLightComponent()) {
   Light->SetMobility(EComponentMobility::Movable);
   Light->SetIntensity(.85f);
   Light->SetLightColor(FLinearColor(.8f,.88f,1.f));
   Light->bLowerHemisphereIsBlack=false;
   Light->LowerHemisphereColor=FLinearColor(.2f,.16f,.11f);
   Light->SetRealTimeCaptureEnabled(true);
  }
  SkyLight->FinishSpawning(SkyTransform);
 }

 // Light warm distance haze; bases remain clear throughout the 88-metre arena.
 if(auto* Fog=World->SpawnActor<AExponentialHeightFog>(FVector(0,0,-150),FRotator::ZeroRotator,Params)) {
  Fog->Tags.Add(TEXT("CTF_Roman_Haze"));
  if(auto* Component=Fog->GetComponent()) {
   Component->SetFogDensity(.0012f);
   Component->SetFogHeightFalloff(.25f);
   Component->SetFogInscatteringColor(FLinearColor(.6f,.66f,.73f));
   Component->SetStartDistance(4500.f);
   Component->SetFogMaxOpacity(.12f);
   Component->SetVolumetricFog(false);
  }
 }

 if(auto* Post=World->SpawnActor<APostProcessVolume>(FVector::ZeroVector,FRotator::ZeroRotator,Params)) {
  Post->Tags.Add(TEXT("CTF_Roman_Grade"));
  Post->bUnbound=true;
  auto& Settings=Post->Settings;
  Settings.bOverride_AutoExposureMethod=true;
  Settings.AutoExposureMethod=AEM_Histogram;
  Settings.bOverride_AutoExposureMinBrightness=true;
  Settings.AutoExposureMinBrightness=1.f;
  Settings.bOverride_AutoExposureMaxBrightness=true;
  Settings.AutoExposureMaxBrightness=1.f;
  Settings.bOverride_AutoExposureBias=true;
  Settings.AutoExposureBias=0.f;
  Settings.bOverride_BloomIntensity=true;
  Settings.BloomIntensity=.18f;
  Settings.bOverride_BloomThreshold=true;
  Settings.BloomThreshold=1.5f;
  Settings.bOverride_VignetteIntensity=true;
  Settings.VignetteIntensity=.08f;
  Settings.bOverride_MotionBlurAmount=true;
  Settings.MotionBlurAmount=0.f;
  Settings.bOverride_SceneFringeIntensity=true;
  Settings.SceneFringeIntensity=0.f;
  Settings.bOverride_ColorSaturation=true;
  Settings.ColorSaturation=FVector4(.98f,.96f,.92f,1.f);
  Settings.bOverride_ColorContrast=true;
  Settings.ColorContrast=FVector4(1.03f,1.03f,1.03f,1.f);
 }
}
