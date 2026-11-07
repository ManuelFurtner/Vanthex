#include "CoreMinimal.h"
#include "Vanthex.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"

namespace {
FString BlenderAssetRoot=TEXT("/Game/Vanthex/Blender/");
template<class T>T* Expr(UMaterial* M){auto* E=NewObject<T>(M);M->GetExpressionCollection().AddExpression(E);return E;}
bool Save(UObject* Asset) {
 auto* P=Asset->GetPackage();P->MarkPackageDirty();
 const FString File=FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(P,Asset,*File,A);
}
template<class T>T* Asset(const FString& Name) {
 const FString Path=BlenderAssetRoot+Name;
 auto* P=CreatePackage(*Path);P->FullyLoad();
 if(auto* Old=FindObject<UObject>(P,*Name)){Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);Old->ClearFlags(RF_Public|RF_Standalone);}
 return NewObject<T>(P,FName(Name),RF_Public|RF_Standalone);
}
void Connect(FExpressionInput& In,UMaterialExpression* E,int32 Output=0){In.Expression=E;In.OutputIndex=Output;}
UMaterial* MaterialMaster() {
 auto* M=Asset<UMaterial>(TEXT("M_Industrial"));
 M->bTangentSpaceNormal=false;M->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);M->SetMaterialUsage(MATUSAGE_Nanite);
 auto* Pos=Expr<UMaterialExpressionWorldPosition>(M);
 auto* N=Expr<UMaterialExpressionVertexNormalWS>(M);
 auto* Color=Expr<UMaterialExpressionVectorParameter>(M);Color->ParameterName=TEXT("Tint");Color->DefaultValue=FLinearColor(.45f,.43f,.38f);
 auto* Rough=Expr<UMaterialExpressionScalarParameter>(M);Rough->ParameterName=TEXT("Roughness");Rough->DefaultValue=.7f;
 auto* Metal=Expr<UMaterialExpressionScalarParameter>(M);Metal->ParameterName=TEXT("Metallic");Metal->DefaultValue=0;
 auto* Glow=Expr<UMaterialExpressionScalarParameter>(M);Glow->ParameterName=TEXT("Glow");Glow->DefaultValue=0;
 auto* S=Expr<UMaterialExpressionCustom>(M);S->OutputType=CMOT_Float3;S->ContainsClipInstruction=CMCI_No;
 S->Description=TEXT("Original world-space microstructure, no licensed textures");
 S->Code=TEXT(R"(
struct Grain {
 float hash(float3 p){p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
 float noise(float3 p){float3 i=floor(p),f=frac(p);f=f*f*(3-2*f);
  return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+1),f.x),f.y),f.z);}
};
Grain g;
float broad=g.noise(P*.013),medium=g.noise(P*.16),fine=g.noise(P*1.8);
float pits=smoothstep(.72,.95,fine)*(1-Metallic);
float3 n=normalize(N),axis=abs(n.z)<.9?float3(0,0,1):float3(1,0,0);
float3 t=normalize(cross(axis,n)),b=cross(n,t);
float dx=g.noise((P+t*.15)*1.8)-fine,dy=g.noise((P+b*.15)*1.8)-fine;
Albedo=Tint*(.88+broad*.16+medium*.07)*(1-pits*.12);
Rough=saturate(Roughness+(fine-.5)*.13+(broad-.5)*.12);
float filter=rcp(1+length(ddx(P))*1.8+length(ddy(P))*1.8);
return normalize(n-(t*dx+b*dy)*lerp(.065,.018,Metallic)*filter);
)");
 const TCHAR* Names[]={TEXT("P"),TEXT("N"),TEXT("Tint"),TEXT("Roughness"),TEXT("Metallic")};
 UMaterialExpression* Inputs[]={Pos,N,Color,Rough,Metal};
 for(int32 I=0;I<5;++I){FCustomInput In;In.InputName=Names[I];Connect(In.Input,Inputs[I]);S->Inputs.Add(In);}
 FCustomOutput A;A.OutputName=TEXT("Albedo");A.OutputType=CMOT_Float3;S->AdditionalOutputs.Add(A);
 A.OutputName=TEXT("Rough");A.OutputType=CMOT_Float1;S->AdditionalOutputs.Add(A);S->RebuildOutputs();
 auto* Em=Expr<UMaterialExpressionMultiply>(M);Connect(Em->A,Color);Connect(Em->B,Glow);
 auto* E=M->GetEditorOnlyData();Connect(E->Normal,S);Connect(E->BaseColor,S,1);Connect(E->Roughness,S,2);Connect(E->Metallic,Metal);Connect(E->EmissiveColor,Em);
 M->PostEditChange();M->ForceRecompileForRendering();return Save(M)?M:nullptr;
}
}

bool VanthexImportBlenderIndustrial(bool Refined,bool PhotoDetails) {
 const bool ReuseMaterials=PhotoDetails||FParse::Param(FCommandLine::Get(),TEXT("NormalsOnly"));
 BlenderAssetRoot=PhotoDetails?TEXT("/Game/Vanthex/Blender/Photoreal/"):Refined?TEXT("/Game/Vanthex/Blender/Refinery/"):TEXT("/Game/Vanthex/Blender/");
 FString Text;const FString Source=FPaths::ProjectDir()/(PhotoDetails?TEXT("ArtSource/Photoreal/industrial.json"):Refined?TEXT("ArtSource/Refinery/industrial.json"):TEXT("ArtSource/Industrial/industrial.json"));
 if(!FFileHelper::LoadFileToString(Text,*Source)){UE_LOG(LogVanthex,Error,TEXT("Missing Blender export %s"),*Source);return false;}
 TSharedPtr<FJsonObject> Root;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root.IsValid())return false;
 if(Root->GetStringField(TEXT("units"))!=TEXT("centimetres"))return false;
 TMap<FString,UTexture2D*> Textures;
 if(Refined&&!ReuseMaterials)for(const FString Family:{TEXT("Concrete"),TEXT("Floor"),TEXT("Metal"),TEXT("Paint"),TEXT("Rubber")})for(const FString Suffix:{TEXT("BC"),TEXT("N"),TEXT("ORM")}) {
  const FString Name=TEXT("T_R_")+Family+TEXT("_")+Suffix;
  const FString File=FPaths::ProjectDir()/TEXT("ArtSource/Refinery/Textures")/(Name+TEXT(".png"));
  if(!IFileManager::Get().FileExists(*File))return false;
  const FString PackageName=BlenderAssetRoot+Name;auto* Package=CreatePackage(*PackageName);Package->FullyLoad();
  if(auto* Old=FindObject<UObject>(Package,*Name)){Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);Old->ClearFlags(RF_Public|RF_Standalone);}
  auto* Factory=NewObject<UTextureFactory>();Factory->SuppressImportOverwriteDialog();bool Cancelled=false;
  auto* T=Cast<UTexture2D>(Factory->ImportObject(UTexture2D::StaticClass(),Package,FName(Name),RF_Public|RF_Standalone,File,nullptr,Cancelled));
  if(!T||Cancelled)return false;
  T->SRGB=Suffix==TEXT("BC");T->CompressionSettings=Suffix==TEXT("N")?TC_Normalmap:Suffix==TEXT("ORM")?TC_Masks:TC_Default;T->MaxTextureSize=2048;
  T->PostEditChange();if(!Save(T))return false;Textures.Add(Name,T);
 }
 extern UMaterial* VanthexBuildRefineryMaterial(UTexture2D*,UTexture2D*,UTexture2D*);
 auto* Master=ReuseMaterials?nullptr:Refined?VanthexBuildRefineryMaterial(Textures.FindRef(TEXT("T_R_Concrete_BC")),Textures.FindRef(TEXT("T_R_Concrete_N")),Textures.FindRef(TEXT("T_R_Concrete_ORM"))):MaterialMaster();if(!Master&&!ReuseMaterials)return false;
 TArray<UMaterialInterface*> Materials;bool OK=true;
 for(const auto& Value:Root->GetArrayField(TEXT("materials"))) {
  const auto J=Value->AsObject();
  if(ReuseMaterials){const FString Name=(PhotoDetails?TEXT("MI_P_"):TEXT("MI_B_"))+J->GetStringField(TEXT("name"));auto* Existing=LoadObject<UMaterialInterface>(nullptr,*(BlenderAssetRoot+Name+TEXT(".")+Name));if(!Existing)return false;Materials.Add(Existing);continue;}
  auto* M=Asset<UMaterialInstanceConstant>(TEXT("MI_B_")+J->GetStringField(TEXT("name")));
  M->SetParentEditorOnly(Master);const auto C=J->GetArrayField(TEXT("color"));
  M->SetVectorParameterValueEditorOnly(TEXT("Tint"),FLinearColor(C[0]->AsNumber(),C[1]->AsNumber(),C[2]->AsNumber()));
  M->SetScalarParameterValueEditorOnly(TEXT("Roughness"),J->GetNumberField(TEXT("roughness")));
  M->SetScalarParameterValueEditorOnly(TEXT("Metallic"),J->GetNumberField(TEXT("metallic")));
  M->SetScalarParameterValueEditorOnly(TEXT("Glow"),J->GetNumberField(TEXT("emission")));
  if(Refined) {
   const FString Family=J->GetStringField(TEXT("texture_family"));
   M->SetTextureParameterValueEditorOnly(TEXT("BaseMap"),Textures.FindRef(TEXT("T_R_")+Family+TEXT("_BC")));
   M->SetTextureParameterValueEditorOnly(TEXT("NormalMap"),Textures.FindRef(TEXT("T_R_")+Family+TEXT("_N")));
   M->SetTextureParameterValueEditorOnly(TEXT("MaskMap"),Textures.FindRef(TEXT("T_R_")+Family+TEXT("_ORM")));
   M->SetScalarParameterValueEditorOnly(TEXT("Coated"),Family==TEXT("Paint")?1:0);
   M->SetScalarParameterValueEditorOnly(TEXT("TileCm"),Family==TEXT("Concrete")||Family==TEXT("Floor")?200:100);
  }
  M->PostEditChange();OK &= Save(M);Materials.Add(M);
 }
 int32 Total=0;
 for(const auto& Value:Root->GetArrayField(TEXT("meshes"))) {
  const auto J=Value->AsObject();const FString Name=J->GetStringField(TEXT("name"));
  if(!Name.StartsWith(TEXT("SM_B_")))return false;
  auto* Mesh=Asset<UStaticMesh>(Name);Mesh->SetNumSourceModels(1);
  auto* D=Mesh->CreateMeshDescription(0);FStaticMeshAttributes A(*D);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
  // Empty palette entries must not become render sections. In particular an
  // unused translucent Glass slot disables Nanite for an otherwise opaque mesh.
  TSet<int32> Used;
  for(const auto& T:J->GetArrayField(TEXT("triangles")))Used.Add(T->AsObject()->GetIntegerField(TEXT("m")));
  TArray<FPolygonGroupID> Groups;Groups.Init(FPolygonGroupID(INDEX_NONE),Materials.Num());TArray<FStaticMaterial> Slots;
  for(int32 I=0;I<Materials.Num();++I)if(Used.Contains(I)){auto G=D->CreatePolygonGroup();Groups[I]=G;A.GetPolygonGroupMaterialSlotNames()[G]=Materials[I]->GetFName();FStaticMaterial S(Materials[I]);S.MaterialSlotName=Materials[I]->GetFName();S.UVChannelData.bInitialized=true;Slots.Add(S);}
  for(const auto& Triangle:J->GetArrayField(TEXT("triangles"))) {
   const auto T=Triangle->AsObject();const int32 Slot=T->GetIntegerField(TEXT("m"));if(!Groups.IsValidIndex(Slot))return false;
   TArray<FVertexInstanceID> VI;
   for(const auto& Corner:T->GetArrayField(TEXT("v"))) {
    const auto V=Corner->AsArray();if(V.Num()!=6)return false;
    FVector3f P(V[0]->AsNumber(),-V[1]->AsNumber(),V[2]->AsNumber()),N(V[3]->AsNumber(),-V[4]->AsNumber(),V[5]->AsNumber());
    const FVertexID Id=D->CreateVertex();A.GetVertexPositions()[Id]=P;
    const auto Instance=D->CreateVertexInstance(Id);A.GetVertexInstanceNormals()[Instance]=N;
    const FVector3f AbsN=N.GetAbs();
    const FVector2f UV=AbsN.Z>=AbsN.X&&AbsN.Z>=AbsN.Y?FVector2f(P.X,P.Y):AbsN.X>AbsN.Y?FVector2f(P.Y,P.Z):FVector2f(P.X,P.Z);
    A.GetVertexInstanceUVs().Set(Instance,0,UV*.01f);VI.Add(Instance);
   }
   D->CreateTriangle(Groups[Slot],VI);++Total;
  }
  Mesh->CommitMeshDescription(0);Mesh->SetStaticMaterials(Slots);
  auto& B=Mesh->GetSourceModel(0).BuildSettings;B.bRecomputeNormals=false;B.bRecomputeTangents=true;B.bGenerateLightmapUVs=false;B.bRemoveDegenerates=true;
  auto Nanite=Mesh->GetNaniteSettings();Nanite.bEnabled=Name!=TEXT("SM_B_PhotoMonitor");Mesh->SetNaniteSettings(Nanite);
  Mesh->CreateBodySetup();Mesh->GetBodySetup()->DefaultInstance.SetCollisionProfileName(TEXT("NoCollision"));Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseSimpleAsComplex;
  Mesh->Build(false);Mesh->PostEditChange();OK &= Save(Mesh);FAssetRegistryModule::AssetCreated(Mesh);
  UE_LOG(LogVanthex,Display,TEXT("BLENDER_IMPORT %s triangles=%d saved=%d"),*Name,D->Triangles().Num(),OK);
 }
 FAssetCompilingManager::Get().FinishAllCompilation();
 UE_LOG(LogVanthex,Display,TEXT("BLENDER_IMPORT %s totalTriangles=%d source=%s"),OK?TEXT("PASS"):TEXT("FAIL"),Total,*Root->GetStringField(TEXT("generator")));
 return OK;
}
#endif
