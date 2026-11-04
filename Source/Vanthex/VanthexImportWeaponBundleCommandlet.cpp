#include "VanthexImportWeaponBundleCommandlet.h"
#include "VanthexImportedWeaponSet.h"
#include "Vanthex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "StaticMeshAttributes.h"
#include "UObject/SavePackage.h"

namespace VanthexBundleImport
{
constexpr const TCHAR* Destination=TEXT("/Game/Vanthex/Imported/Weapons");
struct FTriangle
{
 FSoftSkinVertex Vertices[3];
 int32 Material=0;
};

template<typename T> T* Create(const FString& Folder,const FString& Name)
{
 const FString PackageName=Folder/Name;
 UPackage* Package=FPackageName::DoesPackageExist(PackageName)?LoadPackage(nullptr,*PackageName,LOAD_None):CreatePackage(*PackageName);
 if(!Package) Package=CreatePackage(*PackageName);
 Package->FullyLoad();
 // Import may run repeatedly; reset only our own derived asset, never the licensed source.
 if(UObject* Existing=FindObject<UObject>(Package,*Name)) {
  Existing->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
  Existing->ClearFlags(RF_Public|RF_Standalone);
 }
 return NewObject<T>(Package,FName(Name),RF_Public|RF_Standalone);
}

bool Save(UObject* Asset)
{
 if(!Asset) return false;
 UPackage* Package=Asset->GetPackage(); Package->MarkPackageDirty();
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
 const FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
 const bool Saved=UPackage::SavePackage(Package,Asset,*Filename,Args);
 if(Saved) FAssetRegistryModule::AssetCreated(Asset);
 return Saved;
}

int32 StrongestBone(const FSoftSkinVertex& Vertex,const FSkelMeshSection& Section)
{
 int32 Influence=0;
 for(int32 I=1;I<MAX_TOTAL_INFLUENCES;++I) if(Vertex.InfluenceWeights[I]>Vertex.InfluenceWeights[Influence]) Influence=I;
 const int32 Index=Vertex.InfluenceBones[Influence];
 return Section.BoneMap.IsValidIndex(Index)?Section.BoneMap[Index]:INDEX_NONE;
}

// 0=body, 1=magazine (including its child rounds), 2=bolt/charging handle, -1=hidden spent case.
int32 PartForBone(int32 Bone,const FReferenceSkeleton& Skeleton,FName WeaponId,bool AmmoMaterial)
{
 if(Bone==INDEX_NONE) return 0;
 const FName Name=Skeleton.GetBoneName(Bone);
 if(Name==TEXT("b_gun_shelleject")&&AmmoMaterial) return -1;
 if(Name==TEXT("b_gun_chargehandle")||(WeaponId==TEXT("AK")&&Name==TEXT("b_gun_shelleject"))) return 2;
 for(int32 I=Bone;I!=INDEX_NONE;I=Skeleton.GetParentIndex(I))
  if(Skeleton.GetBoneName(I)==TEXT("b_gun_mag")) return 1;
 return 0;
}

UStaticMesh* BuildPart(FName WeaponId,const TCHAR* Part,const TArray<FTriangle>& Triangles,
 const TArray<FSkeletalMaterial>& Materials,const FTransform& Transform,int32 UVChannels,TArray<FString>& Report)
{
 if(Triangles.IsEmpty()) return nullptr;
 const FString Name=FString::Printf(TEXT("SM_%s_%s"),*WeaponId.ToString(),Part);
 UStaticMesh* Mesh=Create<UStaticMesh>(FString(Destination)/TEXT("Meshes"),Name);
 Mesh->SetNumSourceModels(2);
 FMeshDescription* Description=Mesh->CreateMeshDescription(0);
 FStaticMeshAttributes Attributes(*Description); Attributes.Register();
 auto Normals=Attributes.GetVertexInstanceNormals(); auto Tangents=Attributes.GetVertexInstanceTangents();
 auto Signs=Attributes.GetVertexInstanceBinormalSigns(); auto Colors=Attributes.GetVertexInstanceColors();
 auto UVs=Attributes.GetVertexInstanceUVs(); UVs.SetNumChannels(UVChannels);
 TArray<FPolygonGroupID> Groups; TArray<FStaticMaterial> Slots;
 for(const auto& Material:Materials) {
  Groups.Add(Description->CreatePolygonGroup());
  Attributes.GetPolygonGroupMaterialSlotNames()[Groups.Last()]=Material.MaterialSlotName;
  FStaticMaterial Slot(Material.MaterialInterface,Material.MaterialSlotName); Slot.UVChannelData.bInitialized=true; Slots.Add(Slot);
 }
 for(const FTriangle& Triangle:Triangles) {
  TArray<FVertexInstanceID> Instances;
  for(const FSoftSkinVertex& Source:Triangle.Vertices) {
   const auto Vertex=Description->CreateVertex();
   Description->GetVertexPositions()[Vertex]=FVector3f(Transform.TransformPosition(FVector(Source.Position)));
   const auto Instance=Description->CreateVertexInstance(Vertex); Instances.Add(Instance);
   const FVector3f SourceNormal(Source.TangentZ.X,Source.TangentZ.Y,Source.TangentZ.Z);
   Normals[Instance]=FVector3f(Transform.TransformVectorNoScale(FVector(SourceNormal)).GetSafeNormal());
   Tangents[Instance]=FVector3f(Transform.TransformVectorNoScale(FVector(Source.TangentX)).GetSafeNormal());
   Signs[Instance]=FVector3f::DotProduct(FVector3f::CrossProduct(SourceNormal,Source.TangentX),Source.TangentY)<0?-1.f:1.f;
   const FLinearColor Color=FLinearColor(Source.Color); Colors[Instance]=FVector4f(Color.R,Color.G,Color.B,Color.A);
   for(int32 Channel=0;Channel<UVChannels;++Channel) UVs.Set(Instance,Channel,Source.UVs[Channel]);
  }
  Description->CreateTriangle(Groups[Triangle.Material],Instances);
 }
 Mesh->SetStaticMaterials(Slots); Mesh->CommitMeshDescription(0);
 auto& Settings=Mesh->GetSourceModel(0).BuildSettings;
 Settings.bRecomputeNormals=false; Settings.bRecomputeTangents=false; Settings.bGenerateLightmapUVs=false;
 Settings.bRemoveDegenerates=true; Settings.bUseFullPrecisionUVs=true;
 Mesh->GetSourceModel(1).ReductionSettings.PercentTriangles=.45f;
 Mesh->GetSourceModel(1).ReductionSettings.BaseLODModel=0; Mesh->GetSourceModel(1).ScreenSize.Default=.16f;
 Mesh->CreateBodySetup(); Mesh->GetBodySetup()->DefaultInstance.SetCollisionProfileName(TEXT("NoCollision"));
 auto Nanite=Mesh->GetNaniteSettings(); Nanite.bEnabled=false; Mesh->SetNaniteSettings(Nanite);
 Mesh->Build(false); Mesh->PostEditChange();
 const bool Saved=Save(Mesh);
 Report.Add(FString::Printf(TEXT("%s %s triangles=%d materials=%d bounds=(%s)"),Saved?TEXT("PASS"):TEXT("FAIL"),*Name,Triangles.Num(),Slots.Num(),*Mesh->GetBoundingBox().GetSize().ToString()));
 return Saved?Mesh:nullptr;
}

bool Import(FName WeaponId,const TCHAR* SourcePath,FVanthexImportedWeaponModel& Model,TArray<FString>& Report)
{
 USkeletalMesh* Source=LoadObject<USkeletalMesh>(nullptr,SourcePath);
 if(!Source||!Source->GetImportedModel()||Source->GetImportedModel()->LODModels.IsEmpty()) {
  Report.Add(FString::Printf(TEXT("FAIL missing source %s"),SourcePath)); return false;
 }
 const bool Knife=WeaponId==TEXT("Knife");
 const FTransform Transform=Knife?FTransform(FRotator(90,0,0),FVector(-4,0,0)):
  FTransform(FQuat::Identity,FVector(-1,0,-3.2),FVector(1.0/1.7));
 const auto& Lod=Source->GetImportedModel()->LODModels[0]; const auto& Skeleton=Source->GetRefSkeleton();
 TArray<FTriangle> Parts[3]; int32 HiddenTriangles=0;
 for(const auto& Section:Lod.Sections) {
  if(!Source->GetMaterials().IsValidIndex(Section.MaterialIndex)) return false;
  const auto* Material=Source->GetMaterials()[Section.MaterialIndex].MaterialInterface.Get();
  const bool AmmoMaterial=GetPathNameSafe(Material).Contains(TEXT("/Ammunition/"));
  for(uint32 I=0;I<Section.NumTriangles;++I) {
   FTriangle Triangle; Triangle.Material=Section.MaterialIndex; int32 PartVotes[3]={0,0,0}; int32 HiddenVotes=0;
   for(uint32 Corner=0;Corner<3;++Corner) {
    const uint32 Index=Section.BaseIndex+I*3+Corner;
    if(!Lod.IndexBuffer.IsValidIndex(Index)) return false;
    const int32 VertexIndex=int32(Lod.IndexBuffer[Index])-int32(Section.BaseVertexIndex);
    if(!Section.SoftVertices.IsValidIndex(VertexIndex)) return false;
    Triangle.Vertices[Corner]=Section.SoftVertices[VertexIndex];
    const int32 Part=PartForBone(StrongestBone(Triangle.Vertices[Corner],Section),Skeleton,WeaponId,AmmoMaterial);
    if(Part<0) ++HiddenVotes; else ++PartVotes[Part];
   }
   if(HiddenVotes>=2) { ++HiddenTriangles; continue; }
   int32 Part=0; for(int32 P=1;P<3;++P) if(PartVotes[P]>PartVotes[Part]) Part=P;
   Parts[Part].Add(Triangle);
  }
 }
 Model.WeaponId=WeaponId;
 const int32 UVChannels=FMath::Clamp(int32(Lod.NumTexCoords),1,int32(MAX_TEXCOORDS));
 Model.Body=BuildPart(WeaponId,TEXT("Body"),Parts[0],Source->GetMaterials(),Transform,UVChannels,Report);
 if(!Knife) {
  Model.Magazine=BuildPart(WeaponId,TEXT("Magazine"),Parts[1],Source->GetMaterials(),Transform,UVChannels,Report);
  Model.Bolt=BuildPart(WeaponId,TEXT("Bolt"),Parts[2],Source->GetMaterials(),Transform,UVChannels,Report);
 }
 if(Knife) { Model.Muzzle=Transform.TransformPosition(FVector(0,0,-24.285)); Model.SightPoint=FVector::ZeroVector; }
 else {
  // Component-space socket locations and real aperture/post vertices measured by the inspector.
  const bool M4=WeaponId==TEXT("M4");
  const FVector SourceMuzzle=M4?FVector(59.460723,0,10.578197):FVector(61.854556,0,8.179154);
  const FVector Rear=M4?FVector(2.624,0,17.4425):FVector(20.34,0,13.68);
  const FVector Front=M4?FVector(41.418,0,17.085):FVector(59.278,0,13.303);
  Model.Muzzle=Transform.TransformPosition(SourceMuzzle); Model.SightPoint=Transform.TransformPosition(Rear);
  Model.SightPitch=FMath::RadiansToDegrees(FMath::Atan2(Rear.Z-Front.Z,Front.X-Rear.X)); Model.bReflexSight=false;
 }
 Report.Add(FString::Printf(TEXT("MODEL %s source=%s hiddenSpentCaseTriangles=%d muzzle=(%s) sight=(%s) pitch=%.6f"),
  *WeaponId.ToString(),SourcePath,HiddenTriangles,*Model.Muzzle.ToString(),*Model.SightPoint.ToString(),Model.SightPitch));
 return Model.Body&&(Knife||Model.Magazine);
}
}
#endif

UVanthexImportWeaponBundleCommandlet::UVanthexImportWeaponBundleCommandlet()
{ IsClient=false; IsEditor=true; IsServer=false; LogToConsole=true; }

int32 UVanthexImportWeaponBundleCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 using namespace VanthexBundleImport;
 TArray<FString> Report; bool OK=true;
 auto* Set=Create<UVanthexImportedWeaponSet>(Destination,TEXT("DA_WeaponBundle"));
 const FName Ids[]={TEXT("M4"),TEXT("AK"),TEXT("Knife")};
 const TCHAR* Paths[]={
  TEXT("/Game/FPS_Weapon_Bundle/Weapons/Meshes/AR4/SK_AR4_X.SK_AR4_X"),
  TEXT("/Game/FPS_Weapon_Bundle/Weapons/Meshes/Ka47/SK_KA47_X.SK_KA47_X"),
  TEXT("/Game/FPS_Weapon_Bundle/Weapons/Meshes/M9_Knife/SK_M9_Knife_X.SK_M9_Knife_X")};
 for(int32 I=0;I<UE_ARRAY_COUNT(Ids);++I) {
  FVanthexImportedWeaponModel Model; const bool Imported=Import(Ids[I],Paths[I],Model,Report);
  OK&=Imported; if(Imported) Set->Models.Add(Model);
 }
 // Never publish a partial catalog: runtime will retain the previous complete set or own models.
 if(OK) OK=Save(Set);
 Report.Insert(FString::Printf(TEXT("OVERALL %s models=%d. Licensed derived assets are local-only under /Game/Vanthex/Imported."),OK?TEXT("PASS"):TEXT("FAIL"),Set->Models.Num()),0);
 FFileHelper::SaveStringArrayToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("WeaponBundle-import.txt")));
 UE_LOG(LogVanthex,Display,TEXT("Optional FPS Weapon Bundle import %s (%d models)"),OK?TEXT("PASS"):TEXT("FAIL"),Set->Models.Num());
 return OK?0:1;
#else
 return 1;
#endif
}
