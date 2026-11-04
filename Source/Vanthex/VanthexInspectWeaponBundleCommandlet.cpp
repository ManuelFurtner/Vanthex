#include "VanthexInspectWeaponBundleCommandlet.h"
#include "Vanthex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "StaticMeshAttributes.h"

namespace VanthexBundleInspection
{
void AppendBounds(TArray<FString>& Lines,const FBoxSphereBounds& Bounds)
{
 Lines.Add(FString::Printf(TEXT("  Bounds origin=(%s) size=(%s) min=(%s) max=(%s)"),
  *Bounds.Origin.ToString(),*(Bounds.BoxExtent*2).ToString(),*(Bounds.Origin-Bounds.BoxExtent).ToString(),*(Bounds.Origin+Bounds.BoxExtent).ToString()));
}

// Original asset data remains untouched. These snapshots help establish exact axes and grip placement.
void ExportReferenceGeometry(UStaticMesh* Mesh,TArray<FString>& Lines)
{
 const FString Name=Mesh->GetName();
 if(Name!=TEXT("SM_AR4_X")&&Name!=TEXT("SM_KA47_X")&&Name!=TEXT("SM_M9_Knife_X")&&
    Name!=TEXT("SM_T4_Sight")&&Name!=TEXT("SM_Scope_25x56_X")) return;
 const FMeshDescription* Description=Mesh->GetMeshDescription(0);
 if(!Description) { Lines.Add(TEXT("  Geometry snapshot unavailable: no imported mesh description")); return; }
 const FStaticMeshConstAttributes Attributes(*Description);
 const auto Positions=Attributes.GetVertexPositions();
 const auto Normals=Attributes.GetVertexInstanceNormals();
 const auto UVs=Attributes.GetVertexInstanceUVs();
 TArray<FString> Obj; Obj.Add(TEXT("# Inspection snapshot of locally installed licensed asset. Do not redistribute."));
 int32 VertexNumber=1;
 for(const FTriangleID Triangle:Description->Triangles().GetElementIDs()) {
  const auto Instances=Description->GetTriangleVertexInstances(Triangle);
  for(const auto Instance:Instances) {
   const FVector3f P=Positions[Description->GetVertexInstanceVertex(Instance)];
   const FVector3f N=Normals[Instance]; const FVector2f UV=UVs.Get(Instance,0);
   Obj.Add(FString::Printf(TEXT("v %.6f %.6f %.6f"),P.X,P.Y,P.Z));
   Obj.Add(FString::Printf(TEXT("vt %.6f %.6f"),UV.X,UV.Y));
   Obj.Add(FString::Printf(TEXT("vn %.6f %.6f %.6f"),N.X,N.Y,N.Z));
  }
  Obj.Add(FString::Printf(TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d"),VertexNumber,VertexNumber,VertexNumber,
   VertexNumber+1,VertexNumber+1,VertexNumber+1,VertexNumber+2,VertexNumber+2,VertexNumber+2));
  VertexNumber+=3;
 }
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("WeaponBundleInspection");
 IFileManager::Get().MakeDirectory(*Folder,true);
 const FString Path=Folder/(Name+TEXT(".obj"));
 Lines.Add(FString::Printf(TEXT("  GeometrySnapshot=%s saved=%d"),*Path,FFileHelper::SaveStringArrayToFile(Obj,*Path)));
}

void InspectStatic(UStaticMesh* Mesh,TArray<FString>& Lines)
{
 AppendBounds(Lines,Mesh->GetBounds());
 Lines.Add(FString::Printf(TEXT("  LODs=%d LOD0Triangles=%d"),Mesh->GetNumLODs(),Mesh->GetNumTriangles(0)));
 for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I) {
  const auto& Material=Mesh->GetStaticMaterials()[I];
  Lines.Add(FString::Printf(TEXT("  Material[%d] slot=%s asset=%s"),I,*Material.MaterialSlotName.ToString(),*GetPathNameSafe(Material.MaterialInterface)));
 }
 for(const auto& Socket:Mesh->Sockets) if(Socket)
  Lines.Add(FString::Printf(TEXT("  Socket %s position=(%s) rotation=(%s) scale=(%s)"),*Socket->SocketName.ToString(),
   *Socket->RelativeLocation.ToString(),*Socket->RelativeRotation.ToString(),*Socket->RelativeScale.ToString()));
 ExportReferenceGeometry(Mesh,Lines);
}

void InspectSkeletal(USkeletalMesh* Mesh,TArray<FString>& Lines)
{
 AppendBounds(Lines,Mesh->GetBounds());
 for(int32 I=0;I<Mesh->GetMaterials().Num();++I) {
  const auto& Material=Mesh->GetMaterials()[I];
  Lines.Add(FString::Printf(TEXT("  Material[%d] slot=%s asset=%s"),I,*Material.MaterialSlotName.ToString(),*GetPathNameSafe(Material.MaterialInterface)));
 }
 const auto& Skeleton=Mesh->GetRefSkeleton(); const auto& Local=Skeleton.GetRefBonePose();
 TArray<FTransform> ComponentPose; ComponentPose.SetNum(Local.Num());
 for(int32 I=0;I<Local.Num();++I) {
  const int32 Parent=Skeleton.GetParentIndex(I);
  ComponentPose[I]=Parent==INDEX_NONE?Local[I]:Local[I]*ComponentPose[Parent];
  Lines.Add(FString::Printf(TEXT("  Bone[%d] %s parent=%d component=(%s)"),I,*Skeleton.GetBoneName(I).ToString(),Parent,*ComponentPose[I].ToString()));
 }
 for(const auto* Socket:Mesh->GetActiveSocketList()) if(Socket) {
  const int32 BoneIndex=Skeleton.FindBoneIndex(Socket->BoneName);
  const FTransform Component=BoneIndex==INDEX_NONE?Socket->GetSocketLocalTransform():Socket->GetSocketLocalTransform()*ComponentPose[BoneIndex];
  Lines.Add(FString::Printf(TEXT("  Socket %s bone=%s local=(%s) component=(%s)"),*Socket->SocketName.ToString(),
   *Socket->BoneName.ToString(),*Socket->GetSocketLocalTransform().ToString(),*Component.ToString()));
 }
 const FSkeletalMeshModel* Imported=Mesh->GetImportedModel();
 if(!Imported||Imported->LODModels.IsEmpty()) { Lines.Add(TEXT("  No imported LOD data")); return; }
 const auto& Lod=Imported->LODModels[0];
 Lines.Add(FString::Printf(TEXT("  ImportedLODs=%d LOD0Triangles=%d sections=%d"),Imported->LODModels.Num(),Lod.IndexBuffer.Num()/3,Lod.Sections.Num()));
 TMap<int32,FBox> InfluenceBounds; TMap<int32,int32> InfluenceCounts;
 for(const auto& Section:Lod.Sections) for(const auto& Vertex:Section.SoftVertices) {
  int32 MaxInfluence=0;
  for(int32 J=1;J<MAX_TOTAL_INFLUENCES;++J) if(Vertex.InfluenceWeights[J]>Vertex.InfluenceWeights[MaxInfluence]) MaxInfluence=J;
  const int32 LocalBone=Vertex.InfluenceBones[MaxInfluence];
  if(!Section.BoneMap.IsValidIndex(LocalBone)) continue;
  const int32 Bone=Section.BoneMap[LocalBone];
  if(!InfluenceBounds.Contains(Bone)) InfluenceBounds.Add(Bone,FBox(ForceInit));
  InfluenceBounds[Bone]+=FVector(Vertex.Position); ++InfluenceCounts.FindOrAdd(Bone);
 }
 for(int32 I=0;I<Local.Num();++I) if(const FBox* Box=InfluenceBounds.Find(I))
  Lines.Add(FString::Printf(TEXT("  SkinGroup bone=%s vertices=%d min=(%s) max=(%s)"),*Skeleton.GetBoneName(I).ToString(),InfluenceCounts[I],*Box->Min.ToString(),*Box->Max.ToString()));
}
}
#endif

UVanthexInspectWeaponBundleCommandlet::UVanthexInspectWeaponBundleCommandlet()
{ IsClient=false; IsEditor=true; IsServer=false; LogToConsole=true; }

int32 UVanthexInspectWeaponBundleCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 Registry.ScanPathsSynchronous({TEXT("/Game/FPS_Weapon_Bundle")},true);
 TArray<FAssetData> Assets; Registry.GetAssetsByPath(TEXT("/Game/FPS_Weapon_Bundle"),Assets,true);
 Assets.Sort([](const FAssetData& A,const FAssetData& B){return A.PackageName.LexicalLess(B.PackageName);});
 TArray<FString> Lines; int32 StaticCount=0,SkeletalCount=0,TextureCount=0,Failed=0;
 Lines.Add(TEXT("FPS Weapon Bundle inspection. Read-only; no packages saved. Units: centimetres. Coordinates: original asset component space."));
 for(const auto& Data:Assets) {
  Lines.Add(FString::Printf(TEXT("ASSET %s CLASS %s"),*Data.GetObjectPathString(),*Data.AssetClassPath.ToString()));
  if(Data.AssetClassPath!=UStaticMesh::StaticClass()->GetClassPathName()&&
     Data.AssetClassPath!=USkeletalMesh::StaticClass()->GetClassPathName()&&
     Data.AssetClassPath!=UTexture2D::StaticClass()->GetClassPathName()) continue;
  UObject* Asset=Data.GetAsset();
  if(!Asset) { ++Failed; Lines.Add(TEXT("  LOAD FAILED")); continue; }
  if(auto* Static=Cast<UStaticMesh>(Asset)) { ++StaticCount; VanthexBundleInspection::InspectStatic(Static,Lines); }
  else if(auto* Skeletal=Cast<USkeletalMesh>(Asset)) { ++SkeletalCount; VanthexBundleInspection::InspectSkeletal(Skeletal,Lines); }
  else if(auto* Texture=Cast<UTexture2D>(Asset)) {
   ++TextureCount; Lines.Add(FString::Printf(TEXT("  Texture %dx%d SRGB=%d Compression=%d"),Texture->GetSizeX(),Texture->GetSizeY(),Texture->SRGB,int32(Texture->CompressionSettings)));
  }
 }
 const bool Valid=!Assets.IsEmpty()&&StaticCount>0&&SkeletalCount>0&&Failed==0;
 Lines.Insert(FString::Printf(TEXT("OVERALL %s assets=%d static=%d skeletal=%d textures=%d loadFailures=%d"),Valid?TEXT("PASS"):TEXT("FAIL"),Assets.Num(),StaticCount,SkeletalCount,TextureCount,Failed),0);
 const FString Report=FPaths::ProjectSavedDir()/TEXT("WeaponBundleInspection.txt");
 const bool Saved=FFileHelper::SaveStringArrayToFile(Lines,*Report);
 UE_LOG(LogVanthex,Display,TEXT("Weapon bundle inventory: %s (%d assets) report %s"),Valid&&Saved?TEXT("PASS"):TEXT("FAIL"),Assets.Num(),*Report);
 return Valid&&Saved?0:1;
#else
 return 1;
#endif
}
