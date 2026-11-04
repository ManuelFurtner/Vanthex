#include "VanthexBuildGadgetPresentationCommandlet.h"
#include "Vanthex.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "VanthexGadgetPresentationProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr const TCHAR* Root=TEXT("/Game/Vanthex/Gadgets/");
bool Save(UObject* Object)
{
 if(!Object) return false;
 Object->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(Object->GetOutermost(),Object,*FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
template<typename T> T* Asset(const FString& Name,bool& Created)
{
 const FString Package=FString(Root)+Name;
 if(auto* Existing=LoadObject<T>(nullptr,*(Package+TEXT(".")+Name),nullptr,LOAD_NoWarn)) { Created=false; return Existing; }
 auto* New=NewObject<T>(CreatePackage(*Package),*Name,RF_Public|RF_Standalone|RF_Transactional);
 FAssetRegistryModule::AssetCreated(New); Created=true; return New;
}
template<typename T> T* Expression(UMaterial* M)
{
 auto* E=NewObject<T>(M); M->GetExpressionCollection().AddExpression(E); return E;
}
void Connect(FExpressionInput& Input,UMaterialExpression* E)
{
 Input.Expression=E; Input.OutputIndex=0;
 const auto Outputs=E->GetOutputs();
 if(!Outputs.IsEmpty()) { Input.Mask=Outputs[0].Mask; Input.MaskR=Outputs[0].MaskR; Input.MaskG=Outputs[0].MaskG; Input.MaskB=Outputs[0].MaskB; Input.MaskA=Outputs[0].MaskA; }
}
UMaterial* Material(const FString& Name,bool Smoke,bool Replace)
{
 bool Created=false; auto* M=Asset<UMaterial>(Name,Created);
 if(!Created && !Replace) return M;
 M->PreEditChange(nullptr); M->GetExpressionCollection().Empty();
 M->MaterialDomain=MD_Surface; M->BlendMode=BLEND_Opaque; M->SetShadingModel(MSM_Unlit);
 M->TwoSided=Smoke; M->bUsedWithInstancedStaticMeshes=true; M->bUsedWithNanite=true;
 auto* Color=Expression<UMaterialExpressionVectorParameter>(M); Color->ParameterName=TEXT("Color");
 Color->DefaultValue=Smoke?FLinearColor(.42f,.46f,.49f):FLinearColor(.15f,.65f,1.f);
 if(Smoke) {
  auto* World=Expression<UMaterialExpressionWorldPosition>(M);
  auto* Normal=Expression<UMaterialExpressionVertexNormalWS>(M);
  auto* Time=Expression<UMaterialExpressionTime>(M);
  auto* Noise=Expression<UMaterialExpressionCustom>(M); Noise->OutputType=CMOT_Float3; Noise->ContainsClipInstruction=CMCI_No;
  Noise->Inputs.Reset(); // UMaterialExpressionCustom starts with an unnamed input.
  for(const TCHAR* NameInput:{TEXT("P"),TEXT("N"),TEXT("T"),TEXT("Tint")}) { FCustomInput I; I.InputName=NameInput; Noise->Inputs.Add(I); }
  Connect(Noise->Inputs[0].Input,World); Connect(Noise->Inputs[1].Input,Normal); Connect(Noise->Inputs[2].Input,Time); Connect(Noise->Inputs[3].Input,Color);
  Noise->Code=TEXT(R"(
float3 q=P*0.023+float3(T*.10,T*.04,-T*.075);
float a=sin(q.x+sin(q.y*.83))*sin(q.y*.92+sin(q.z))*sin(q.z*.77+sin(q.x*.67));
float b=sin(q.x*2.13+q.z*.38)*sin(q.y*1.81-q.x*.22);
float lighting=.70+.12*dot(normalize(N),normalize(float3(.35,-.3,.8)));
return Tint*(lighting+.13*a+.045*b);
)");
  Connect(M->GetEditorOnlyData()->EmissiveColor,Noise);
 } else {
  auto* Strength=Expression<UMaterialExpressionScalarParameter>(M); Strength->ParameterName=TEXT("Strength"); Strength->DefaultValue=3.f;
  auto* Multiply=Expression<UMaterialExpressionMultiply>(M); Connect(Multiply->A,Color); Connect(Multiply->B,Strength);
  Connect(M->GetEditorOnlyData()->EmissiveColor,Multiply);
 }
 M->PostEditChange(); M->ForceRecompileForRendering(); return Save(M)?M:nullptr;
}
USoundWave* Import(const FString& Event,bool Replace)
{
 const FString Name=TEXT("S_GD_")+Event, Package=FString(Root)+Name;
 auto* Existing=LoadObject<USoundWave>(nullptr,*(Package+TEXT(".")+Name),nullptr,LOAD_NoWarn);
 if(Existing && !Replace) return Existing;
 const FString Source=FPaths::ProjectDir()/TEXT("AudioSource/Gadgets")/(Name+TEXT(".wav"));
 if(!FPaths::FileExists(Source)) { UE_LOG(LogVanthex,Error,TEXT("GADGET_PRESENTATION missing source %s"),*Source); return nullptr; }
 auto* Factory=NewObject<USoundFactory>(); Factory->bAutoCreateCue=false; Factory->SuppressImportDialogs(); bool Canceled=false;
 auto* W=Cast<USoundWave>(Factory->ImportObject(USoundWave::StaticClass(),Existing?Existing->GetOutermost():CreatePackage(*Package),*Name,
  RF_Public|RF_Standalone|RF_Transactional,Source,nullptr,Canceled));
 if(!W || Canceled) return nullptr;
 W->bLooping=Event.EndsWith(TEXT("Loop")); W->Volume=1.f; W->Pitch=1.f; W->SoundGroup=SOUNDGROUP_Effects;
 W->Priority=Event==TEXT("Frag")?90.f:(Event==TEXT("Flash")?80.f:(W->bLooping?15.f:45.f));
 W->LoadingBehavior=ESoundWaveLoadingBehavior::ForceInline; W->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM); W->PostEditChange();
 if(!Existing) FAssetRegistryModule::AssetCreated(W);
 if(W->NumChannels!=1 || W->Duration<=0.f || W->Duration>1.25f) return nullptr;
 return Save(W)?W:nullptr;
}
USoundAttenuation* Attenuation(const FString& Name,float Range,bool Replace)
{
 bool Created=false; auto* A=Asset<USoundAttenuation>(Name,Created);
 if(Created || Replace) {
  auto& S=A->Attenuation;
  S.bSpatialize=true; S.SpatializationAlgorithm=SPATIALIZATION_Default; S.bAttenuate=true;
  S.DistanceAlgorithm=EAttenuationDistanceModel::Logarithmic; S.AttenuationShape=EAttenuationShape::Sphere;
  S.AttenuationShapeExtents=FVector(60,0,0); S.FalloffDistance=Range-60.f;
  S.bEnableOcclusion=true; S.OcclusionTraceChannel=ECC_Visibility; S.OcclusionVolumeAttenuation=.25f;
  S.OcclusionLowPassFilterFrequency=1500.f; S.OcclusionInterpolationTime=.08f;
  S.bEnableReverbSend=false; S.bEnableListenerFocus=false;
  S.NonSpatializedRadiusStart=0.f; S.NonSpatializedRadiusEnd=0.f;
  if(!Save(A)) return nullptr;
 }
 return A;
}
}
#endif

UVanthexBuildGadgetPresentationCommandlet::UVanthexBuildGadgetPresentationCommandlet()
{
 IsClient=false; IsEditor=true; IsServer=false; LogToConsole=true;
}

int32 UVanthexBuildGadgetPresentationCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 const bool Replace=FParse::Param(*Params,TEXT("ReplacePrototypeAssets")); bool Created=false;
 auto* P=Asset<UVanthexGadgetPresentationProfile>(TEXT("DA_GadgetPresentation"),Created);
 const bool Update=Created || Replace;
 TMap<FName,TObjectPtr<USoundBase>> Sounds;
 for(const TCHAR* Event:{TEXT("Throw"),TEXT("FlightLoop"),TEXT("Bounce01"),TEXT("Bounce02"),TEXT("Bounce03"),TEXT("Flash"),TEXT("SmokeStart"),TEXT("SmokeLoop"),TEXT("SmokeEnd"),TEXT("Frag"),TEXT("EMP"),TEXT("EMPHumLoop"),TEXT("SensorStart"),TEXT("SensorPing"),TEXT("SensorEnd")}) {
  auto* W=Import(Event,Replace); if(!W) return 1; Sounds.Add(FName(Event),W);
 }
 auto* Dense=Material(TEXT("M_GadgetSmokeDense"),true,Replace);
 auto* Energy=Material(TEXT("M_GadgetEnergy"),false,Replace);
 auto* Near=Attenuation(TEXT("SA_GadgetNear"),1800.f,Replace);
 auto* Far=Attenuation(TEXT("SA_GadgetFar"),6500.f,Replace);
 auto* Concurrency=Asset<USoundConcurrency>(TEXT("SC_GadgetAudio"),Created);
 if(Created || Replace) {
  Concurrency->Concurrency.MaxCount=20; Concurrency->Concurrency.bLimitToOwner=false;
  Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew;
  Concurrency->Concurrency.VoiceStealReleaseTime=.01f; if(!Save(Concurrency)) return 1;
 }
 if(!Dense || !Energy || !Near || !Far) return 1;
 if(Update) {
  P->Sounds=Sounds; P->SmokeMaterial=Dense; P->EnergyMaterial=Energy; P->NearAttenuation=Near; P->FarAttenuation=Far; P->Concurrency=Concurrency;
  P->Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  P->Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  P->Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 }
 if(!P->Sphere || !P->Cylinder || !P->Cube || !Save(P)) return 1;
 UE_LOG(LogVanthex,Display,TEXT("GADGET_PRESENTATION_ASSETS PASS sounds=%d materials=2 profile=%s"),Sounds.Num(),*P->GetPathName());
 return 0;
#else
 return 1;
#endif
}
