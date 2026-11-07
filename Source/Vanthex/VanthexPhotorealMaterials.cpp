#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Vanthex.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Factories/TextureFactory.h"

bool VanthexInspectPhotorealTextures()
{
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Photorealism/TextureAudit");
 IFileManager::Get().MakeDirectory(*Folder,true);bool OK=true;
 for(const FString Name:{TEXT("T_ConcretePillar01"),TEXT("T_FactoryConcreteFloor01"),TEXT("T_ConcreteFloor_02"),TEXT("T_FrontConcrete01"),TEXT("T_Metalbeam01"),TEXT("T_MetalFrame_01"),TEXT("T_RubberFloor"),TEXT("T_PlasticBoxes")}) {
  for(const FString Suffix:{TEXT("BC"),TEXT("N"),TEXT("ORM")}) {
   const FString Asset=Name+TEXT("_")+Suffix;
   auto* Texture=LoadObject<UTexture2D>(nullptr,*(TEXT("/Game/Textures/")+Asset+TEXT(".")+Asset));
   if(!Texture){OK=false;continue;}
   UE_LOG(LogVanthex,Display,TEXT("PBR_AUDIT %s %dx%d source=%dx%d SRGB=%d compression=%d"),*Asset,Texture->GetSizeX(),Texture->GetSizeY(),Texture->Source.GetSizeX(),Texture->Source.GetSizeY(),Texture->SRGB,int32(Texture->CompressionSettings));
   if(Suffix==TEXT("BC")) {
    FImage Source;
    if(Texture->Source.GetMipImage(Source,0))OK &= FImageUtils::SaveImageByExtension(*(Folder/(Asset+TEXT(".png"))),Source);
   }
  }
 }
 return OK;
}

namespace {
const FString PhotoRoot=TEXT("/Game/Vanthex/Blender/Photoreal/");
template<class T>T* PhotoAsset(const FString& Name){auto* P=CreatePackage(*(PhotoRoot+Name));P->FullyLoad();if(auto* A=FindObject<T>(P,*Name))return A;return NewObject<T>(P,*Name,RF_Public|RF_Standalone);}
template<class T>T* PhotoExpr(UMaterial* M){auto* E=NewObject<T>(M);M->GetExpressionCollection().AddExpression(E);return E;}
void PhotoLink(FExpressionInput& I,UMaterialExpression* E,int32 Output=0){I.Expression=E;I.OutputIndex=Output;}
bool PhotoSave(UObject* O){const auto File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;return UPackage::SavePackage(O->GetOutermost(),O,*File,A);}
UTexture2D* PhotoTexture(const FString& Name,bool Licensed=false){const FString P=(Licensed?TEXT("/Game/Textures/"):Name.StartsWith(TEXT("T_P_"))?PhotoRoot:TEXT("/Game/Vanthex/Blender/Refinery/"))+Name;return LoadObject<UTexture2D>(nullptr,*(P+TEXT(".")+Name));}
}

bool VanthexBuildPhotorealMaterials()
{
 for(const FString Suffix:{TEXT("BC"),TEXT("N"),TEXT("ORM")}) {
  const FString Name=TEXT("T_P_Zinc_")+Suffix;const FString File=FPaths::ProjectDir()/TEXT("ArtSource/Photoreal/Textures")/(Name+TEXT(".png"));
  if(!IFileManager::Get().FileExists(*File))return false;
  auto* Pkg=CreatePackage(*(PhotoRoot+Name));Pkg->FullyLoad();auto* Factory=NewObject<UTextureFactory>();Factory->SuppressImportOverwriteDialog();bool Cancelled=false;
  auto* T=Cast<UTexture2D>(Factory->ImportObject(UTexture2D::StaticClass(),Pkg,FName(Name),RF_Public|RF_Standalone,File,nullptr,Cancelled));if(!T||Cancelled)return false;
  T->SRGB=Suffix==TEXT("BC");T->CompressionSettings=Suffix==TEXT("N")?TC_Normalmap:Suffix==TEXT("ORM")?TC_Masks:TC_Default;T->MaxTextureSize=2048;T->PostEditChange();if(!PhotoSave(T))return false;
 }
 // Separate material instances: no edits or copies of the original licensed maps
 // or baseline mesh assets. Texture atlases were inspected and rejected for tiling.
 auto BuildMaster=[&](bool Licensed,bool Glass=false)->UMaterial* {
 auto* M=PhotoAsset<UMaterial>(Glass?TEXT("M_InstrumentGlass"):Licensed?TEXT("M_PhysicallyConstructed_LinearMask"):TEXT("M_PhysicallyConstructed"));M->GetExpressionCollection().Empty();
 if(Glass){M->BlendMode=BLEND_Translucent;M->TranslucencyLightingMode=TLM_SurfacePerPixelLighting;M->TwoSided=true;}
 M->bTangentSpaceNormal=false;M->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);M->SetMaterialUsage(MATUSAGE_Nanite);
 auto Scalar=[&](const TCHAR* Name,float V){auto* E=PhotoExpr<UMaterialExpressionScalarParameter>(M);E->ParameterName=Name;E->DefaultValue=V;return E;};
 auto* P=PhotoExpr<UMaterialExpressionWorldPosition>(M);auto* N=PhotoExpr<UMaterialExpressionVertexNormalWS>(M);
 auto* Tint=PhotoExpr<UMaterialExpressionVectorParameter>(M);Tint->ParameterName=TEXT("Tint");Tint->DefaultValue=FLinearColor(.4f,.42f,.4f);
 auto* Tile=Scalar(TEXT("TileCm"),240);auto* Rough=Scalar(TEXT("Roughness"),.7f);auto* Metal=Scalar(TEXT("Metallic"),0);
 auto* SourceColor=Scalar(TEXT("SourceColor"),1);auto* NormalStrength=Scalar(TEXT("NormalStrength"),.4f);
 auto* Coated=Scalar(TEXT("Coated"),0);auto* Glow=Scalar(TEXT("Glow"),0);auto* Pulse=Scalar(TEXT("Pulse"),0);
 auto* Aniso=Scalar(TEXT("Anisotropy"),0);auto* Time=PhotoExpr<UMaterialExpressionTime>(M);
 auto* UV=PhotoExpr<UMaterialExpressionCustom>(M);UV->OutputType=CMOT_Float2;
 UV->Code=TEXT("float3 a=abs(N); return (a.z>=a.x&&a.z>=a.y?float2(P.x,P.y*sign(N.z)):a.x>a.y?float2(P.y,P.z*sign(N.x)):float2(-P.x,P.z*sign(N.y)))/max(Tile,1.0f);");
 const TCHAR* UName[]={TEXT("P"),TEXT("N"),TEXT("Tile")};UMaterialExpression* UValue[]={P,N,Tile};
 for(int32 I=0;I<3;++I){FCustomInput In;In.InputName=UName[I];PhotoLink(In.Input,UValue[I]);UV->Inputs.Add(In);}
 auto Sample=[&](const TCHAR* Name,UTexture2D* T,EMaterialSamplerType Type){auto* E=PhotoExpr<UMaterialExpressionTextureSampleParameter2D>(M);E->ParameterName=Name;E->Texture=T;E->SamplerType=Type;PhotoLink(E->Coordinates,UV);return E;};
 const FString Default=Licensed?TEXT("T_FactoryConcreteFloor01"):TEXT("T_R_Concrete");
 auto* BC=Sample(TEXT("BaseMap"),PhotoTexture(Default+TEXT("_BC"),Licensed),SAMPLERTYPE_Color);
 auto* NM=Sample(TEXT("NormalMap"),PhotoTexture(Default+TEXT("_N"),Licensed),SAMPLERTYPE_Normal);
 auto* ORM=Sample(TEXT("MaskMap"),PhotoTexture(Default+TEXT("_ORM"),Licensed),Licensed?SAMPLERTYPE_LinearColor:SAMPLERTYPE_Masks);
 if(!BC->Texture||!NM->Texture||!ORM->Texture)return nullptr;
 auto* S=PhotoExpr<UMaterialExpressionCustom>(M);S->OutputType=CMOT_Float3;S->Description=TEXT("Physical-scale PBR, signed projection basis, restrained construction wear");
 S->Code=TEXT(R"(
float3 n=normalize(N), a=abs(n);
bool z=a.z>=a.x&&a.z>=a.y; bool x=!z&&a.x>a.y;
float3 t=z?float3(1,0,0):x?float3(0,1,0):float3(-1,0,0);
float3 b=z?float3(0,sign(n.z),0):x?float3(0,0,sign(n.x)):float3(0,0,sign(n.y));
float luma=dot(Color,float3(.2126f,.7152f,.0722f));
float3 base=lerp(luma.xxx,Color,SourceColor)*Tint;
float edge=Coated*smoothstep(.035f,.24f,1-max(a.x,max(a.y,a.z)))*.12f;
Albedo=lerp(base,float3(.26f,.28f,.27f),edge);
// Preserve full measured-map roughness variation instead of reducing it to 22%.
Rough=clamp(Roughness+(Mask.g-.5f)*.65f,.24f,.94f);
Metal=lerp(Metallic, .82f, edge);
Occlusion=lerp(1,saturate(Mask.r),.7f);
Tangent=t;
Emission=Tint*Glow*(1+Pulse*sin(Time*1.6f));
float3 mapped=normalize(t*Normal.x*NormalStrength+b*Normal.y*NormalStrength+n*max(Normal.z,.25f));
return mapped;
)");
 const TCHAR* Names[]={TEXT("N"),TEXT("Color"),TEXT("Normal"),TEXT("Mask"),TEXT("Tint"),TEXT("Roughness"),TEXT("Metallic"),TEXT("SourceColor"),TEXT("NormalStrength"),TEXT("Coated"),TEXT("Glow"),TEXT("Pulse"),TEXT("Time")};
 UMaterialExpression* Values[]={N,BC,NM,ORM,Tint,Rough,Metal,SourceColor,NormalStrength,Coated,Glow,Pulse,Time};
 for(int32 I=0;I<13;++I){FCustomInput In;In.InputName=Names[I];PhotoLink(In.Input,Values[I]);S->Inputs.Add(In);}
 for(const auto& Name:{TEXT("Albedo"),TEXT("Rough"),TEXT("Metal"),TEXT("Occlusion"),TEXT("Tangent"),TEXT("Emission")}){FCustomOutput O;O.OutputName=Name;O.OutputType=FCString::Strcmp(Name,TEXT("Albedo"))==0||FCString::Strcmp(Name,TEXT("Tangent"))==0||FCString::Strcmp(Name,TEXT("Emission"))==0?CMOT_Float3:CMOT_Float1;S->AdditionalOutputs.Add(O);}S->RebuildOutputs();
 auto* E=M->GetEditorOnlyData();PhotoLink(E->BaseColor,S,1);PhotoLink(E->Normal,S);PhotoLink(E->Roughness,S,2);PhotoLink(E->Metallic,S,3);PhotoLink(E->AmbientOcclusion,S,4);PhotoLink(E->Tangent,S,5);PhotoLink(E->EmissiveColor,S,6);PhotoLink(E->Anisotropy,Aniso);
 if(Glass)PhotoLink(E->Opacity,Scalar(TEXT("Opacity"),.12f));
 M->PostEditChange();M->ForceRecompileForRendering();return PhotoSave(M)?M:nullptr;
 };
 auto* M=BuildMaster(false);auto* LicensedMaster=BuildMaster(true);auto* GlassMaster=BuildMaster(false,true);if(!M||!LicensedMaster||!GlassMaster)return false;
 FString Text;if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("ArtSource/Refinery/industrial.json"))))return false;
 TSharedPtr<FJsonObject> Json;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json))return false;
 bool OK=true;
 for(const auto& Entry:Json->GetArrayField(TEXT("materials"))) {
  const auto J=Entry->AsObject();const FString Name=J->GetStringField(TEXT("name"));const auto C=J->GetArrayField(TEXT("color"));
  FLinearColor Color(C[0]->AsNumber(),C[1]->AsNumber(),C[2]->AsNumber());
  FString Texture=TEXT("T_R_Paint");bool Licensed=false;float TileCm=100,Roughness=.52f,Metallic=0,Strength=.25f,Coat=1,ColorMix=0,Anisotropy=0;
  if(Name==TEXT("Concrete")){Texture=TEXT("T_FrontConcrete01");Licensed=true;TileCm=400;Color=FLinearColor(.62f,.635f,.60f);Roughness=.79f;Strength=.42f;Coat=0;ColorMix=.7f;}
  else if(Name==TEXT("Floor")){Texture=TEXT("T_FactoryConcreteFloor01");Licensed=true;TileCm=400;Color=FLinearColor(.56f,.57f,.54f);Roughness=.64f;Strength=.55f;Coat=0;ColorMix=.7f;}
  else if(Name==TEXT("Aluminium")||Name==TEXT("Steel")||Name==TEXT("Copper")){Texture=Name==TEXT("Steel")?TEXT("T_P_Zinc"):TEXT("T_R_Metal");Metallic=Name==TEXT("Steel")?.90f:.97f;Color=Name==TEXT("Copper")?FLinearColor(.83f,.51f,.27f):Name==TEXT("Steel")?FLinearColor(.42f,.46f,.47f):FLinearColor(.68f,.71f,.73f);Roughness=Name==TEXT("Aluminium")?.43f:.51f;Strength=.10f;Coat=0;Anisotropy=0;}
  else if(Name==TEXT("Rubber")||Name==TEXT("Stair")){Texture=TEXT("T_R_Rubber");Color=Name==TEXT("Stair")?FLinearColor(.11f,.125f,.12f):FLinearColor(.028f,.033f,.032f);Roughness=.81f;Strength=.15f;Coat=0;}
  else if(Name==TEXT("Glass")){Texture=TEXT("T_R_Rubber");Color=FLinearColor(.08f,.11f,.12f);Roughness=.18f;Coat=0;Strength=.03f;}
  else if(Name==TEXT("Ivory")){Color=FLinearColor(.64f,.67f,.64f);Roughness=.43f;}
  else if(Name==TEXT("Light")||Name==TEXT("AmberLight")||Name==TEXT("Screen")){Texture=TEXT("T_R_Rubber");Roughness=.32f;Coat=0;Strength=0;}
  auto* MI=PhotoAsset<UMaterialInstanceConstant>(TEXT("MI_P_")+Name);MI->SetParentEditorOnly(Name==TEXT("Glass")?GlassMaster:Licensed?LicensedMaster:M);
  MI->SetVectorParameterValueEditorOnly(TEXT("Tint"),Color);
  for(const auto& Suffix:{TEXT("BC"),TEXT("N"),TEXT("ORM")}){
   const FString Slot=FCString::Strcmp(Suffix,TEXT("BC"))==0?TEXT("BaseMap"):FCString::Strcmp(Suffix,TEXT("N"))==0?TEXT("NormalMap"):TEXT("MaskMap");auto* T=PhotoTexture(Texture+TEXT("_")+Suffix,Licensed);if(!T)return false;MI->SetTextureParameterValueEditorOnly(*Slot,T);
  }
  const TCHAR* Params[]={TEXT("TileCm"),TEXT("Roughness"),TEXT("Metallic"),TEXT("NormalStrength"),TEXT("Coated"),TEXT("SourceColor"),TEXT("Anisotropy"),TEXT("Glow"),TEXT("Pulse")};
  const float V[]={TileCm,Roughness,Metallic,Strength,Coat,ColorMix,Anisotropy,float(J->GetNumberField(TEXT("emission")))*.45f,Name==TEXT("Screen")?.035f:0};
  for(int32 I=0;I<9;++I)MI->SetScalarParameterValueEditorOnly(Params[I],V[I]);
  MI->PostEditChange();OK &= PhotoSave(MI);UE_LOG(LogVanthex,Display,TEXT("PHOTO_MATERIAL %s source=%s tile=%.0fcm"),*Name,*Texture,TileCm);
 }
 auto* Plastic=PhotoAsset<UMaterialInstanceConstant>(TEXT("MI_P_Plastic"));Plastic->SetParentEditorOnly(M);
 Plastic->SetVectorParameterValueEditorOnly(TEXT("Tint"),FLinearColor(.035f,.047f,.052f));
 Plastic->SetScalarParameterValueEditorOnly(TEXT("Roughness"),.42f);Plastic->SetScalarParameterValueEditorOnly(TEXT("Metallic"),0);
 Plastic->SetScalarParameterValueEditorOnly(TEXT("Coated"),0);Plastic->SetScalarParameterValueEditorOnly(TEXT("NormalStrength"),.12f);
 Plastic->SetScalarParameterValueEditorOnly(TEXT("TileCm"),80);Plastic->SetScalarParameterValueEditorOnly(TEXT("SourceColor"),0);
 for(const auto& Suffix:{TEXT("BC"),TEXT("N"),TEXT("ORM")})Plastic->SetTextureParameterValueEditorOnly(FCString::Strcmp(Suffix,TEXT("BC"))==0?TEXT("BaseMap"):FCString::Strcmp(Suffix,TEXT("N"))==0?TEXT("NormalMap"):TEXT("MaskMap"),PhotoTexture(FString(TEXT("T_R_Rubber_"))+Suffix));
 Plastic->PostEditChange();OK &= PhotoSave(Plastic);
 return OK;
}
#endif
