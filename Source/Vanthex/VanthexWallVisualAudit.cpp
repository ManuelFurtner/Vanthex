#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Query the actual world without giving decorative meshes temporary collision.
// Solid wall modules must agree with the separate blocking proxies at walk height.
bool VanthexAuditVisibleWalls(UWorld* World,TArray<FString>& Lines)
{
 int32 Samples=0,Missing=0;TArray<FString> Detail;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(VisibleWallAgreement),false);
 for(TActorIterator<AActor> It(World);It;++It) {
  TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
  for(auto* Part:Parts) {
   if(!Part->IsVisible()||!Part->GetStaticMesh()||Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision||!Part->GetName().Contains(TEXT("Wall")))continue;
   auto* ISM=Cast<UInstancedStaticMeshComponent>(Part);const int32 Count=ISM?ISM->GetInstanceCount():1;
   const auto Box=Part->GetStaticMesh()->GetBoundingBox();
   for(int32 Index=0;Index<Count;++Index) {
    FTransform Transform=Part->GetComponentTransform();if(ISM)ISM->GetInstanceTransform(Index,Transform,true);
    const FVector Size=Box.GetSize()*Transform.GetScale3D().GetAbs();
    if(Size.X<90||Size.Z<120||Size.Y>70)continue;
    const FVector Normal=Transform.GetRotation().RotateVector(FVector(0,1,0));if(FMath::Abs(Normal.Z)>.2)continue;
    for(float U:{-.35f,0.f,.35f})for(float V:{-.3f,0.f,.3f}) {
     const FVector Point=Transform.TransformPosition(Box.GetCenter()+FVector(U*Box.GetSize().X,0,V*Box.GetSize().Z));
     FHitResult Wall;if(World->LineTraceSingleByObjectType(Wall,Point-Normal*55,Point+Normal*55,Objects,Query))continue;
     bool Reachable=false;
     for(float Side:{-1.f,1.f}) {
      const FVector Near=Point+Normal*(Side*65);FHitResult Floor;
      if(World->LineTraceSingleByObjectType(Floor,Near,Near-FVector(0,0,230),Objects,Query)&&Floor.GetComponent()&&(Floor.GetComponent()->ComponentHasTag(TEXT("GridV4Floor"))||Floor.GetComponent()->ComponentHasTag(TEXT("GridV4Ramp")))&&Point.Z-Floor.ImpactPoint.Z>=25&&Point.Z-Floor.ImpactPoint.Z<=190)Reachable=true;
     }
     if(!Reachable)continue;
     ++Missing;Detail.Add(FString::Printf(TEXT("GHOST_WALL %s instance=%d point=%s size=%s"),*Part->GetName(),Index,*Point.ToString(),*Size.ToString()));
    }
    ++Samples;
   }
  }
 }
 Lines.Add(FString::Printf(TEXT("VISUAL_WALL_AUDIT %d wall instances; %d unsupported reachable samples"),Samples,Missing));
 Lines.Append(Detail);return Missing==0;
}

bool VanthexAuditStairPassages(UWorld* World,TArray<FString>& Lines)
{
 int32 Treads=0,Obstructed=0;TArray<FString> Details;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(StairVisualClearance),false);
 for(TActorIterator<AActor> It(World);It;++It) {
  TArray<UInstancedStaticMeshComponent*> Parts;It->GetComponents(Parts);
  for(auto* Part:Parts)if(Part->IsVisible()&&Part->GetName().Contains(TEXT("GreyboxTread"))&&Part->GetStaticMesh()) {
   const auto Bounds=Part->GetStaticMesh()->GetBoundingBox();
   for(int32 Index=0;Index<Part->GetInstanceCount();++Index) {
    ++Treads;FTransform T;Part->GetInstanceTransform(Index,T,true);
    const auto Box=Bounds.TransformBy(T);if(Box.GetSize().Z<220)continue;
    const auto C=Box.GetCenter();const FVector Probe(C.X,C.Y,Box.Min.Z+195);FHitResult Floor;
    if(!World->LineTraceSingleByObjectType(Floor,Probe,Probe-FVector(0,0,230),Objects,Query)||!Floor.GetComponent()||!Floor.GetComponent()->ComponentHasTag(TEXT("GridV4Floor")))continue;
    const FVector Stand=Floor.ImpactPoint+FVector(0,0,98);
    if(Box.Max.Z<Stand.Z+96||!Box.IsInside(Stand))continue;
    if(World->OverlapAnyTestByObjectType(Stand,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(42,96),Query))continue;
    ++Obstructed;Details.Add(FString::Printf(TEXT("GHOST_STAIR_BASE tread=%d point=%s visualHeight=%.1f free standing capsule inside visible solid"),Index,*Stand.ToString(),Box.GetSize().Z));
   }
  }
 }
 Lines.Add(FString::Printf(TEXT("STAIR_VISUAL_CLEARANCE %d treads; %d false solid bases in physically walkable space"),Treads,Obstructed));Lines.Append(Details);
 return Treads>0&&Obstructed==0;
}
