#include "VanthexNavigation.h"
#include "VanthexMapLayout.h"
#include "VanthexGridV3Layout.h"
#include "Engine/World.h"

namespace
{
constexpr float GridStep=150.f;
constexpr float CapsuleRadius=42.f;
constexpr float CapsuleHalfHeight=96.f;
// A capsule on an incline needs a little more than HalfHeight above the
// vertical floor trace. Twelve centimetres also avoids floor-contact noise.
constexpr float FloorToCenter=110.f;
constexpr float FloorTolerance=36.f;
constexpr float MaxSlopeTangent=0.65f;

FCollisionObjectQueryParams StaticObjects()
{
 FCollisionObjectQueryParams Objects;
 Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 return Objects;
}

bool FloorAt(UWorld* World,const FVector& Expected,FVector& Floor)
{
 FCollisionQueryParams Params(SCENE_QUERY_STAT(GridV3Floor),false);
 FHitResult Hit;
 if(!World->LineTraceSingleByObjectType(Hit,Expected+FVector(0,0,FloorTolerance),Expected-FVector(0,0,FloorTolerance),StaticObjects(),Params)
   || Hit.bStartPenetrating || Hit.ImpactNormal.Z<0.84f) return false;
 Floor=Hit.ImpactPoint;
 return !World->OverlapAnyTestByObjectType(Floor+FVector(0,0,FloorToCenter),FQuat::Identity,StaticObjects(),FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalfHeight),Params);
}

bool WalkSegment(UWorld* World,const FVector& FloorStart,const FVector& FloorEnd,FString* Failure)
{
 auto Fail=[&](const TCHAR* Why,const FVector& Position) {
  if(Failure) *Failure=FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f)"),Why,Position.X,Position.Y,Position.Z);
  return false;
 };
 if(!World) return Fail(TEXT("missing world"),FloorStart);
 const float Horizontal=FVector::Dist2D(FloorStart,FloorEnd);
 if(FMath::Abs(FloorEnd.Z-FloorStart.Z)>Horizontal*MaxSlopeTangent+2.f) return Fail(TEXT("slope exceeds walking limit"),FloorEnd);
 const int32 Samples=FMath::Max(1,FMath::CeilToInt(Horizontal/50.f));
 FCollisionQueryParams Params(SCENE_QUERY_STAT(GridV3Walk),false);
 FVector Previous=FVector::ZeroVector;
 for(int32 I=0;I<=Samples;++I) {
  const FVector Expected=FMath::Lerp(FloorStart,FloorEnd,float(I)/Samples);
  FVector Floor;
  if(!FloorAt(World,Expected,Floor)) return Fail(TEXT("missing walkable floor or capsule clearance"),Expected);
  const FVector Center=Floor+FVector(0,0,FloorToCenter);
  if(I>0) {
   // Continuous ramps are required: a trace cannot bridge a hole or silently
   // turn a staircase landing into a jump/drop edge.
   if(FMath::Abs(Center.Z-Previous.Z)>FVector::Dist2D(Center,Previous)*MaxSlopeTangent+4.f) return Fail(TEXT("discontinuous floor"),Floor);
   FHitResult Hit;
   if(World->SweepSingleByObjectType(Hit,Previous,Center,FQuat::Identity,StaticObjects(),FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalfHeight),Params)) return Fail(TEXT("standing capsule blocked"),Hit.ImpactPoint);
  }
  Previous=Center;
 }
 return true;
}

struct FNavNode
{
 FVector Point;
 TArray<int32> Neighbours;
};

struct FGridCache
{
 TWeakObjectPtr<UWorld> World;
 TArray<FNavNode> Nodes;
 TArray<TArray<int32>> Cells;
 FVector2D Origin=FVector2D::ZeroVector;
 int32 Width=0;
 int32 Height=0;
 int32 ComponentCount=0;
};

// Immutable world-static collision is shared by every bot. Dynamic characters,
// the Core and optional doors never poison the cached route graph.
FGridCache GridCache;

void CandidateFloors(const FVector2D& XY,TArray<float>& Heights)
{
 Heights.Reset();
 auto Add=[&](float Height) {
  for(float Existing:Heights) if(FMath::Abs(Existing-Height)<5.f) return;
  Heights.Add(Height);
 };
 for(const auto& Room:VanthexMap::Rooms(GridCache.World.Get())) {
  if(FMath::Abs(XY.X-Room.Center.X)<=Room.Size.X*0.5f && FMath::Abs(XY.Y-Room.Center.Y)<=Room.Size.Y*0.5f) Add(Room.Center.Z);
 }
 auto Segment=[&](const FVector& A,const FVector& B,float Width) {
  const FVector2D Start(A.X,A.Y),Delta(B.X-A.X,B.Y-A.Y);
  const float T=FMath::Clamp(FVector2D::DotProduct(XY-Start,Delta)/FMath::Max(Delta.SizeSquared(),1.f),0.f,1.f);
  if(FVector2D::DistSquared(XY,Start+Delta*T)<=FMath::Square(Width*0.5f)) Add(FMath::Lerp(A.Z,B.Z,T));
 };
 for(const auto& Connection:VanthexMap::Connections(GridCache.World.Get())) for(int32 I=1;I<Connection.Points.Num();++I) Segment(Connection.Points[I-1],Connection.Points[I],Connection.Width);
 for(const auto& Link:VanthexMap::VerticalLinks(GridCache.World.Get())) Segment(Link.Bottom,Link.Top,Link.Width);
}

void BuildGrid(UWorld* World)
{
 if(GridCache.World.Get()==World && !GridCache.Cells.IsEmpty()) return;
 GridCache=FGridCache();
 GridCache.World=World;
 FBox2D Bounds(ForceInit);
 for(const auto& Room:VanthexMap::Rooms(GridCache.World.Get())) {
  const FVector2D Center(Room.Center.X,Room.Center.Y),Half(Room.Size.X*0.5f,Room.Size.Y*0.5f);
  Bounds+=Center-Half; Bounds+=Center+Half;
 }
 for(const auto& Connection:VanthexMap::Connections(GridCache.World.Get())) for(const FVector& P:Connection.Points) {
  Bounds+=FVector2D(P.X-Connection.Width,P.Y-Connection.Width);
  Bounds+=FVector2D(P.X+Connection.Width,P.Y+Connection.Width);
 }
 for(const auto& Link:VanthexMap::VerticalLinks(GridCache.World.Get())) for(const FVector P:{Link.Bottom,Link.Top}) {
  Bounds+=FVector2D(P.X-Link.Width,P.Y-Link.Width);
  Bounds+=FVector2D(P.X+Link.Width,P.Y+Link.Width);
 }
 GridCache.Origin=FVector2D(FMath::FloorToFloat(Bounds.Min.X/GridStep)*GridStep,FMath::FloorToFloat(Bounds.Min.Y/GridStep)*GridStep);
 GridCache.Width=FMath::CeilToInt((Bounds.Max.X-GridCache.Origin.X)/GridStep)+1;
 GridCache.Height=FMath::CeilToInt((Bounds.Max.Y-GridCache.Origin.Y)/GridStep)+1;
 GridCache.Cells.SetNum(GridCache.Width*GridCache.Height);
 TArray<float> Heights;
 for(int32 Y=0;Y<GridCache.Height;++Y) for(int32 X=0;X<GridCache.Width;++X) {
  const FVector2D XY=GridCache.Origin+FVector2D(X*GridStep,Y*GridStep);
  CandidateFloors(XY,Heights);
  for(float Z:Heights) {
   FVector Floor;
   if(!FloorAt(World,FVector(XY.X,XY.Y,Z),Floor)) continue;
   bool Duplicate=false;
   for(int32 Existing:GridCache.Cells[X+Y*GridCache.Width]) if(FMath::Abs(GridCache.Nodes[Existing].Point.Z-(Floor.Z+FloorToCenter))<1.f) {Duplicate=true;break;}
   if(Duplicate) continue;
   const int32 NodeIndex=GridCache.Nodes.AddDefaulted();
   GridCache.Nodes[NodeIndex].Point=Floor+FVector(0,0,FloorToCenter);
   GridCache.Cells[X+Y*GridCache.Width].Add(NodeIndex);
  }
 }
 // Connect each pair once. Four offset directions produce eight-direction
 // movement without corner cutting: the full capsule is swept on every edge.
 const FIntPoint Offsets[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(1,1),FIntPoint(-1,1)};
 for(int32 Y=0;Y<GridCache.Height;++Y) for(int32 X=0;X<GridCache.Width;++X) for(const FIntPoint& Offset:Offsets) {
  const int32 NextX=X+Offset.X,NextY=Y+Offset.Y;
  if(NextX<0 || NextX>=GridCache.Width || NextY>=GridCache.Height) continue;
  for(int32 A:GridCache.Cells[X+Y*GridCache.Width]) for(int32 B:GridCache.Cells[NextX+NextY*GridCache.Width]) {
   const FVector Start=GridCache.Nodes[A].Point-FVector(0,0,FloorToCenter),End=GridCache.Nodes[B].Point-FVector(0,0,FloorToCenter);
   if(!WalkSegment(World,Start,End,nullptr)) continue;
   GridCache.Nodes[A].Neighbours.Add(B);
   GridCache.Nodes[B].Neighbours.Add(A);
  }
 }
 TArray<bool> Visited; Visited.Init(false,GridCache.Nodes.Num());
 for(int32 I=0;I<GridCache.Nodes.Num();++I) if(!Visited[I]) {
  ++GridCache.ComponentCount;
  TArray<int32> Queue; Queue.Add(I); Visited[I]=true;
  for(int32 Head=0;Head<Queue.Num();++Head) for(int32 Next:GridCache.Nodes[Queue[Head]].Neighbours) if(!Visited[Next]) {Visited[Next]=true;Queue.Add(Next);}
 }
}

bool DynamicWalkSegment(UWorld* World,const FVector& Start,const FVector& End)
{
 FCollisionObjectQueryParams Objects;
 Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(EconomyDynamicRoute),false);
 FHitResult Hit;
 return !World->SweepSingleByObjectType(Hit,Start,End,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalfHeight),Params);
}

int32 Anchor(UWorld* World,const FVector& Position,FVector& Projected,bool CheckDynamic=false)
{
 // Objectives are often near the floor, characters at capsule height. Resolve
 // the nearby intended floor first, then verify the short connection to the
 // graph. Nearest XY alone would connect different storeys through a ceiling.
 TArray<float> Heights;
 CandidateFloors(FVector2D(Position.X,Position.Y),Heights);
 float BestHeightScore=TNumericLimits<float>::Max();
 bool FoundFloor=false;
 FVector Floor=FVector::ZeroVector;
 for(float Z:Heights) {
  FVector Candidate;
  if(!FloorAt(World,FVector(Position.X,Position.Y,Z),Candidate)) continue;
  const float Score=FMath::Abs(Position.Z-(Candidate.Z+FloorToCenter));
  if(Score<BestHeightScore && Score<240.f) {BestHeightScore=Score;Floor=Candidate;FoundFloor=true;}
 }
 if(!FoundFloor) return INDEX_NONE;
 Projected=Floor+FVector(0,0,FloorToCenter);
 TArray<TPair<float,int32>> Candidates;
 const int32 CellX=FMath::RoundToInt((Position.X-GridCache.Origin.X)/GridStep),CellY=FMath::RoundToInt((Position.Y-GridCache.Origin.Y)/GridStep);
 for(int32 Y=FMath::Max(0,CellY-2);Y<=FMath::Min(GridCache.Height-1,CellY+2);++Y) for(int32 X=FMath::Max(0,CellX-2);X<=FMath::Min(GridCache.Width-1,CellX+2);++X) for(int32 I:GridCache.Cells[X+Y*GridCache.Width]) {
  const FVector& P=GridCache.Nodes[I].Point;
  if(FMath::Abs(P.Z-Projected.Z)>GridStep*2.f*MaxSlopeTangent) continue;
  Candidates.Emplace(FVector::DistSquared(P,Projected),I);
 }
 Candidates.Sort([](const TPair<float,int32>& A,const TPair<float,int32>& B){return A.Key<B.Key;});
 for(const auto& Candidate:Candidates) if(WalkSegment(World,Floor,GridCache.Nodes[Candidate.Value].Point-FVector(0,0,FloorToCenter),nullptr)
  && (!CheckDynamic || DynamicWalkSegment(World,Projected,GridCache.Nodes[Candidate.Value].Point))) return Candidate.Value;
 return INDEX_NONE;
}

bool FindGridV3Route(UWorld* World,const FVector& StartPosition,const FVector& Goal,TArray<FVector>& Route,bool CheckDynamic=false)
{
 BuildGrid(World);
 FVector ProjectedStart,ProjectedGoal;
 const int32 Start=Anchor(World,StartPosition,ProjectedStart,CheckDynamic),End=Anchor(World,Goal,ProjectedGoal,CheckDynamic);
 if(Start==INDEX_NONE || End==INDEX_NONE) return false;
 struct FOpenNode {int32 Index;float Estimate;};
 const auto Less=[](const FOpenNode& A,const FOpenNode& B){return A.Estimate<B.Estimate;};
 TArray<FOpenNode> Open;
 TArray<float> Cost; Cost.Init(TNumericLimits<float>::Max(),GridCache.Nodes.Num());
 TArray<int32> Previous; Previous.Init(INDEX_NONE,GridCache.Nodes.Num());
 TArray<bool> Closed; Closed.Init(false,GridCache.Nodes.Num());
 Cost[Start]=0.f; Previous[Start]=Start;
 Open.HeapPush(FOpenNode{Start,0.f},Less);
 while(!Open.IsEmpty()) {
  FOpenNode Current; Open.HeapPop(Current,Less,EAllowShrinking::No);
  const int32 A=Current.Index;
  if(Closed[A]) continue;
  Closed[A]=true;
  if(A==End) break;
  for(int32 B:GridCache.Nodes[A].Neighbours) {
   if(CheckDynamic && !DynamicWalkSegment(World,GridCache.Nodes[A].Point,GridCache.Nodes[B].Point)) continue;
   const float NewCost=Cost[A]+FVector::Dist(GridCache.Nodes[A].Point,GridCache.Nodes[B].Point);
   if(NewCost>=Cost[B]) continue;
   Cost[B]=NewCost; Previous[B]=A;
   Open.HeapPush(FOpenNode{B,NewCost+float(FVector::Dist(GridCache.Nodes[B].Point,GridCache.Nodes[End].Point))},Less);
  }
 }
 if(Previous[End]==INDEX_NONE) return false;
 TArray<int32> Reverse;
 for(int32 I=End;;I=Previous[I]) {Reverse.Add(I);if(I==Start) break;}
 for(int32 I=Reverse.Num()-1;I>=0;--I) Route.Add(GridCache.Nodes[Reverse[I]].Point);
 // Keep the projected goal at standing height, including objectives whose
 // actor pivot lies below the capsule center.
 Route.Add(ProjectedGoal);
 return true;
}

bool FindLegacyRoute(UWorld* World,const FVector& StartPosition,const FVector& Goal,TArray<FVector>& Route)
{
 Route.Reset(); if(!World) return false;
 const FVector2D Bounds=VanthexMap::Extent()-FVector2D(200,200);
 constexpr float Step=200.f;
 const int32 Width=FMath::RoundToInt(Bounds.X*2/Step)+1,Height=FMath::RoundToInt(Bounds.Y*2/Step)+1;
 const int32 Count=Width*Height;
 auto Cell=[&](const FVector& P) {return FMath::Clamp(FMath::RoundToInt((P.X+Bounds.X)/Step),0,Width-1)+Width*FMath::Clamp(FMath::RoundToInt((P.Y+Bounds.Y)/Step),0,Height-1);};
 auto Point=[&](int32 I) {return FVector((I%Width)*Step-Bounds.X,(I/Width)*Step-Bounds.Y,100);};
 // Cache only immutable world-static edges; all five bots reuse this map.
 static TWeakObjectPtr<UWorld> CachedWorld;
 static TArray<uint8> Edges;
 const int32 Directions[]={-1,1,-Width,Width};
 if(CachedWorld.Get()!=World || Edges.Num()!=Count) {
  CachedWorld=World; Edges.Init(0,Count);
  FCollisionQueryParams Params(SCENE_QUERY_STAT(GridNavigation),false);
  FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic);
  for(int32 A=0;A<Count;++A) for(int32 D=0;D<4;++D) {
   const int32 B=A+Directions[D]; if(B<0 || B>=Count || (D<2 && A/Width!=B/Width)) continue;
   FHitResult Hit;
   if(!World->SweepSingleByObjectType(Hit,Point(A),Point(B),FQuat::Identity,Objects,FCollisionShape::MakeCapsule(48,94),Params)) Edges[A]|=1<<D;
  }
 }
 TArray<int32> Prev; Prev.Init(-1,Count); TArray<int32> Queue;
 const int32 Start=Cell(StartPosition),End=Cell(Goal); Prev[Start]=Start; Queue.Add(Start);
 for(int32 Head=0;Head<Queue.Num() && Prev[End]<0;++Head) {
  const int32 A=Queue[Head];
  for(int32 D=0;D<4;++D) if(Edges[A]&(1<<D)) {const int32 B=A+Directions[D]; if(Prev[B]<0) {Prev[B]=A; Queue.Add(B);}}
 }
 if(Prev[End]<0) return false;
 for(int32 I=End;I!=Start;I=Prev[I]) Route.Insert(Point(I),0);
 Route.Add(Goal); return true;
}
}

bool VanthexNavigation::FindRoute(UWorld* World,const FVector& Start,const FVector& Goal,TArray<FVector>& Route)
{
 Route.Reset();
 if(!World) return false;
 return VanthexMap::IsFacility(World)?FindGridV3Route(World,Start,Goal,Route):FindLegacyRoute(World,Start,Goal,Route);
}

bool VanthexNavigation::CheckWalkableSegment(UWorld* World,const FVector& FloorStart,const FVector& FloorEnd,FString* Failure)
{
 return WalkSegment(World,FloorStart,FloorEnd,Failure);
}

bool VanthexNavigation::FindRouteLength(UWorld* World,const FVector& Start,const FVector& Goal,float& Length)
{
 Length=0.f;
 TArray<FVector> Route;
 const bool Grid=VanthexMap::IsFacility(World);
 if(Grid ? !FindGridV3Route(World,Start,Goal,Route,true) : !FindRoute(World,Start,Goal,Route)) return false;
 FVector Previous=Start;
 if(Grid) { if(Anchor(World,Start,Previous,true)==INDEX_NONE) return false; }
 else Previous.Z=110.f;
 for(const FVector& Point:Route) { if(!DynamicWalkSegment(World,Previous,Point)) return false; Length+=FVector::Dist(Previous,Point); Previous=Point; }
 return true;
}

void VanthexNavigation::GridV3GraphStats(UWorld* World,int32& NodeCount,int32& EdgeCount,int32& ComponentCount)
{
 NodeCount=EdgeCount=ComponentCount=0;
 if(!World || !VanthexMap::IsFacility(World)) return;
 BuildGrid(World);
 NodeCount=GridCache.Nodes.Num();
 for(const FNavNode& Node:GridCache.Nodes) EdgeCount+=Node.Neighbours.Num();
 EdgeCount/=2;
 ComponentCount=GridCache.ComponentCount;
}
