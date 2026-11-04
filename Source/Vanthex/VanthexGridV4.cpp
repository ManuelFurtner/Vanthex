#include "VanthexTestArena.h"
#include "VanthexGridV4Layout.h"
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
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#if WITH_EDITOR
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#endif

namespace {
UMaterialInterface* AegisPalette(UStaticMesh* Mesh,const FString& Key,int32 Slot){
 const bool Accent=Key.Contains(TEXT("#"));
 if(!Accent&&!Key.Contains(TEXT("Concrete"))&&!Key.Contains(TEXT("Platform_01"))&&!Key.Contains(TEXT("MetalBeam"))&&!Key.Contains(TEXT("MetalWall")))return nullptr;
 FString Safe=Key.Replace(TEXT("#"),TEXT("_"));
 FString Name=FString::Printf(TEXT("MI_Aegis_%s_%d"),*Safe,Slot);
 FString Path=TEXT("/Game/Vanthex/Materials/Aegis/")+Name;
#if WITH_EDITOR
 if(IsRunningCommandlet()){
  auto* Package=FPackageName::DoesPackageExist(Path)?LoadPackage(nullptr,*Path,LOAD_None):CreatePackage(*Path);
  if(!Package)return nullptr;Package->FullyLoad();
  auto* M=FindObject<UMaterialInstanceConstant>(Package,*Name);
  if(!M)M=NewObject<UMaterialInstanceConstant>(Package,FName(Name),RF_Public|RF_Standalone);
  auto* Parent=Accent?VanthexArt::GetEnergyMaterial():Mesh->GetMaterial(Slot);
  if(!Parent)return nullptr;M->SetParentEditorOnly(Parent);
  FLinearColor Tint=Key.Contains(TEXT("Floor"))?FLinearColor(.14f,.17f,.19f):Key.Contains(TEXT("ConcreteWall"))?FLinearColor(.52f,.50f,.45f):FLinearColor(.12f,.16f,.19f);
  if(Accent){
   Tint=Key.Contains(TEXT("Amber"))?FLinearColor(1.f,.32f,.045f):Key.Contains(TEXT("Green"))?FLinearColor(.05f,1.f,.35f):Key.Contains(TEXT("Violet"))?FLinearColor(.7f,.15f,1.f):FLinearColor(.04f,.65f,1.f);
   M->SetVectorParameterValueEditorOnly(TEXT("Color"),Tint*1.35f);M->SetVectorParameterValueEditorOnly(TEXT("EmissiveColor"),Tint*1.35f);
  }else{
   M->SetVectorParameterValueEditorOnly(TEXT("ColorA"),Tint);M->SetVectorParameterValueEditorOnly(TEXT("ColorB"),Tint*.65f);M->SetVectorParameterValueEditorOnly(TEXT("ColorC"),Tint*.8f);
  }
  M->PostEditChange();Package->MarkPackageDirty();
  FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  return UPackage::SavePackage(Package,M,*File,Args)?M:nullptr;
 }
#endif
 return LoadObject<UMaterialInterface>(nullptr,*(Path+TEXT(".")+Name));
}
}


// This builder deliberately has no dependency on the old arena footprints.
void AVanthexTestArena::BuildGridV4()
{
 UWorld* World=GetWorld();
 if(!World) return;
 CubeMesh=VanthexArt::LoadCube();
 BuildAtmosphere();
 for(TActorIterator<ASkyLight> It(World);It;++It)It->GetLightComponent()->SetIntensity(.28f);
 for(TActorIterator<APostProcessVolume> It(World);It;++It){It->Settings.AutoExposureBias=0.f;It->Settings.BloomIntensity=.1f;}

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
  FString MeshName=Name;int32 Hash=INDEX_NONE;if(Name.FindChar(TCHAR('#'),Hash))MeshName=Name.Left(Hash);
  const FString Sub=MeshName==TEXT("SM_RoboArm04")?TEXT("Robot/"):TEXT("");
  UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Meshes/")+Sub+MeshName+TEXT(".")+MeshName));
  if(!Mesh) { ++MissingCount; UE_LOG(LogVanthex,Error,TEXT("GRIDV4 missing kit mesh %s"),*Name); return nullptr; }
  auto* G=NewObject<UInstancedStaticMeshComponent>(this,FName(*(TEXT("GridV4_")+Name)));
  G->SetStaticMesh(Mesh); G->SetupAttachment(SceneRoot); G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  for(int32 Slot=0;Slot<Mesh->GetStaticMaterials().Num();++Slot)if(auto* Palette=AegisPalette(Mesh,Name,Slot))G->SetMaterial(Slot,Palette);
  G->SetCastShadow(!Name.Contains(TEXT("#"))); G->ComponentTags.Add(TEXT("GridV4Kit")); G->RegisterComponent(); Groups.Add(Name,G);
  return G;
 };
 auto Fit=[&](const FString& Name,const FVector& Center,const FVector& Size,const FRotator& Rotation=FRotator::ZeroRotator) {
  auto* G=Group(Name); if(!G) return;
  const FBox Box=G->GetStaticMesh()->GetBoundingBox(); const FVector Native=Box.GetSize();
  const FVector Scale(Size.X/FMath::Max(1.,Native.X),Size.Y/FMath::Max(1.,Native.Y),Size.Z/FMath::Max(1.,Native.Z));
  G->AddInstance(FTransform(Rotation,Center-Rotation.RotateVector(Box.GetCenter()*Scale),Scale),true); ++InstanceCount;
 };
 auto Collision=[&](const FVector& Center,const FVector& Size,const FRotator& Rotation=FRotator::ZeroRotator,const TCHAR* Tag=TEXT("GridV4Structure")) {
  auto* C=SpawnBlock(Center,Size/100.f,FLinearColor::Gray,.8f,.1f,EVanthexSurface::Concrete);
  if(C) { C->SetWorldRotation(Rotation); C->SetVisibility(false); C->SetCastShadow(false); C->ComponentTags.Add(Tag); ++CollisionCount; }
 };
 auto Prop=[&](const FString& Name,const FVector& Ground,float Scale=1.f,float Yaw=0.f,bool Solid=false) {
  auto* G=Group(Name); if(!G) return;
  const FBox Box=G->GetStaticMesh()->GetBoundingBox(); const FRotator R(0,Yaw,0);
  const FVector Offset(Box.GetCenter().X,Box.GetCenter().Y,Box.Min.Z);
  G->AddInstance(FTransform(R,Ground-R.RotateVector(Offset*Scale),FVector(Scale)),true); ++InstanceCount;
  if(Solid) Collision(Ground+FVector(0,0,Box.GetSize().Z*Scale*.5f),Box.GetSize()*Scale,R,TEXT("GridV4Cover"));
 };
 auto CeilingLamp=[&](FVector Ground,float Ceiling,float Yaw=0.f) {
  Fit(TEXT("SM_LampSet_01"),FVector(Ground.X,Ground.Y,Ceiling-12),FVector(120,38,24),FRotator(0,Yaw,0));
 };
 auto Point=[&](const FVector& P,FLinearColor Color,float Intensity,float Radius,bool Shadow=false) {
  auto* A=World->SpawnActor<APointLight>(P,FRotator::ZeroRotator); if(!A) return;
  auto* L=Cast<UPointLightComponent>(A->GetLightComponent()); L->SetMobility(EComponentMobility::Movable);
  L->SetLightColor(Color); L->SetIntensity(Intensity*.55f); L->SetAttenuationRadius(Radius); L->SetCastShadows(Shadow);
  A->Tags.Add(TEXT("GridV4Light")); ++LightCount;
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
  A->Tags.Add(TEXT("GridV4Sign"));
 };

 constexpr float Cell=100.f;
 TSet<FIntPoint> Floors[3];
 auto AddRect=[&](int32 L,float MinX,float MinY,float MaxX,float MaxY) {
  for(int32 X=FMath::FloorToInt(MinX/Cell);X<FMath::CeilToInt(MaxX/Cell);++X)
   for(int32 Y=FMath::FloorToInt(MinY/Cell);Y<FMath::CeilToInt(MaxY/Cell);++Y) Floors[L+1].Add(FIntPoint(X,Y));
 };
 for(const auto& R:GridV4::Rooms()) AddRect(R.Level,R.Center.X-R.Size.X*.5f,R.Center.Y-R.Size.Y*.5f,R.Center.X+R.Size.X*.5f,R.Center.Y+R.Size.Y*.5f);
 for(const auto& C:GridV4::Connections()) for(int32 I=1;I<C.Points.Num();++I) {
  const FVector A=C.Points[I-1],B=C.Points[I]; const float H=C.Width*.5f;
  AddRect(FMath::RoundToInt(A.Z/600.f),FMath::Min(A.X,B.X)-H,FMath::Min(A.Y,B.Y)-H,FMath::Max(A.X,B.X)+H,FMath::Max(A.Y,B.Y)+H);
 }
 auto InRamp=[&](const FVector& P,const GridV4::FVerticalLink& V,float Extra=0.f) {
  return FMath::Abs(P.X-V.Bottom.X)<=V.Width*.5f+Extra && P.Y>=V.Bottom.Y-Extra && P.Y<V.Top.Y+Extra;
 };
 // The upper slab must end at the actual top of the incline, not cover its last treads.
 for(const auto& V:GridV4::VerticalLinks()) {
  auto& F=Floors[FMath::RoundToInt(V.Top.Z/600.f)+1];
  TArray<FIntPoint> Remove;
  for(const auto& C:F) if(InRamp(FVector((C.X+.5f)*Cell,(C.Y+.5f)*Cell,0),V)) Remove.Add(C);
  for(const auto& C:Remove) F.Remove(C);
 }
 for(int32 L=0;L<=1;++L) {
  TArray<FIntPoint> Remove;
  for(const auto& C:Floors[L+1]) if(GridV4::IsCoreAperture(FVector((C.X+.5f)*Cell,(C.Y+.5f)*Cell,0))) Remove.Add(C);
  for(const auto& C:Remove) Floors[L+1].Remove(C);
 }
 auto CoreFootprint=[](const FVector& P) { return FMath::Abs(P.X)<=1200 && FMath::Abs(P.Y)<=1200; };
 auto RoofHeight=[&](const FVector& P,int32 Level) { return Level==0 && CoreFootprint(P)?1320.f:420.f; };
 auto RoofAllowed=[&](const FVector& P,int32 Level) {
  if(Level==1&&CoreFootprint(P))return false;
  if(Level==-1 && GridV4::IsCoreAperture(P)) return false;
  for(const auto& V:GridV4::VerticalLinks()) if(FMath::IsNearlyEqual(Level*600.f,float(V.Bottom.Z)) && InRamp(P,V,0)) return false;
  return true;
 };
 auto BoundaryHeight=[&](const FVector& P,int32 Level) {
  for(const auto& V:GridV4::VerticalLinks()) if(FMath::IsNearlyEqual(Level*600.f,float(V.Bottom.Z)) && InRamp(P,V,1)) {
   const float T=FMath::Clamp(float((P.Y-V.Bottom.Y)/(V.Top.Y-V.Bottom.Y)),0.f,1.f);
   return FMath::Max(0.f,float(V.Top.Z-V.Bottom.Z)*T);
  }
  return RoofHeight(P,Level);
 };
 auto Wall=[&](const FVector& A,const FVector& B,float Bottom,float Height,bool Rail=false,const TCHAR* Tag=TEXT("GridV4Structure")) {
  const FVector Mid=(A+B)*.5f+FVector(0,0,Bottom+Height*.5f);
  const bool AlongX=FMath::Abs(B.X-A.X)>FMath::Abs(B.Y-A.Y);
  const float Length=FVector::Dist2D(A,B); const FRotator Rot(0,AlongX?0.f:90.f,0);
  if(Length<1 || Height<1) return;
  if(Rail) {
   // Rail has matching bar collision, leaving the gap above it open to Core passes.
   Fit(TEXT("SM_MetalBeam01"),Mid+FVector(0,0,Height*.5f-8),FVector(Length,12,16),Rot);
   Collision(Mid+FVector(0,0,Height*.5f-8),FVector(Length,12,16),Rot,TEXT("GridV4Rail"));
   for(float D=0;D<=Length;D+=200) {
    const FVector P=A+(B-A)*(D/Length)+FVector(0,0,Bottom+Height*.5f);
    Fit(TEXT("SM_Column_02"),P,FVector(12,12,Height)); Collision(P,FVector(12,12,Height),FRotator::ZeroRotator,TEXT("GridV4Rail"));
   }
   return;
  }
  Collision(Mid,FVector(Length,28,Height),Rot,Tag);
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
  for(int32 Y=-50;Y<59;++Y) for(int32 X=-64;X<68;++X) {
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
   Collision(P,FVector(W*Cell,H*Cell,16),FRotator::ZeroRotator,TEXT("GridV4Floor"));
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
   for(int32 Y=-50;Y<59;++Y) for(int32 X=-64;X<68;++X) {
    FIntPoint C(X,Y); if(!Edges.Contains(C)||Used.Contains(C)) continue;
    const bool AlongX=Dir<2; const FIntPoint Step=AlongX?FIntPoint(1,0):FIntPoint(0,1);
    FVector Edge=(FVector((X+.5f)*Cell,(Y+.5f)*Cell,0)+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*Cell*.5f);
    const float Height=BoundaryHeight(Edge,Level);
    auto OpenRamp=[&](FVector P) {
     for(const auto& V:GridV4::VerticalLinks()) if(FMath::Abs(P.X-V.Bottom.X)<V.Width*.5f+1) {
      if(FMath::IsNearlyEqual(Z,float(V.Bottom.Z)) && FMath::Abs(P.Y-V.Bottom.Y)<1)return true;
      if(FMath::IsNearlyEqual(Z,float(V.Top.Z)) && FMath::Abs(P.Y-V.Top.Y)<1)return true;
     }
     return false;
    };
    auto SafeRail=[&](const FVector& P) {
     const FVector Out=P+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*50;
     return Level==1 && CoreFootprint(Out) && Floors[1].Contains(FIntPoint(FMath::FloorToInt(Out.X/100),FMath::FloorToInt(Out.Y/100)));
    };
    auto UpperCrossing=[&](const FVector& P) {
     const FVector Out=P+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*50;
     return Level==0 && RoofHeight(P,Level)>600 && Floors[2].Contains(FIntPoint(FMath::FloorToInt(Out.X/100),FMath::FloorToInt(Out.Y/100)));
    };
    const bool Skip=OpenRamp(Edge);
    const bool UpperDoor=UpperCrossing(Edge);
    const bool Rail=SafeRail(Edge) || (Level==0 && GridV4::IsCoreAperture(Edge+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*10));
    int32 N=1; Used.Add(C);
    while(N<120) {
     const FIntPoint Next=C+Step*N; const FVector NP=Edge+FVector(Step.X,Step.Y,0)*Cell*N;
     const bool NextRail=SafeRail(NP)||(Level==0&&GridV4::IsCoreAperture(NP+FVector(Dirs[Dir].X,Dirs[Dir].Y,0)*10));
     if(!Edges.Contains(Next)||Used.Contains(Next)||!FMath::IsNearlyEqual(BoundaryHeight(NP,Level),Height)||OpenRamp(NP)!=Skip||NextRail!=Rail||UpperCrossing(NP)!=UpperDoor) break;
     Used.Add(Next); ++N;
    }
    if(Skip) continue;
    const FVector Axis(Step.X,Step.Y,0); const FVector A=Edge-Axis*Cell*.5f,B=A+Axis*Cell*N;
    if(UpperDoor) { Wall(A,B,Z,600); Wall(A,B,Z+1020,Height-1020); }
    else Wall(A,B,Z,Rail?105.f:Height,Rail);
   }
  }
  // Collision for ceilings uses strips, independently of visual100cm tiles.
  for(int32 Y=-50;Y<59;++Y) {
   int32 X=-64;
   while(X<68) {
    const FVector P((X+.5f)*Cell,(Y+.5f)*Cell,Z); const float H=RoofHeight(P,Level);
    if(!Set.Contains(FIntPoint(X,Y))||!RoofAllowed(P,Level)) { ++X; continue; }
    const int32 Start=X++;
    while(X<68) { const FVector N((X+.5f)*Cell,(Y+.5f)*Cell,Z); if(!Set.Contains(FIntPoint(X,Y))||!RoofAllowed(N,Level)||!FMath::IsNearlyEqual(RoofHeight(N,Level),H)) break; ++X; }
    Collision(FVector((Start+X)*Cell*.5f,(Y+.5f)*Cell,Z+H+8),FVector((X-Start)*Cell,Cell,16),FRotator::ZeroRotator,TEXT("GridV4Ceiling"));
   }
  }
 }

 // The Core pass well removes floor slabs, not the facility's outer roof.
 Fit(TEXT("SM_FactoryFloorLarge01"),FVector(600,500,1328),FVector(400,400,16));
 Collision(FVector(600,500,1328),FVector(400,400,16),FRotator::ZeroRotator,TEXT("GridV4Ceiling"));

 // High containment bulkheads above ordinary main-floor door openings.
 for(int32 Side=0;Side<4;++Side) for(int32 I=-12;I<12;++I) {
  FVector P=Side<2?FVector((I+.5f)*100,Side==0?-1200:1200,0):FVector(Side==2?-1200:1200,(I+.5f)*100,0);
  const FVector Out=Side<2?FVector(0,Side==0?-1:1,0):FVector(Side==2?-1:1,0,0);
  const FIntPoint Outside(FMath::FloorToInt((P.X+Out.X*50)/100),FMath::FloorToInt((P.Y+Out.Y*50)/100));
  if(!Floors[1].Contains(Outside)) continue;
  const FVector Axis=Side<2?FVector(50,0,0):FVector(0,50,0);
  const bool UpperOpen=Floors[2].Contains(Outside);
  if(UpperOpen) { Wall(P-Axis,P+Axis,420,180); Wall(P-Axis,P+Axis,1020,300); }
  else Wall(P-Axis,P+Axis,420,900);
 }

 for(const auto& V:GridV4::VerticalLinks()) {
  const FVector Delta=V.Top-V.Bottom; const float Run=Delta.Size2D(); const float Angle=FMath::RadiansToDegrees(FMath::Atan2(Delta.Z,Run));
  const FVector Mid=(V.Bottom+V.Top)*.5f; const FRotator Slope(Angle,90,0);
  const FVector Normal=Slope.RotateVector(FVector(0,0,1));
  Collision(Mid-Normal*10,FVector(Delta.Size(),V.Width,20),Slope,TEXT("GridV4Ramp"));
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
  Collision(Mid+FVector(0,0,428),FVector(Delta.Size(),V.Width+40,16),Slope,TEXT("GridV4Ceiling"));
  // Vertical panels end exactly at the landing planes. Rotated tall panels
  // protruded beyond the top and obstructed the perpendicular corridor.
  for(int32 I=0;I<20;++I) for(float Side:{-1.f,1.f}) {
   const FVector C=V.Bottom+Delta*((I+.5f)/20.f)+FVector(Side*(V.Width*.5f+14),0,210);
   Collision(C,FVector(28,Run/20.f,480));
  }
  // Continuous visible cladding hides the stepped collision envelope. Its
  // complete rotated bounds stay between the two landing planes.
  const float VisualRun=(Run-440.f*FMath::Sin(FMath::DegreesToRadians(Angle)))/FMath::Cos(FMath::DegreesToRadians(Angle));
  for(float Side:{-1.f,1.f}) {
   Fit(TEXT("SM_ConcreteWall"),Mid+FVector(Side*(V.Width*.5f+14),0,210),FVector(VisualRun,28,440),FRotator(Angle,90,0));
   for(float T:{.05f,.95f})Fit(TEXT("SM_ConcreteWall"),V.Bottom+Delta*T+FVector(Side*(V.Width*.5f+14),0,210),FVector(28,Run*.1f,480));
  }
  for(int32 I=1;I<4;++I) {
   const FVector P=V.Bottom+Delta*(I/4.f)+FVector(0,0,330);
   CeilingLamp(P,P.Z+90,90);
   Point(P,FLinearColor(.8f,.85f,1.f),6000,600);
  }
  Sign(FString::Printf(TEXT("%s  /  %s"),V.Top.Z>0?TEXT("UPPER ACCESS"):TEXT("SERVICE ACCESS"),*FString::Printf(TEXT("S%02d"),V.Id+1)),V.Bottom+FVector(0,-40,280),-90,FLinearColor(.45f,.75f,1.f),28);
 }

 // Light, wayfinding and structural rhythm are authored per room, not per arena.
 for(const auto& R:GridV4::Rooms()) {
  const bool OpenCore=R.Id==3||R.Id==17||R.Id==18;
  const FLinearColor White=FLinearColor::LerpUsingHSV(FLinearColor(.85f,.9f,1.f),R.Color,.12f);
  Point(R.Center+FVector(0,0,R.Id==3?900:330),White,R.Id==3?40000.f:18000.f,R.Id==3?1600.f:FMath::Max(R.Size.X,R.Size.Y)*.95f,R.Id==3);
  for(float Side:{-1.f,1.f}) {
   const FVector P=R.Center+FVector(Side*R.Size.X*.27f,R.Size.Y*.22f,0);
   float MountedCeiling=R.Center.Z+RoofHeight(P,R.Level);
   if(OpenCore && CoreFootprint(P))MountedCeiling=1320;
   const FIntPoint CellXY(FMath::FloorToInt(P.X/100),FMath::FloorToInt(P.Y/100));
   if(R.Level==0 && Floors[2].Contains(CellXY))MountedCeiling=584;
   CeilingLamp(P,MountedCeiling,90);
   Point(P+FVector(0,0,285),White,6500,850);
  }
  if(!OpenCore) Fit(TEXT("SM_MetalBeam01"),R.Center+FVector(0,R.Size.Y*.22f,397.5),FVector(R.Size.X,45,45));
  if(R.Id!=22 && R.Id!=15 && !OpenCore) Sign(FString::Printf(TEXT("%02d  %s"),R.Id+1,R.Name),R.Center+FVector(-R.Size.X*.12f,R.Size.Y*.5f-22,R.Id==2?350:290),-90,R.Color,32);
 }
 // Corridor fixture cadence: no decorative solids in doorways or center paths.
 TSet<FIntVector> FixtureCells;
 for(const auto& C:GridV4::Connections()) for(int32 I=1;I<C.Points.Num();++I) {
  const FVector A=C.Points[I-1],B=C.Points[I]; const float Distance=FVector::Dist(A,B);
  const int32 Count=FMath::Max(1,FMath::FloorToInt(Distance/900));
  for(int32 N=1;N<=Count;++N) {
   const FVector P=FMath::Lerp(A,B,float(N)/(Count+1)); bool InRoom=false;
   for(const auto& R:GridV4::Rooms()) if(FMath::IsNearlyEqual(P.Z,R.Center.Z)&&FMath::Abs(P.X-R.Center.X)<R.Size.X*.5f&&FMath::Abs(P.Y-R.Center.Y)<R.Size.Y*.5f) { InRoom=true; break; }
   bool UnderStair=false;
   for(const auto& V:GridV4::VerticalLinks()) if(P.Z>=V.Bottom.Z-10 && P.Z<=V.Top.Z && InRamp(P,V,100)) {UnderStair=true;break;}
   if(InRoom || UnderStair) continue;
   const FIntVector Key(FMath::RoundToInt(P.X/400),FMath::RoundToInt(P.Y/400),FMath::RoundToInt(P.Z/600));
   if(FixtureCells.Contains(Key)) continue; FixtureCells.Add(Key);
   float MountedCeiling=P.Z+420;
   if(CoreFootprint(P) && P.Z>=0)MountedCeiling=1320;
   if(P.Z==0 && Floors[2].Contains(FIntPoint(FMath::FloorToInt(P.X/100),FMath::FloorToInt(P.Y/100))))MountedCeiling=584;
   CeilingLamp(P,MountedCeiling,FMath::Abs(B.X-A.X)>1?0:90);
   Point(P+FVector(0,0,290),P.Z<0?FLinearColor(1.f,.84f,.64f):FLinearColor(.8f,.88f,1.f),8000,750);
  }
 }


 // Keep furnishing collision outside route centrelines, including room entry bends.
 auto ClearLane=[&](const FVector& P,float Radius){
  for(const auto& C:GridV4::Connections())for(int32 I=1;I<C.Points.Num();++I){
   const auto A=C.Points[I-1],B=C.Points[I];if(FMath::Abs(A.Z-P.Z)>80)continue;
   if(FMath::PointDistToSegment(P,A,B)<Radius+125)return false;
  }
  for(const auto& V:GridV4::VerticalLinks())if(FMath::PointDistToSegment(P,V.Bottom,V.Top)<Radius+170)return false;
  return true;
 };
 auto SupportedFootprint=[&](const FVector& P,const FVector& Size,float Yaw) {
  const int32 L=FMath::RoundToInt(P.Z/600.f)+1;if(L<0||L>2)return false;
  for(float X:{-.5f,0.f,.5f})for(float Y:{-.5f,0.f,.5f}) {
   const FVector Q=P+FRotator(0,Yaw,0).RotateVector(FVector(X*Size.X,Y*Size.Y,0));
   if(!Floors[L].Contains(FIntPoint(FMath::FloorToInt(Q.X/100),FMath::FloorToInt(Q.Y/100))))return false;
   for(const auto& V:GridV4::VerticalLinks())if(InRamp(Q,V,30) && Q.Z>=V.Bottom.Z && Q.Z<=V.Top.Z)return false;
  }
  return true;
 };
 FCollisionObjectQueryParams FurnitureObjects;FurnitureObjects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams FurnitureClearance(SCENE_QUERY_STAT(GridV4FurniturePlacement),false);
 TArray<UPrimitiveComponent*> BuildingParts;GetComponents(BuildingParts);
 for(auto* C:BuildingParts)if(C->ComponentHasTag(TEXT("GridV4Floor")))FurnitureClearance.AddIgnoredComponent(C);
 auto Machine=[&](const FString& Name,const FVector& P,const FVector& Size,float Yaw=0.f){
  if(!ClearLane(P,FMath::Max(Size.X,Size.Y)*.5f)||!SupportedFootprint(P,Size,Yaw))return false;
  if(World->OverlapAnyTestByObjectType(P+FVector(0,0,Size.Z*.5f),FRotator(0,Yaw,0).Quaternion(),FurnitureObjects,FCollisionShape::MakeBox(Size*.5f-FVector(2)),FurnitureClearance))return false;
  Fit(Name,P+FVector(0,0,Size.Z*.5f),Size,FRotator(0,Yaw,0));
  Collision(P+FVector(0,0,Size.Z*.5f),Size,FRotator(0,Yaw,0),TEXT("GridV4Cover"));
  return true;
 };
 for(const auto& R:GridV4::Rooms()){
  // Repeated ribs, ceiling services and a restrained illuminated wayfinding strip.
  const FVector C=R.Center;const float HX=R.Size.X*.5f,HY=R.Size.Y*.5f;
  const FString Glow=(R.Id==14||R.Id==15)?TEXT("SM_MetalBeam01#Violet"):(R.Id==21||R.Id==22)?TEXT("SM_MetalBeam01#Green"):(R.Level<0||R.Id==5||R.Id==6||R.Id==8)?TEXT("SM_MetalBeam01#Amber"):TEXT("SM_MetalBeam01#Cyan");
  if(R.Id!=3 && R.Id!=17 && R.Id!=18) {
   const float W=FMath::Min(650.f,float(R.Size.X-160));
   Fit(Glow,C+FVector(0,HY-30,360),FVector(W,9,7));
   for(float Side:{-1.f,1.f})Fit(TEXT("SM_MetalBeam01"),C+FVector(Side*W*.45f,HY-30,391),FVector(8,8,58));
  }
  for(float Side:{-1.f,1.f}){
   bool Supported=R.Id!=3 && R.Id!=17 && R.Id!=18;
   for(float Offset:{-190.f,0.f,190.f}) {
    const FVector Outside=C+FVector(Side*(HX+50),HY*.15f+Offset,0);
    Supported &= !Floors[R.Level+1].Contains(FIntPoint(FMath::FloorToInt(Outside.X/100),FMath::FloorToInt(Outside.Y/100)));
   }
   if(Supported) Fit(TEXT("SM_MetalWall_02"),C+FVector(Side*(HX-22),HY*.15f,160),FVector(400,12,180),FRotator(0,90,0));
   if(R.Id!=3 && R.Id!=17 && R.Id!=18) Fit(TEXT("SM_CeilingVentilation01"),C+FVector(Side*HX*.45f,0,409),FVector(100,FMath::Min(900.f,float(R.Size.Y-150)),22));
  }
  const FVector NW=C+FVector(-HX+170,HY-180,0),NE=C+FVector(HX-180,HY-180,0);
  const FVector SW=C+FVector(-HX+180,-HY+180,0),SE=C+FVector(HX-180,-HY+180,0);
  if(R.Id==2){
   for(float X:{-380.f,0.f,380.f})for(float Side:{-1.f,1.f}){
    const FVector P=C+FVector(X,Side*(HY-140),0);
    if(!ClearLane(P,120))continue;
    if(!Machine(TEXT("SM_ElectricalPanel_03"),P,FVector(240,120,300)))continue;
    Fit(TEXT("SM_OfficeMonitor_02"),P+FVector(0,-Side*65,210),FVector(180,16,75),FRotator(0,Side>0?180:0,0));
    for(float Z:{65.f,115.f,165.f})Fit(TEXT("SM_MetalBeam01#Cyan"),P+FVector(0,-Side*68,Z),FVector(170,3,3));
   }
   Machine(TEXT("SM_AirConditioner01"),NE,FVector(220,180,310));
  }else if(R.Id==4||R.Id==9){
   for(const FVector P:{NW,NE,SW,SE}){
    const bool DeskPlaced=Machine(TEXT("SM_AssemblyLineDesk01"),P,FVector(290,150,100));
    if(DeskPlaced && ClearLane(P,220))Prop(TEXT("SM_RoboArm04"),P+FVector(0,0,100),.65f,0);
   }
   Fit(TEXT("SM_CeilingVentilation01"),C+FVector(0,0,408),FVector(700,200,24));
  }else if(R.Id==8||R.Id==11||R.Id==13){
   for(const FVector P:{NW,NE,SW,SE})Machine(TEXT("SM_Boiler_01"),P,FVector(250,210,300));
   for(float Side:{-1.f,1.f})Fit(TEXT("SM_Pipe_round_long"),C+FVector(Side*(HX-60),0,320),FVector(45,R.Size.Y-200,45));
  }else if(R.Id==7||R.Id==10||R.Id==16||R.Id==23){
   Machine(TEXT("SM_StorageShelves_group01"),NW,FVector(300,160,270));
   Machine(TEXT("SM_Boxes_group_06"),SE,FVector(230,220,150),15);
   Machine(TEXT("SM_ForkLift"),NE,FVector(250,150,190),90);
  }else if(R.Id==22||R.Id==15){
   Machine(TEXT("SM_Container01_01"),NW,FVector(380,210,220),0);
   Machine(TEXT("SM_AssemblyLineControl01"),SE,FVector(260,150,180),180);
   Machine(TEXT("SM_Boxes_group_06"),NE,FVector(210,210,150),15);
   // Flank cover is offset from the small exposed capture footprint.
   Machine(TEXT("SM_AssemblyLineControl01"),SW,FVector(250,150,110),90);
   for(float Side:{-1.f,1.f})Fit(TEXT("SM_MetalBeam01"),C+FVector(Side*330,0,392.5),FVector(R.Size.Y-100,65,55),FRotator(0,90,0));
   Sign(R.Id==22?TEXT("A  /  TRANSFER LOCK"):TEXT("B  /  ISOLATION LOCK"),C+FVector(0,HY-35,290),-90,R.Color,40);
  }else if(R.Id!=3&&R.Id!=18&&R.Id!=12){
   Machine(TEXT("SM_AssemblyLineControl01"),NW,FVector(240,140,120),90);
   Machine(TEXT("SM_AssemblyLineControl01"),SE,FVector(200,120,190));
   Machine(TEXT("SM_Boxes_group_06"),NE,FVector(160,160,100),10);
  }
  // Knee-height defensive cover at selected side lanes, not on every room centre.
  if(R.Id==1||R.Id==5||R.Id==14||R.Id==21)Machine(TEXT("SM_ContainerP4_01"),C+FVector(-HX*.45f,HY*.45f,0),FVector(250,120,115));
 }

 for(float Side:{-1.f,1.f}){
  Point(FVector(Side*1080,-500,270),FLinearColor(.6f,.78f,1.f),18000,1300);
  Fit(TEXT("SM_MetalBeam01#Cyan"),FVector(Side*200,0,2),FVector(8,300,3));
  Fit(TEXT("SM_MetalBeam01#Cyan"),FVector(0,Side*200,2),FVector(300,8,3));
 }
 // AEGIS reactor: diagonal corner bulkheads, suspended ring and physically supported bridge.
 for(float SX:{-1.f,1.f})for(float SY:{-1.f,1.f}){
  const FVector P(SX*1010,SY*1010,0);const FRotator Rotation(0,SX==SY?-45.f:45.f,0);
  Fit(TEXT("SM_MetalWall_02"),P+FVector(0,0,190),FVector(520,35,380),Rotation);
  Collision(P+FVector(0,0,190),FVector(520,35,380),Rotation,TEXT("GridV4Cover"));

  Point(P+FVector(-SX*170,-SY*170,800),FLinearColor(.08f,.5f,1.f),7500,650);
 }
 for(int32 I=0;I<8;++I){
  const float A=2*PI*I/8,B=2*PI*(I+1)/8;
  const FVector P(FMath::Cos(A)*820,FMath::Sin(A)*820,1080),Q(FMath::Cos(B)*820,FMath::Sin(B)*820,1080);
  Fit(TEXT("SM_MetalBeam01"),FVector(P.X,P.Y,1200),FVector(16,16,240));
  Fit(TEXT("SM_MetalBeam01"),(P+Q)*.5f,FVector(FVector::Dist(P,Q),75,90),(Q-P).Rotation());
  Fit(TEXT("SM_MetalBeam01#Cyan"),(P+Q)*.5f-FVector(0,0,48),FVector(FVector::Dist(P,Q)-30,12,8),(Q-P).Rotation());
 }
 for(float Y:{-740.f,-260.f})Fit(TEXT("SM_MetalBeam01"),FVector(1100,Y,570),FVector(1400,45,60));
 for(const FVector P:{FVector(-1050,0,0),FVector(1050,150,0)})Machine(TEXT("SM_Boiler_01"),P,FVector(190,220,330));
 for(const FVector P:{FVector(-700,850,0),FVector(700,-900,0)})Machine(TEXT("SM_ContainerP4_01"),P,FVector(280,120,115));
 Sign(TEXT("AEGIS  /  REACTOR 07"),FVector(0,-1170,1010),90,FLinearColor(.25f,.8f,1.f),54);
 Sign(TEXT("LOWER CORE PASS"),FVector(600,1175,290),-90,FLinearColor(.9f,.6f,.15f),24);
 // Compact inner containment bay. Offset doors preserve flank routes while
 // opaque baffles block cross-room fire and the canopy shields the Core above.
 Wall(FVector(-450,-1100,0),FVector(-450,-650,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(-450,-150,0),FVector(-450,250,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(-450,800,0),FVector(-450,1100,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(450,-1100,0),FVector(450,-850,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(450,-350,0),FVector(450,100,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(450,850,0),FVector(450,1100,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(-450,-950,0),FVector(450,-950,0),0,420,false,TEXT("GridV4Cover"));
 Wall(FVector(-450,950,0),FVector(450,950,0),0,420,false,TEXT("GridV4Cover"));
 Fit(TEXT("SM_FactoryFloorLarge01"),FVector(0,0,432),FVector(930,460,24));
 Collision(FVector(0,0,432),FVector(930,460,24),FRotator::ZeroRotator,TEXT("GridV4Floor"));
 for(float Side:{-1.f,1.f}) {
  Fit(TEXT("SM_MetalBeam01"),FVector(0,Side*220,409),FVector(940,40,24));
  Fit(TEXT("SM_MetalBeam01#Cyan"),FVector(0,Side*220,394),FVector(680,7,5));
 }
 CeilingLamp(FVector(0,0,0),420,0);
 Point(FVector(0,0,330),FLinearColor(.83f,.92f,1.f),12000,900,true);
 // Route-specific signs distinguish objectives by name, colour and elevation.
 Sign(TEXT("A  TRANSFER  /  LEVEL +1"),FVector(-2000,4420,285+600),-90,FLinearColor(.15f,.9f,.6f),30);
 Sign(TEXT("B  ISOLATION  /  LEVEL -1"),FVector(3600,-3950,-300),90,FLinearColor(.8f,.35f,1.f),30);
 auto Socket=[&](const TCHAR* Name,const TCHAR* Type,FVector P){auto* M=World->SpawnActor<ATargetPoint>(P,FRotator::ZeroRotator);if(M){M->Tags.Add(TEXT("GridV4FutureShift"));M->Tags.Add(FName(Name));M->Tags.Add(FName(Type));}};
 Socket(TEXT("AEGIS_WestServiceLift"),TEXT("SHIFT_LiftLanding"),FVector(-5400,-2800,-600));
 Socket(TEXT("AEGIS_EastServiceLift"),TEXT("SHIFT_LiftLanding"),FVector(5700,-2000,-600));
 Socket(TEXT("AEGIS_PowerTerminal"),TEXT("SHIFT_HackTerminal"),FVector(3700,3200,100));
 Socket(TEXT("AEGIS_LabBreachPanel"),TEXT("SHIFT_BreachableWall"),FVector(3500,-1000,180));
 for(const auto& R:GridV4::Rooms())Socket(*FString::Printf(TEXT("AEGIS_Lighting_%02d"),R.Id),TEXT("SHIFT_LightingSector"),R.Center+FVector(0,0,300));
 UE_LOG(LogVanthex,Display,TEXT("GRIDV4 AEGIS rooms=%d connections=%d vertical=%d kitGroups=%d instances=%d proxies=%d lights=%d missing=%d"),GridV4::Rooms().Num(),GridV4::Connections().Num(),GridV4::VerticalLinks().Num(),Groups.Num(),InstanceCount,CollisionCount,LightCount,MissingCount);
}
