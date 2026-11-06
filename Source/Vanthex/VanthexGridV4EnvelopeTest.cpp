#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "VanthexGridV4Layout.h"

// This probes the built world, not just its intended corridor graph. A legal
// route through every room does not prove that its outer shell is sealed.
bool VanthexValidateGridV4Envelope(UWorld* World,TArray<FString>& Lines)
{
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(GridV4Envelope),false);
 int32 Supported=0,Probes=0,Leaks=0;
 auto Playable=[](const FHitResult& Hit) {
  const auto* C=Hit.GetComponent();
  return C && Hit.ImpactNormal.Z>.65f && (C->ComponentHasTag(TEXT("GridV4Floor"))||C->ComponentHasTag(TEXT("GridV4Ramp"))||C->ComponentHasTag(TEXT("GridV4Cover")));
 };
 for(float Floor:{-600.f,0.f,444.f,600.f}) for(int32 Y=-4800;Y<=5600;Y+=100) for(int32 X=-6100;X<=6500;X+=100) {
  const FVector P(X+50,Y+50,Floor);
  FHitResult Ground;
  if(!World->LineTraceSingleByObjectType(Ground,P+FVector(0,0,25),P-FVector(0,0,25),Objects,Query)||!Playable(Ground))continue;
  ++Supported;
  for(float HalfHeight:{60.f,96.f}) for(const FVector Direction:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)}) for(float Jump:{0.f,150.f}) {
   const FVector A=P+FVector(0,0,HalfHeight+14+Jump),B=A+Direction*200;
   if(World->OverlapAnyTestByObjectType(A,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(42,HalfHeight),Query))continue;
   ++Probes;
   if(World->SweepTestByObjectType(A,B,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(42,HalfHeight),Query))continue;
   FHitResult Landing;
   const FVector End=P+Direction*200;
   if(World->LineTraceSingleByObjectType(Landing,End+FVector(0,0,30+Jump),FVector(End.X,End.Y,-800),Objects,Query)&&Playable(Landing))continue;
   ++Leaks;
   if(Leaks<=30)Lines.Add(FString::Printf(TEXT("LEAK from %s toward %s jump=%.0f landing=%s"),*P.ToString(),*Direction.ToString(),Jump,*Landing.ImpactPoint.ToString()));
  }
 }
 Lines.Add(FString::Printf(TEXT("ENVELOPE %d supported floor samples; %d standing/crouching/jump capsule sweeps; %d unguarded exits to void/non-playable roofs"),Supported,Probes,Leaks));
 return Supported>1000 && Probes>4000 && Leaks==0;
}

bool VanthexValidateGridV4Furniture(UWorld* World,TArray<FString>& Lines)
{
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(GridV4Furniture),false);
 // Exclude matching furniture proxies: the test must find the building floor,
 // not mistake the object's own collision for its support.
 for(TActorIterator<AActor> It(World);It;++It){
  TArray<UPrimitiveComponent*> Parts;It->GetComponents(Parts);
  for(auto* C:Parts)if(C->ComponentHasTag(TEXT("GridV4Cover")))Query.AddIgnoredActor(*It);
 }
 const TSet<FString> Grounded={TEXT("SM_ElectricalPanel_03"),TEXT("SM_AirConditioner01"),TEXT("SM_AssemblyLineDesk01"),TEXT("SM_Boiler_01"),TEXT("SM_StorageShelves_group01"),TEXT("SM_Boxes_group_06"),TEXT("SM_ForkLift"),TEXT("SM_Container01_01"),TEXT("SM_AssemblyLineControl01"),TEXT("SM_DeskControl_01"),TEXT("SM_ContainerP4_01")};
 int32 Count=0,Unsupported=0;
 for(TActorIterator<AActor> It(World);It;++It){
  TArray<UInstancedStaticMeshComponent*> Groups;It->GetComponents(Groups);
  for(auto* G:Groups){
   UStaticMesh* Mesh=G->GetStaticMesh();if(!Mesh||(!Grounded.Contains(Mesh->GetName())&&!G->GetName().Contains(TEXT("GreyboxCover"))))continue;
   const FBox Box=Mesh->GetBoundingBox();
   for(int32 I=0;I<G->GetInstanceCount();++I){
    ++Count;FTransform T;G->GetInstanceTransform(I,T,true);bool Good=true;
    for(float X:{0.f,.5f,1.f})for(float Y:{0.f,.5f,1.f}){
     const FVector P=T.TransformPosition(FVector(FMath::Lerp(Box.Min.X,Box.Max.X,X),FMath::Lerp(Box.Min.Y,Box.Max.Y,Y),Box.Min.Z));
     FHitResult Hit;
     Good &= World->LineTraceSingleByObjectType(Hit,P+FVector(0,0,5),P-FVector(0,0,20),Objects,Query)&&Hit.ImpactNormal.Z>.9f&&Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("GridV4Floor"));
    }
    if(!Good){++Unsupported;Lines.Add(FString::Printf(TEXT("UNSUPPORTED %s instance %d at %s"),*Mesh->GetName(),I,*T.GetLocation().ToString()));}
   }
  }
 }
 Lines.Add(FString::Printf(TEXT("FURNITURE %d grounded cover instances checked at nine footprint points; %d unsupported"),Count,Unsupported));
 return Count>=20&&Unsupported==0;
}

bool VanthexValidateGreyboxStairs(UWorld* World,TArray<FString>& Lines)
{
 TArray<FBox> Treads;
 for(TActorIterator<AActor> It(World);It;++It) {
  TArray<UInstancedStaticMeshComponent*> Groups;It->GetComponents(Groups);
  for(auto* G:Groups)if(G->GetName().Contains(TEXT("GreyboxTread"))&&G->GetStaticMesh())
   for(int32 I=0;I<G->GetInstanceCount();++I){FTransform T;G->GetInstanceTransform(I,T,true);Treads.Add(G->GetStaticMesh()->GetBoundingBox().TransformBy(T));}
 }
 bool Good=Treads.Num()==GridV4::VerticalLinks().Num()*48;
 int32 Samples=0,Failures=0;
 for(const auto& V:GridV4::VerticalLinks())for(int32 I=0;I<48;++I)for(float Side:{-.45f,0.f,.45f}) {
  const FVector P=FMath::Lerp(V.Bottom,V.Top,(I+.5f)/48.f)+FVector(Side*V.Width,0,0);
  bool Found=false;
  for(const auto& Box:Treads)if(P.X>=Box.Min.X-.1&&P.X<=Box.Max.X+.1&&P.Y>=Box.Min.Y-.1&&P.Y<=Box.Max.Y+.1&&FMath::Abs(Box.Max.Z-(V.Bottom.Z+(I+1)*(V.Top.Z-V.Bottom.Z)/48.f))<.1) {Found=true;break;}
  ++Samples;if(!Found)++Failures;
 }
 Lines.Add(FString::Printf(TEXT("GREYBOX_STAIRS %d visible cube treads; %d centre/side samples; %d missing or incorrect heights"),Treads.Num(),Samples,Failures));
 return Good&&Failures==0;
}
