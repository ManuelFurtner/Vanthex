#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"

static void RecordPhotorealLighting(UWorld* World)
{
 TArray<FString> Audit;
 Audit.Add(TEXT("Runtime renderer configuration and authored light inventory; not a visual or frame-rate verdict."));
 for(const TCHAR* Name:{TEXT("r.DynamicGlobalIlluminationMethod"),TEXT("r.ReflectionMethod"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.Lumen.Reflections.ScreenTraces"),TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"),TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"),TEXT("r.AnisotropicMaterials")}) {
  auto* V=IConsoleManager::Get().FindConsoleVariable(Name);Audit.Add(FString::Printf(TEXT("%s=%s"),Name,V?*V->GetString():TEXT("unregistered")));
 }
 for(TActorIterator<APostProcessVolume> It(World);It;++It){const auto& S=It->Settings;Audit.Add(FString::Printf(TEXT("POST exposure min=%.3f max=%.3f bias=%.3f bloom=%.3f LumenFG=%.2f override=%d LumenLighting=%.2f override=%d LumenReflection=%.2f override=%d"),S.AutoExposureMinBrightness,S.AutoExposureMaxBrightness,S.AutoExposureBias,S.BloomIntensity,S.LumenFinalGatherQuality,int32(S.bOverride_LumenFinalGatherQuality),S.LumenSceneLightingQuality,int32(S.bOverride_LumenSceneLightingQuality),S.LumenReflectionQuality,int32(S.bOverride_LumenReflectionQuality)));}
 int32 Total=0,Shadowed=0;
 for(TActorIterator<ALight> It(World);It;++It)if(auto* L=It->GetLightComponent()) {
  ++Total;Shadowed+=L->CastShadows?1:0;
  Audit.Add(FString::Printf(TEXT("LIGHT %s pos=%s intensity=%.3f shadow=%d contact=%.4f bias=%.3f"),*It->GetClass()->GetName(),*It->GetActorLocation().ToString(),L->Intensity,int32(L->CastShadows),L->ContactShadowLength,L->ShadowBias));
 }
 Audit.Add(FString::Printf(TEXT("TOTAL %d lights; %d shadow casting"),Total,Shadowed));
 FFileHelper::SaveStringToFile(FString::Join(Audit,TEXT("\n")),*(FPaths::ProjectSavedDir()/TEXT("Photorealism/LightingAudit.txt")));
}

// Identity-free snapshot of the real blocking mesh transforms and channel responses.
// Re-baking changes actor names; changing a physical surface or bullet response must
// change this digest. Materials, lights and non-colliding decoration are excluded.
bool VanthexAuditVisualCollision(UWorld* World,TArray<FString>& Lines)
{
 if(FParse::Param(FCommandLine::Get(),TEXT("PhotorealAudit")))RecordPhotorealLighting(World);
 TArray<FString> Surfaces;
 for(TActorIterator<AActor> It(World);It;++It) {
  TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
  for(auto* C:Parts) {
   if(!C->GetStaticMesh()||C->GetCollisionEnabled()==ECollisionEnabled::NoCollision||C->GetCollisionObjectType()!=ECC_WorldStatic)continue;
   const auto T=C->GetComponentTransform();const auto P=T.GetLocation(),S=T.GetScale3D();const auto R=T.Rotator();
   // Cooking can change the sign bit of floating zero. Canonicalise only values
   // that already round to zero at the recorded precision, never real distances.
   auto Zero=[](double V,double HalfUnit=.0005){return FMath::Abs(V)<HalfUnit?0.:V;};
   FString Entry=FString::Printf(TEXT("%s|%.3f %.3f %.3f|%.3f %.3f %.3f|%.4f %.4f %.4f|%d|%d|"),*C->GetStaticMesh()->GetPathName(),Zero(P.X),Zero(P.Y),Zero(P.Z),Zero(R.Pitch),Zero(R.Yaw),Zero(R.Roll),Zero(S.X,.00005),Zero(S.Y,.00005),Zero(S.Z,.00005),int32(C->GetCollisionEnabled()),int32(C->CanCharacterStepUpOn));
   for(int32 Channel=0;Channel<ECC_MAX;++Channel)Entry+=FString::FromInt(int32(C->GetCollisionResponseToChannel(ECollisionChannel(Channel))));
   Surfaces.Add(Entry);
  }
 }
 Surfaces.Sort();const FString Canonical=FString::Join(Surfaces,TEXT("\n"));FTCHARToUTF8 Bytes(*Canonical);
 uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.Get(),Bytes.Length(),Hash);
 const FString Digest=BytesToHex(Hash,FSHA1::DigestSize);
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("HighFidelity");IFileManager::Get().MakeDirectory(*Folder,true);
 const FString Baseline=Folder/TEXT("CollisionBaseline.txt");
 FFileHelper::SaveStringToFile(Canonical,*(Folder/TEXT("CollisionCurrentSurfaces.txt")));
 if(FParse::Param(FCommandLine::Get(),TEXT("WriteVisualCollisionBaseline")))FFileHelper::SaveStringToFile(Digest,*Baseline);
 // Audited before the visual upgrade. Keep this invariant on fresh clones and
 // packaged builds too; a missing Saved file must not silently bypass it.
 // The original raw digest was 9ACE2225FCF5565FEE45ADC813C24C47A51620BB.
 // Independently comparing all 1,860 records proved the only cooked difference
 // was signed zero; canonicalised original and cooked records have this digest.
 const FString RecordedDigest=TEXT("A358EFB49579314017BAABC2CE47A5F5F8ECB5FE");
 FString Expected;const bool HasBaseline=FFileHelper::LoadFileToString(Expected,*Baseline);
 const bool LocalMatches=!HasBaseline||Expected.TrimStartAndEnd()==Digest||(Expected.TrimStartAndEnd()==TEXT("9ACE2225FCF5565FEE45ADC813C24C47A51620BB")&&Digest==RecordedDigest);
 Lines.Add(FString::Printf(TEXT("COLLISION_SIGNATURE %d static surfaces SHA1=%s baseline=%s"),Surfaces.Num(),*Digest,HasBaseline?(LocalMatches?TEXT("MATCH"):TEXT("MISMATCH")):TEXT("not recorded")));
 Lines.Add(FString::Printf(TEXT("RECORDED_COLLISION_CONTRACT %s"),Surfaces.Num()==1860&&Digest==RecordedDigest?TEXT("MATCH"):TEXT("MISMATCH")));
 return Surfaces.Num()==1860&&Digest==RecordedDigest&&LocalMatches;
}
