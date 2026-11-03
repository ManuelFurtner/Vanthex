#include "VanthexWeaponAudioAcoustics.h"
#include "VanthexGridV3Layout.h"
#include "VanthexMapLayout.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace
{
struct FEdge { int32 A, B; float Length; bool bClear=true; };
struct FGraph
{
 TWeakObjectPtr<UWorld> World;
 TArray<FVector> Nodes;
 TArray<FEdge> Edges;
 float LastTrace=-100.f;
 int32 Node(FVector P) {
  for(int32 I=0; I<Nodes.Num(); ++I) if(Nodes[I].Equals(P,1.)) return I;
  return Nodes.Add(P);
 }
 void Segment(FVector A, FVector B) {
  A.Z+=145.; B.Z+=145.;
  const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Distance(A,B)/350.));
  int32 Previous=Node(A);
  for(int32 I=1; I<=Steps; ++I) {
   const int32 Next=Node(FMath::Lerp(A,B,double(I)/Steps));
   if(Previous!=Next) Edges.Add({Previous,Next,float(FVector::Distance(Nodes[Previous],Nodes[Next])),true});
   Previous=Next;
  }
 }
};
// One graph per active world, discarded on world replacement. No ticking component or audio-thread traces.
FGraph Graph;

FCollisionQueryParams Query(UWorld* World, AActor* Source)
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(WeaponAcoustics),false);
 for(TActorIterator<APawn> It(World); It; ++It) Q.AddIgnoredActor(*It);
 for(AActor* A=Source; A; A=A->GetOwner()) Q.AddIgnoredActor(A);
 return Q;
}
bool Clear(UWorld* World, FVector A, FVector B, const FCollisionQueryParams& Q)
{
 return !World->LineTraceTestByChannel(A,B,ECC_Visibility,Q);
}
int32 Room(FVector Position)
{
 int32 Best=-1; double Height=DBL_MAX;
 for(const auto& R:GridV3::Rooms()) {
  const FVector D=Position-R.Center;
  if(FMath::Abs(D.X)<=R.Size.X*.5 && FMath::Abs(D.Y)<=R.Size.Y*.5 && D.Z>=-30. && D.Z<R.Size.Z
   && FMath::Abs(D.Z-145.)<Height) { Best=R.Id; Height=FMath::Abs(D.Z-145.); }
 }
 return Best;
}
}

void VanthexWeaponAcoustics::Invalidate(UWorld* World)
{
 if(Graph.World.Get()==World) Graph.LastTrace=-100.f;
}

FVanthexWeaponAcousticRoute VanthexWeaponAcoustics::Route(UWorld* World,FVector Source,FVector Listener,AActor* SourceActor)
{
 FVanthexWeaponAcousticRoute R;
 R.ApparentPosition=Source; R.Distance=float(FVector::Distance(Source,Listener));
 if(!World) return R;
 const auto Q=Query(World,SourceActor);
 if(VanthexMap::IsGridV3(World)) {
  R.SourceRoom=Room(Source); R.ListenerRoom=Room(Listener);
  if(R.SourceRoom>=0) {
   const auto& Size=GridV3::Rooms()[R.SourceRoom].Size;
   R.ReflectionDelay=FMath::Clamp(float((Size.X+Size.Y)/137200.),.012f,.045f);
  }
 }
 R.bDirect=Clear(World,Source,Listener,Q);
 if(R.bDirect) return R;
 // Transmission is a quiet, low-passed fallback when no physically open path exists.
 R.Transmission=.075f; R.LowPassHz=850.f;
 if(!VanthexMap::IsGridV3(World)) return R;
 if(Graph.World.Get()!=World) {
  Graph=FGraph(); Graph.World=World;
  for(const auto& C:GridV3::Connections()) for(int32 I=1; I<C.Points.Num(); ++I) Graph.Segment(C.Points[I-1],C.Points[I]);
  for(const auto& V:GridV3::VerticalLinks()) Graph.Segment(V.Bottom,V.Top);
 }
 const float Now=World->GetTimeSeconds();
 if(Now-Graph.LastTrace>.10f || Now<Graph.LastTrace) {
  for(auto& E:Graph.Edges) E.bClear=Clear(World,Graph.Nodes[E.A],Graph.Nodes[E.B],Q);
  Graph.LastTrace=Now;
 }
 const int32 N=Graph.Nodes.Num();
 TArray<float> Dist; Dist.Init(FLT_MAX,N);
 TArray<int32> Previous; Previous.Init(-1,N);
 TArray<bool> Visited; Visited.Init(false,N);
 TArray<float> Exit; Exit.Init(FLT_MAX,N);
 // The nearest visible centerline points connect actual muzzle and listener positions to the topology.
 auto Nearest=[&](FVector P) {
  TArray<int32> Order; for(int32 I=0; I<N; ++I) if(FVector::DistSquared(P,Graph.Nodes[I])<FMath::Square(1600.)) Order.Add(I);
  Order.Sort([&](int32 A,int32 B){return FVector::DistSquared(P,Graph.Nodes[A])<FVector::DistSquared(P,Graph.Nodes[B]);});
  if(Order.Num()>12) Order.SetNum(12);
  return Order;
 };
 for(int32 I:Nearest(Source)) if(Clear(World,Source,Graph.Nodes[I],Q)) Dist[I]=float(FVector::Distance(Source,Graph.Nodes[I]));
 for(int32 I:Nearest(Listener)) if(Clear(World,Listener,Graph.Nodes[I],Q)) Exit[I]=float(FVector::Distance(Listener,Graph.Nodes[I]));
 int32 End=-1; float Best=FLT_MAX;
 for(int32 Step=0; Step<N; ++Step) {
  int32 U=-1; float Min=FLT_MAX;
  for(int32 I=0; I<N; ++I) if(!Visited[I] && Dist[I]<Min) { U=I; Min=Dist[I]; }
  if(U<0 || Min>=Best) break;
  Visited[U]=true;
  if(Exit[U]<FLT_MAX && Min+Exit[U]<Best) { Best=Min+Exit[U]; End=U; }
  for(const auto& E:Graph.Edges) if(E.bClear && (E.A==U || E.B==U)) {
   const int32 V=E.A==U?E.B:E.A;
   if(Min+E.Length<Dist[V]) {Dist[V]=Min+E.Length; Previous[V]=U;}
  }
 }
 if(End>=0) {
  R.bPortal=true; R.Distance=FMath::Max(R.Distance,Best); R.ApparentPosition=Graph.Nodes[End];
  R.Transmission=.58f; R.LowPassHz=5200.f;
  for(int32 I=End; I>=0 && R.Hops<=N; I=Previous[I]) ++R.Hops;
 }
 return R;
}
