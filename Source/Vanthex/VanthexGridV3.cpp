#include "VanthexTestArena.h"
#include "VanthexGridV3Layout.h"
#include "Vanthex.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/TextRenderActor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

// This builder deliberately has no dependency on the old arena footprints.
void AVanthexTestArena::BuildGridV3()
{
 UWorld* World=GetWorld();
 if(!World) return;
 CubeMesh=VanthexArt::LoadCube();
 BuildAtmosphere();
 // The shared atmosphere also creates demonstration spotlights. Replace those
 // with actual room fixtures; retain its sky/fog/postprocess configuration.
 for(TActorIterator<ASpotLight> It(World);It;++It) It->Destroy();
 for(TActorIterator<ADirectionalLight> It(World);It;++It) It->GetLightComponent()->SetIntensity(.12f);
 for(TActorIterator<AExponentialHeightFog> It(World);It;++It) {
  It->GetComponent()->SetFogDensity(.001f); It->GetComponent()->SetFogMaxOpacity(.08f);
 }

 TMap<FString,UInstancedStaticMeshComponent*> Groups;
 int32 InstanceCount=0,CollisionCount=0,LightCount=0,MissingCount=0;
 auto Group=[&](const FString& Name)->UInstancedStaticMeshComponent* {
  if(auto** Found=Groups.Find(Name)) return *Found;
  const FString Sub=Name==TEXT("SM_RoboArm04")?TEXT("Robot/"):TEXT("");
  UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Meshes/")+Sub+Name+TEXT(".")+Name));
  if(!Mesh) { ++MissingCount; UE_LOG(LogVanthex,Error,TEXT("GRIDV3 missing kit mesh %s"),*Name); return nullptr; }
  auto* G=NewObject<UInstancedStaticMeshComponent>(this,FName(*(TEXT("GridV3_")+Name)));
  G->SetStaticMesh(Mesh); G->SetupAttachment(SceneRoot); G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  G->SetCastShadow(true); G->ComponentTags.Add(TEXT("GridV3Kit")); G->RegisterComponent(); Groups.Add(Name,G);
  return G;
 };
 auto Fit=[&](const FString& Name,const FVector& Center,const FVector& Size,const FRotator& Rotation=FRotator::ZeroRotator) {
  auto* G=Group(Name); if(!G) return;
  const FBox Box=G->GetStaticMesh()->GetBoundingBox(); const FVector Native=Box.GetSize();
  const FVector Scale(Size.X/FMath::Max(1.,Native.X),Size.Y/FMath::Max(1.,Native.Y),Size.Z/FMath::Max(1.,Native.Z));
  G->AddInstance(FTransform(Rotation,Center-Rotation.RotateVector(Box.GetCenter()*Scale),Scale),true); ++InstanceCount;
 };
 auto Collision=[&](const FVector& Center,const FVector& Size,const FRotator& Rotation=FRotator::ZeroRotator,const TCHAR* Tag=TEXT("GridV3Structure")) {
  auto* C=SpawnBlock(Center,Size/100.f,FLinearColor::Gray,.8f,.1f,EVanthexSurface::Concrete);
  if(C) { C->SetWorldRotation(Rotation); C->SetVisibility(false); C->SetCastShadow(false); C->ComponentTags.Add(Tag); ++CollisionCount; }
 };
 auto Prop=[&](const FString& Name,const FVector& Ground,float Scale=1.f,float Yaw=0.f,bool Solid=false) {
  auto* G=Group(Name); if(!G) return;
  const FBox Box=G->GetStaticMesh()->GetBoundingBox(); const FRotator R(0,Yaw,0);
  const FVector Offset(Box.GetCenter().X,Box.GetCenter().Y,Box.Min.Z);
  G->AddInstance(FTransform(R,Ground-R.RotateVector(Offset*Scale),FVector(Scale)),true); ++InstanceCount;
  if(Solid) Collision(Ground+FVector(0,0,Box.GetSize().Z*Scale*.5f),Box.GetSize()*Scale,R,TEXT("GridV3Cover"));
 };
 auto Point=[&](const FVector& P,FLinearColor Color,float Intensity,float Radius,bool Shadow=false) {
  auto* A=World->SpawnActor<APointLight>(P,FRotator::ZeroRotator); if(!A) return;
  auto* L=Cast<UPointLightComponent>(A->GetLightComponent()); L->SetMobility(EComponentMobility::Movable);
  L->SetLightColor(Color); L->SetIntensity(Intensity*.65f); L->SetAttenuationRadius(Radius); L->SetCastShadows(Shadow);
  A->Tags.Add(TEXT("GridV3Light")); ++LightCount;
 };
 auto Sign=[&](const FString& Label,const FVector& P,float Yaw,const FLinearColor& Color,float Size=35.f) {
  auto* A=World->SpawnActor<ATextRenderActor>(P,FRotator(0,Yaw,0)); if(!A) return;
  const float ActualSize=FMath::Min(Size,820.f/FMath::Max(1,Label.Len())/.58f);
  const float Width=FMath::Clamp(Label.Len()*ActualSize*.58f+55,180.f,900.f);
  const FRotator R(0,Yaw,0); const FVector Normal=R.RotateVector(FVector(1,0,0));
  // Rotate an actual dark steel kit panel toward the reader, preserving the
  // asset's own UV/material pairing (a wall-atlas override causes artifacts).
  Fit(TEXT("SM_Platform_01"),P-Normal*6,FVector(ActualSize*2.1f,Width,8),FRotator(-90,Yaw,0));
  if(Size<=35) for(float Side:{-1.f,1.f}) {
   Fit(TEXT("SM_MetalBeam01"),P+R.RotateVector(FVector(-6,Side*Width*.4f,80)),FVector(100,7,7),FRotator(90,0,0));
  }
  auto* T=A->GetTextRender(); T->SetText(FText::FromString(Label)); T->SetWorldSize(ActualSize);
  T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
  T->SetTextRenderColor(Color.ToFColor(true)); T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  A->Tags.Add(TEXT("GridV3Sign"));
 };

 constexpr float Cell=100.f;
 TSet<FIntPoint> Floors[3];
 auto AddRect=[&](int32 L,float MinX,float MinY,float MaxX,float MaxY) {
  for(int32 X=FMath::FloorToInt(MinX/Cell);X<FMath::CeilToInt(MaxX/Cell);++X)
   for(int32 Y=FMath::FloorToInt(MinY/Cell);Y<FMath::CeilToInt(MaxY/Cell);++Y) Floors[L+1].Add(FIntPoint(X,Y));
 };
 for(const auto& R:GridV3::Rooms()) AddRect(R.Level,R.Center.X-R.Size.X*.5f,R.Center.Y-R.Size.Y*.5f,R.Center.X+R.Size.X*.5f,R.Center.Y+R.Size.Y*.5f);
 for(const auto& C:GridV3::Connections()) for(int32 I=1;I<C.Points.Num();++I) {
  const FVector A=C.Points[I-1],B=C.Points[I]; const float H=C.Width*.5f;
  AddRect(FMath::RoundToInt(A.Z/600.f),FMath::Min(A.X,B.X)-H,FMath::Min(A.Y,B.Y)-H,FMath::Max(A.X,B.X)+H,FMath::Max(A.Y,B.Y)+H);
 }
 auto InRamp=[&](const FVector& P,const GridV3::FVerticalLink& V,float Extra=0.f) {
  return FMath::Abs(P.X-V.Bottom.X)<=V.Width*.5f+Extra && P.Y>=V.Bottom.Y-Extra && P.Y<V.Top.Y+Extra;
 };
 // The upper slab must end at the actual top of the incline, not cover its last treads.
 for(const auto& V:GridV3::VerticalLinks()) {
  auto& F=Floors[FMath::RoundToInt(V.Top.Z/600.f)+1];
  TArray<FIntPoint> Remove;
  for(const auto& C:F) if(InRamp(FVector((C.X+.5f)*Cell,(C.Y+.5f)*Cell,0),V)) Remove.Add(C);
  for(const auto& C:Remove) F.Remove(C);
 }
 for(int32 L=0;L<=1;++L) {
  TArray<FIntPoint> Remove;
  for(const auto& C:Floors[L+1]) if(GridV3::IsCoreAperture(FVector((C.X+.5f)*Cell,(C.Y+.5f)*Cell,0))) Remove.Add(C);
  for(const auto& C:Remove) Floors[L+1].Remove(C);
 }
 auto CoreFootprint=[](const FVector& P) { return FMath::Abs(P.X)<=900 && FMath::Abs(P.Y)<=900; };
 auto RoofHeight=[&](const FVector& P,int32 Level) { return Level==0 && CoreFootprint(P)?1320.f:420.f; };
 auto RoofAllowed=[&](const FVector& P,int32 Level) {
  if(Level==-1 && GridV3::IsCoreAperture(P)) return false;
  for(const auto& V:GridV3::VerticalLinks()) if(FMath::IsNearlyEqual(Level*600.f,float(V.Bottom.Z)) && InRamp(P,V,0)) return false;
  return true;
 };
 auto Wall=[&](const FVector& A,const FVector& B,float Bottom,float Height,bool Rail=false) {
  const FVector Mid=(A+B)*.5f+FVector(0,0,Bottom+Height*.5f);
  const bool AlongX=FMath::Abs(B.X-A.X)>FMath::Abs(B.Y-A.Y);
  const float Length=FVector::Dist2D(A,B); const FRotator Rot(0,AlongX?0.f:90.f,0);
  if(Length<1 || Height<1) return;
  if(Rail) {
   // Rail has matching bar collision, leaving the gap above it open to Core passes.
   Fit(TEXT("SM_MetalBeam01"),Mid+FVector(0,0,Height*.5f-8),FVector(Length,12,16),Rot);
   Collision(Mid+FVector(0,0,Height*.5f-8),FVector(Length,12,16),Rot,TEXT("GridV3Rail"));
   for(float D=0;D<=Length;D+=200) {
    const FVector P=A+(B-A)*(D/Length)+FVector(0,0,Bottom+Height*.5f);
    Fit(TEXT("SM_Column_02"),P,FVector(12,12,Height)); Collision(P,FVector(12,12,Height),FRotator::ZeroRotator,TEXT("GridV3Rail"));
   }
   return;
  }
  Collision(Mid,FVector(Length,28,Height),Rot);
  for(float D=0;D<Length;D+=400) for(float H=0;H<Height;H+=200) {
   const float W=FMath::Min(400.f,Length-D),Tall=FMath::Min(200.f,Height-H);
   const FVector P=A+(B-A)*((D+W*.5f)/Length)+FVector(0,0,Bottom+H+Tall*.5f);
   Fit(TEXT("SM_ConcreteWall"),P,FVector(W,30,Tall),Rot);
  }
  // Kick rail and a dark service band make scale legible without flat cube skins.
  Fit(TEXT("SM_MetalBeam01"),(A+B)*.5f+FVector(0,0,Bottom+22),FVector(Length,34,16),Rot);
  Fit(TEXT("SM_MetalBeam01"),(A+B)*.5f+FVector(0,0,Bottom+Height-24),FVector(Length,34,16),Rot);
 };

 for(int32 Level=-1;Level<=1;++Level) {
  auto& Set=Floors[Level+1]; const float Z=Level*600.f;
  // Greedily pack floor modules into at most600cm tiles. This preserves UV scale
  // and drastically reduces components relative to per-cell collision.
  TSet<FIntPoint> Done;
  for(int32 Y=-33;Y<40;++Y) for(int32 X=-51;X<51;++X) {
   const FIntPoint Start(X,Y); if(!Set.Contains(Start)||Done.Contains(Start)) continue;
   int32 W=1,H=1;
   while(W<6 && Set.Contains(FIntPoint(X+W,Y)) && !Done.Contains(FIntPoint(X+W,Y))) ++W;
   for(;H<6;++H) {
    bool Row=true; for(int32 D=0;D<W;++D) if(!Set.Contains(FIntPoint(X+D,Y+H))||Done.Contains(FIntPoint(X+D,Y+H))) { Row=false; break; }
    if(!Row) break;
   }
   for(int32 DY=0;DY<H;++DY) for(int32 DX=0;DX<W;++DX) Done.Add(FIntPoint(X+DX,Y+DY));
   const FVector P((X+W*.5f)*Cell,(Y+H*.5f)*Cell,Z-8);
   Fit(Level==1?TEXT("SM_Platform_01"):TEXT("SM_ConcreteFloor_01"),P,FVector(W*Cell,H*Cell,16));
   Collision(P,FVector(W*Cell,H*Cell,16),FRotator::ZeroRotator,TEXT("GridV3Floor"));
  }
  // Ceiling tiles are separate because the Core has an actual double-height room.
  for(const auto& C:Set) {
   const FVector P((C.X+.5f)*Cell,(C.Y+.5f)*Cell,Z);
   if(!RoofAllowed(P,Level)) continue;
   const float H=RoofHeight(P,Level);
   Fit(TEXT("SM_FactoryFloorLarge01"),P+FVector(0,0,H+8),FVector(Cell,Cell,16));
   // Ceiling collision also prevents players/throws escaping closed rooms.
  }
  // Merge exterior boundary runs. The occupancy union makes real room/corridor openings.
  const FIntPoint Dirs[4]={{0,-1},{0,1},{-1,0},{1,0}};
  for(int32 Dir=0;Dir<4;++Dir) {
   TSet<FIntPoint> Edges;
   for(const auto& C:Set) if(!Set.Contains(C+Dirs[Dir])) Edges.Add(C);
   TSet<FIntPoint> Used;
   for(int32 Y=-33;Y<40;++Y) for(int32 X=-51;X<51;++X) {
    FIntPoint C(X,Y); if(!Edges.Contains(C)||Used.Contains(C)) continue;
    const bool AlongX=Dir<2; const FIntPoint Step=AlongX?FIntPoint(1,0):FIntPoint(0,1);
    FVector Edge=(FVector((X+.5f)*Cell,(Y+.5f)*Cell,0)+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*Cell*.5f);
    const float Height=RoofHeight(Edge,Level);
    auto OpenRamp=[&](FVector P) { for(const auto& V:GridV3::VerticalLinks()) if((FMath::IsNearlyEqual(Z,float(V.Top.Z))||FMath::IsNearlyEqual(Z,float(V.Bottom.Z))) && InRamp(P,V,30)) return true; return false; };
    auto UpperCrossing=[&](const FVector& P) {
     const FVector Out=P+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*50;
     return Level==0 && RoofHeight(P,Level)>600 && Floors[2].Contains(FIntPoint(FMath::FloorToInt(Out.X/100),FMath::FloorToInt(Out.Y/100)));
    };
    const bool Skip=OpenRamp(Edge);
    const bool UpperDoor=UpperCrossing(Edge);
    const bool Rail=(Level==1 && CoreFootprint(Edge)) || (Level==0 && GridV3::IsCoreAperture(Edge+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*10));
    int32 N=1; Used.Add(C);
    while(N<120) {
     const FIntPoint Next=C+Step*N; const FVector NP=Edge+FVector(Step.X,Step.Y,0)*Cell*N;
     const bool NextRail=(Level==1&&CoreFootprint(NP))||(Level==0&&GridV3::IsCoreAperture(NP+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*10));
     if(!Edges.Contains(Next)||Used.Contains(Next)||!FMath::IsNearlyEqual(RoofHeight(NP,Level),Height)||OpenRamp(NP)!=Skip||NextRail!=Rail||UpperCrossing(NP)!=UpperDoor) break;
     Used.Add(Next); ++N;
    }
    if(Skip) continue;
    const FVector Axis(Step.X,Step.Y,0); const FVector A=Edge-Axis*Cell*.5f,B=A+Axis*Cell*N;
    if(UpperDoor) { Wall(A,B,Z,600); Wall(A,B,Z+1020,Height-1020); }
    else Wall(A,B,Z,Rail?105.f:Height,Rail);
   }
  }
  // Collision for ceilings uses strips, independently of visual100cm tiles.
  for(int32 Y=-33;Y<40;++Y) {
   int32 X=-51;
   while(X<51) {
    const FVector P((X+.5f)*Cell,(Y+.5f)*Cell,Z); const float H=RoofHeight(P,Level);
    if(!Set.Contains(FIntPoint(X,Y))||!RoofAllowed(P,Level)) { ++X; continue; }
    const int32 Start=X++;
    while(X<51) { const FVector N((X+.5f)*Cell,(Y+.5f)*Cell,Z); if(!Set.Contains(FIntPoint(X,Y))||!RoofAllowed(N,Level)||!FMath::IsNearlyEqual(RoofHeight(N,Level),H)) break; ++X; }
    Collision(FVector((Start+X)*Cell*.5f,(Y+.5f)*Cell,Z+H+8),FVector((X-Start)*Cell,Cell,16),FRotator::ZeroRotator,TEXT("GridV3Ceiling"));
   }
  }
 }

 // The Core pass well removes floor slabs, not the facility's outer roof.
 Fit(TEXT("SM_FactoryFloorLarge01"),FVector(0,400,1328),FVector(400,400,16));
 Collision(FVector(0,400,1328),FVector(400,400,16),FRotator::ZeroRotator,TEXT("GridV3Ceiling"));

 // High containment bulkheads above ordinary main-floor door openings.
 for(int32 Side=0;Side<4;++Side) for(int32 I=-9;I<9;++I) {
  FVector P=Side<2?FVector((I+.5f)*100,Side==0?-900:900,0):FVector(Side==2?-900:900,(I+.5f)*100,0);
  const FVector Out=Side<2?FVector(0,Side==0?-1:1,0):FVector(Side==2?-1:1,0,0);
  const FIntPoint Outside(FMath::FloorToInt((P.X+Out.X*50)/100),FMath::FloorToInt((P.Y+Out.Y*50)/100));
  if(!Floors[1].Contains(Outside)) continue;
  const FVector Axis=Side<2?FVector(50,0,0):FVector(0,50,0);
  const bool UpperOpen=Floors[2].Contains(Outside);
  if(UpperOpen) { Wall(P-Axis,P+Axis,420,180); Wall(P-Axis,P+Axis,1020,300); }
  else Wall(P-Axis,P+Axis,420,900);
 }

 for(const auto& V:GridV3::VerticalLinks()) {
  const FVector Delta=V.Top-V.Bottom; const float Run=Delta.Size2D(); const float Angle=FMath::RadiansToDegrees(FMath::Atan2(Delta.Z,Run));
  const FVector Mid=(V.Bottom+V.Top)*.5f; const FRotator Slope(Angle,90,0);
  const FVector Normal=Slope.RotateVector(FVector(0,0,1));
  Collision(Mid-Normal*10,FVector(Delta.Size(),V.Width,20),Slope,TEXT("GridV3Ramp"));
  // This kit stair climbs toward local -X. Its full bounds include the handrail,
  // so neither +90 yaw nor fitting the bounding-box height describes its treads.
  // Three 16-tread modules give roughly 12.5 cm risers instead of stretching one flight
  // across the complete 600 cm rise. Anchors are measured from source tread faces.
  if(auto* Stairs=Group(TEXT("SM_Stair_01"))) {
   constexpr int32 Flights=3,TreadsPerFlight=16;
   constexpr double LowerFrontX=-6.5697,LowerTreadZ=33.0207;
   constexpr double UpperRearX=-522.4669,UpperTreadZ=336.8436;
   const FVector NativeSize=Stairs->GetStaticMesh()->GetBoundingBox().GetSize();
   const float FlightRun=Run/Flights,FlightRise=Delta.Z/Flights;
   const float Riser=FlightRise/TreadsPerFlight;
   const double ScaleZ=(FlightRise-Riser)/(UpperTreadZ-LowerTreadZ);
   const FVector Scale(FlightRun/(LowerFrontX-UpperRearX),V.Width/NativeSize.Y,ScaleZ);
   const FVector Anchor(LowerFrontX,Stairs->GetStaticMesh()->GetBoundingBox().GetCenter().Y,LowerTreadZ-Riser/ScaleZ);
   const FRotator Rotation(0,Delta.Rotation().Yaw+180.f,0);
   for(int32 Flight=0;Flight<Flights;++Flight) {
    const FVector Bottom=V.Bottom+Delta*(double(Flight)/Flights);
    Stairs->AddInstance(FTransform(Rotation,Bottom-Rotation.RotateVector(Anchor*Scale),Scale),true);
    ++InstanceCount;
   }
  }
  Fit(TEXT("SM_FactoryFloorLarge01"),Mid+FVector(0,0,428),FVector(Delta.Size(),V.Width+40,16),Slope);
  Collision(Mid+FVector(0,0,428),FVector(Delta.Size(),V.Width+40,16),Slope,TEXT("GridV3Ceiling"));
  // Side walls follow the incline, leaving crossing main-floor passages below high treads.
  for(int32 I=0;I<10;++I) {
   const FVector P=V.Bottom+Delta*((I+.5f)/10.f);
   for(float Side:{-1.f,1.f}) {
    const FVector C=P+FVector(Side*(V.Width*.5f+14),0,210);
    Fit(TEXT("SM_ConcreteWall"),C,FVector(200,28,420),FRotator(Angle,90,0));
    Collision(C,FVector(200,28,420),FRotator(Angle,90,0));
   }
  }
  for(int32 I=1;I<4;++I) {
   const FVector P=V.Bottom+Delta*(I/4.f)+FVector(0,0,330);
   Prop(TEXT("SM_LampSet_01"),P+FVector(0,0,40),1.5f,90);
   Point(P,FLinearColor(.8f,.85f,1.f),9000,620);
  }
  Sign(FString::Printf(TEXT("%s  /  %s"),V.Top.Z>0?TEXT("UPPER ACCESS"):TEXT("SERVICE ACCESS"),*FString::Printf(TEXT("S%02d"),V.Id+1)),V.Bottom+FVector(0,-40,280),-90,FLinearColor(.45f,.75f,1.f),28);
 }

 // Light, wayfinding and structural rhythm are authored per room, not per arena.
 for(const auto& R:GridV3::Rooms()) {
  const float Ceiling=R.Center.Z+FMath::Min(420.,R.Size.Z);
  const FLinearColor White=FLinearColor::LerpUsingHSV(FLinearColor(.85f,.9f,1.f),R.Color,.12f);
  Point(R.Center+FVector(0,0,R.Id==6?900:330),White,R.Id==6?72000.f:24000.f,R.Id==6?1500.f:FMath::Max(R.Size.X,R.Size.Y)*.8f,R.Id==6);
  for(float Side:{-1.f,1.f}) {
   const FVector P=R.Center+FVector(Side*R.Size.X*.27f,R.Size.Y*.22f,0);
   Prop(TEXT("SM_LampSet_01"),FVector(P.X,P.Y,Ceiling-28),1.8f,90);
   Point(P+FVector(0,0,300),White,8500,650);
   const FVector Column=R.Center+FVector(Side*(R.Size.X*.5f-65),R.Size.Y*.5f-65,205);
   if(R.Id!=19) { Fit(TEXT("SM_Column_02"),Column,FVector(55,70,410)); Collision(Column,FVector(55,70,410)); }
  }
  Fit(TEXT("SM_MetalBeam01"),R.Center+FVector(0,R.Size.Y*.22f,FMath::Min(420.,R.Size.Z)-45),FVector(R.Size.X-60,45,45));
  Sign(FString::Printf(TEXT("%02d  %s"),R.Id+1,R.Name),R.Center+FVector(-R.Size.X*.12f,R.Size.Y*.5f-22,R.Id==2?350:290),-90,R.Color,32);
  // Perimeter details remain outside the centerlines and the requested camera corners.
  const FVector East=R.Center+FVector(R.Size.X*.5f-110,-R.Size.Y*.28f,0);
  if(R.Id!=6 && R.Id!=18 && R.Id!=21) {
   Prop(TEXT("SM_ElectricalPanel_01"),East+FVector(0,0,100),1.15f,90);
   Fit(TEXT("SM_Pipe_round_long"),R.Center+FVector(R.Size.X*.5f-40,0,340),FVector(22,R.Size.Y-180,22));
  }
 }
 // Corridor fixture cadence: no decorative solids in doorways or center paths.
 TSet<FIntVector> FixtureCells;
 for(const auto& C:GridV3::Connections()) for(int32 I=1;I<C.Points.Num();++I) {
  const FVector A=C.Points[I-1],B=C.Points[I]; const float Distance=FVector::Dist(A,B);
  const int32 Count=FMath::Max(1,FMath::FloorToInt(Distance/700));
  for(int32 N=1;N<=Count;++N) {
   const FVector P=FMath::Lerp(A,B,float(N)/(Count+1)); bool InRoom=false;
   for(const auto& R:GridV3::Rooms()) if(FMath::IsNearlyEqual(P.Z,R.Center.Z)&&FMath::Abs(P.X-R.Center.X)<R.Size.X*.5f&&FMath::Abs(P.Y-R.Center.Y)<R.Size.Y*.5f) { InRoom=true; break; }
   if(InRoom) continue;
   const FIntVector Key(FMath::RoundToInt(P.X/400),FMath::RoundToInt(P.Y/400),FMath::RoundToInt(P.Z/600));
   if(FixtureCells.Contains(Key)) continue; FixtureCells.Add(Key);
   Prop(TEXT("SM_LampSet_01"),P+FVector(0,0,385),1.5f,FMath::Abs(B.X-A.X)>1?0:90);
   Point(P+FVector(0,0,320),P.Z<0?FLinearColor(1.f,.72f,.42f):FLinearColor(.65f,.8f,1.f),10000,550);
  }
 }

 // Distinct functional furnishings, kept away from the navigation center samples.
 for(int32 I=0;I<4;++I) {
  const FVector P(-2200+I*260,-200,0);
  Fit(TEXT("SM_ElectricalPanel_03"),P+FVector(0,0,150),FVector(180,100,300));
  Collision(P+FVector(0,0,150),FVector(180,100,300),FRotator::ZeroRotator,TEXT("GridV3Cover"));
  // Monitoring faces and a shared cooling plenum give this bank a data-room
  // identity instead of repeating the power room's tall caution cabinets.
  Fit(TEXT("SM_OfficeMonitor_02"),P+FVector(0,-58,215),FVector(140,22,85),FRotator(0,180,0));
  Fit(TEXT("SM_Platform_01"),P+FVector(0,-57,95),FVector(130,160,8),FRotator(-90,-90,0));
  for(float H:{60.f,90.f,120.f,150.f}) Fit(TEXT("SM_MetalBeam01"),P+FVector(0,-64,H),FVector(150,8,6));
 }
 Fit(TEXT("SM_CeilingVentilation01"),FVector(-1800,-500,400),FVector(600,180,32));
 Prop(TEXT("SM_DeskControl_01"),FVector(-2200,-2100,0),.8f,90,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(-1600,-2100,0),.8f,90,true);
 Prop(TEXT("SM_StorageShelves_group01"),FVector(-4000,350,0),.55f,0,true);
 Prop(TEXT("SM_StorageShelves_group03"),FVector(-3200,350,0),.55f,0,true);
 Prop(TEXT("SM_Boxes_group_01"),FVector(-3900,-250,0),.85f,20,true);
 Prop(TEXT("SM_ForkLift"),FVector(-3650,350,0),.8f,0,true);
 Prop(TEXT("SM_Container01_01"),FVector(-3650,2450,0),.7f,90,true);
 Prop(TEXT("SM_Boxes_group_06"),FVector(-2800,2500,0),1.f,0,true);
 Prop(TEXT("SM_AssemblyLineDesk01"),FVector(-1600,2400,0),1.f,0,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(-1000,2450,0),.8f,90,true);
 for(float X:{1700.f,2700.f}) {
  Prop(TEXT("SM_AssemblyLineDesk01"),FVector(X,-300,0),1.2f,0,true);
  Prop(TEXT("SM_AssemblyLineControl01"),FVector(X,-900,0),1.f,180,true);
  Prop(TEXT("SM_RoboArm04"),FVector(X,-300,120),.7f,0,false);
 }
 Prop(TEXT("SM_AssemblyLine02"),FVector(2200,-1000,0),1.f,0,true);
 Prop(TEXT("SM_Boiler_01"),FVector(2450,1300,0),.5f,0,true);
 for(float Y:{800.f,1100.f,1400.f}) {
  Fit(TEXT("SM_ElectricalPanel_01"),FVector(2700,Y,145),FVector(180,100,290),FRotator(0,90,0));
  Collision(FVector(2700,Y,145),FVector(100,180,290),FRotator::ZeroRotator,TEXT("GridV3Cover"));
 }
 Prop(TEXT("SM_DeskControl_01"),FVector(4150,-2450,0),1.f,90,true);
 Prop(TEXT("SM_AssemblyLineControl01"),FVector(3450,-1900,0),1.15f,0,true);
 Prop(TEXT("SM_ContainerP4_01"),FVector(4200,-1900,0),.45f,0,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(2500,2350,0),.85f,90,true);
 for(float X:{-3950.f,-3300.f}) Prop(TEXT("SM_Boxes_group_06"),FVector(X,-2700,0),1.f,0,true);
 for(float X:{3650.f,4350.f}) Prop(TEXT("SM_Boxes_group_06"),FVector(X,2400,0),1.f,0,true);

 // Lower infrastructure: machinery defines sheltered positions, never invisible aisles.
 Prop(TEXT("SM_Boiler_01"),FVector(-2050,-2650,-600),.48f,0,true);
 Prop(TEXT("SM_Boiler_01"),FVector(-1350,-2650,-600),.48f,0,true);
 for(float X:{200.f,1000.f}) Prop(TEXT("SM_ElectricalPanel_01"),FVector(X,-2750,-500),1.5f,0,false);
 Prop(TEXT("SM_Boxes_group_01"),FVector(-3850,-1800,-600),.65f,0,true);
 Prop(TEXT("SM_AssemblyLineDesk01"),FVector(3900,-1250,-600),1.1f,0,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(1700,2000,-600),.8f,90,true);
 for(float Side:{-1.f,1.f}) {
  Fit(TEXT("SM_Pipe_round_long"),FVector(Side*650,0,-250),FVector(40,1400,40));
  Prop(TEXT("SM_ElectricalPanel_01"),FVector(Side*650,-500,-500),1.f,Side*90,false);
 }
 // The upper galleries have low equipment and open local containment overlooks.
 Prop(TEXT("SM_DeskControl_01"),FVector(-650,300,600),.6f,0,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(1300,1800,600),.75f,90,true);
 Prop(TEXT("SM_DeskControl_01"),FVector(2100,1800,600),.75f,90,true);
 Prop(TEXT("SM_Boxes_group_06"),FVector(3050,-2100,600),.8f,0,true);
 Prop(TEXT("SM_AssemblyLineDesk01"),FVector(2400,-2100,600),1.f,0,true);

 // Containment machinery surrounds clear crossing lanes and the neutral Core.
 for(const FVector P:{FVector(-650,-780,0),FVector(650,650,0)}) {
  Prop(TEXT("SM_Boiler_01"),P,.3f,0,true);
  Fit(TEXT("SM_ConcretePillar01"),P+FVector(0,0,650),FVector(90,90,1300));
  Collision(P+FVector(0,0,650),FVector(90,90,1300));
 }
 for(float Y:{-780.f,780.f}) Fit(TEXT("SM_MetalBeam01"),FVector(0,Y,1150),FVector(1800,100,100));
 Fit(TEXT("SM_MetalBeam01"),FVector(750,0,1150),FVector(1800,100,100),FRotator(0,90,0));
 // Visible load paths: deep bridge bearers, suspension rods and upper trusses.
 for(float Y:{-250.f,250.f}) {
  if(Y<0) {
   // Keep the southern bearer outside stair 04's upper opening and handrails.
   const auto& Access=GridV3::VerticalLinks()[3];
   const float Left=Access.Top.X-Access.Width*.5f-20,Right=Access.Top.X+Access.Width*.5f+20;
   Fit(TEXT("SM_MetalBeam01"),FVector((300+Left)*.5f,Y,565),FVector(Left-300,55,70));
   Fit(TEXT("SM_MetalBeam01"),FVector((Right+1700)*.5f,Y,565),FVector(1700-Right,55,70));
  } else Fit(TEXT("SM_MetalBeam01"),FVector(1000,Y,565),FVector(1400,55,70));
  Fit(TEXT("SM_Column_02"),FVector(600,Y,910),FVector(18,18,680));
  Collision(FVector(600,Y,910),FVector(18,18,680),FRotator::ZeroRotator,TEXT("GridV3Structure"));
  Fit(TEXT("SM_MetalBeam01"),FVector(0,Y,1250),FVector(1800,75,90));
 }
 for(float X:{-600.f,600.f}) Fit(TEXT("SM_MetalWall_02"),FVector(X,-883,900),FVector(400,30,550));
 for(float X:{150.f,450.f}) Prop(TEXT("SM_PedestrianPath01"),FVector(X,-300,1),.6f,0,false);
 Sign(TEXT("MERIDIAN / CORE CONTAINMENT"),FVector(0,-865,1050),90,FLinearColor(.3f,.7f,1.f),48);
 Sign(TEXT("LOWER ACCESS / CORE PASS"),FVector(0,680,260),-90,FLinearColor(.9f,.6f,.15f),24);
 // Persistent editor sockets reserve future SHIFT integration. Target points
 // have no interaction/tick/collision logic and cannot activate an M4 mechanic.
 auto Socket=[&](const TCHAR* Name,const TCHAR* Type,const FVector& P) {
  FActorSpawnParameters Params; Params.Name=FName(Name);
  auto* Marker=World->SpawnActor<ATargetPoint>(P,FRotator::ZeroRotator,Params);
  if(Marker) { Marker->Tags.Add(TEXT("GridV3FutureShift")); Marker->Tags.Add(FName(Type)); Marker->Tags.Add(FName(Name)); }
 };
 Socket(TEXT("GridV3_SecurityDoor_Alpha"),TEXT("SHIFT_SecurityDoor"),FVector(-1300,-2400,0));
 Socket(TEXT("GridV3_SecurityDoor_Bravo"),TEXT("SHIFT_SecurityDoor"),FVector(2800,2100,0));
 Socket(TEXT("GridV3_ServiceLift_West"),TEXT("SHIFT_LiftLanding"),FVector(-4400,-1900,-600));
 Socket(TEXT("GridV3_ServiceLift_East"),TEXT("SHIFT_LiftLanding"),FVector(4400,-1000,-600));
 Socket(TEXT("GridV3_ContainmentBridge"),TEXT("SHIFT_MovableBridge"),FVector(1000,0,600));
 Socket(TEXT("GridV3_PowerTerminal"),TEXT("SHIFT_HackTerminal"),FVector(2700,1100,120));
 Socket(TEXT("GridV3_SecurityTerminal"),TEXT("SHIFT_HackTerminal"),FVector(-2200,-2100,120));
 Socket(TEXT("GridV3_LabBreachPanel"),TEXT("SHIFT_BreachableWall"),FVector(2900,0,180));
 Socket(TEXT("GridV3_Camera_Security"),TEXT("SHIFT_SecurityCamera"),FVector(-2400,-2700,340));
 Socket(TEXT("GridV3_Camera_Containment"),TEXT("SHIFT_SecurityCamera"),FVector(780,-780,900));
 Socket(TEXT("GridV3_Camera_ExtractionA"),TEXT("SHIFT_SecurityCamera"),FVector(-3900,2700,340));
 Socket(TEXT("GridV3_Camera_ExtractionB"),TEXT("SHIFT_SecurityCamera"),FVector(4350,-2700,340));
 for(const auto& R:GridV3::Rooms()) Socket(*FString::Printf(TEXT("GridV3_LightingSector_%02d"),R.Id),TEXT("SHIFT_LightingSector"),R.Center+FVector(0,0,300));
 UE_LOG(LogVanthex,Display,TEXT("GRIDV3 geometry: rooms=%d connections=%d vertical=%d kitGroups=%d instances=%d proxies=%d lights=%d missing=%d"),GridV3::Rooms().Num(),GridV3::Connections().Num(),GridV3::VerticalLinks().Num(),Groups.Num(),InstanceCount,CollisionCount,LightCount,MissingCount);
}
