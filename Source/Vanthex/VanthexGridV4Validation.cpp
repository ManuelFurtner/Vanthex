#include "VanthexTestArena.h"

#include "Vanthex.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexGridV4Layout.h"
#include "VanthexNavigation.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void AVanthexTestArena::ValidateGridV4()
{
 TArray<FString> Lines;
 bool Passed=true;
 int32 Checks=0;
 auto Check=[&](bool Value,const FString& Name) {
  ++Checks;
  Passed&=Value;
  Lines.Add(FString(Value?TEXT("PASS "):TEXT("FAIL "))+Name);
 };
 auto RouteLength=[](const FVector& Start,const TArray<FVector>& Route) {
  double Length=0.;
  FVector Previous=Start;
  for(const FVector& Point:Route) {Length+=FVector::Distance(Previous,Point);Previous=Point;}
  return Length;
 };
 UWorld* World=GetWorld();
 extern bool VanthexValidateGridV4Envelope(UWorld*,TArray<FString>&);
 Check(VanthexValidateGridV4Envelope(World,Lines),TEXT("outer shell contains walking and jumping capsules at all supported floor edges"));
 extern bool VanthexValidateGridV4Furniture(UWorld*,TArray<FString>&);
 Check(VanthexValidateGridV4Furniture(World,Lines),TEXT("floor-mounted kit furniture has actual support under its footprint"));
 FCollisionObjectQueryParams StaticObjects;
 StaticObjects.AddObjectTypesToQuery(ECC_WorldStatic);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(GridV4Validation),false);
 for(const FVector Eye:{FVector(-800,0,165),FVector(800,0,165),FVector(0,-1100,165),FVector(0,1100,165),FVector(0,0,1100)}) {
  FHitResult Shield;
  Check(World->LineTraceSingleByObjectType(Shield,Eye,FVector(0,0,165),StaticObjects,Params),FString::Printf(TEXT("Core containment blocks direct fire from %s"),*Eye.ToString()));
 }
 Check(AlphaSpawns.Num()==3 && BravoSpawns.Num()==3,TEXT("exactly six team spawn actors"));
 Check(Core && ExtractionA && ExtractionB,TEXT("Core and both extraction objectives exist"));
 Check(GridV4::Rooms().Num()>=18 && GridV4::Rooms().Num()<=25,FString::Printf(TEXT("%d distinct authored spaces"),GridV4::Rooms().Num()));
 Check(GridV4::VerticalLinks().Num()>=3,FString::Printf(TEXT("%d independent physical vertical links"),GridV4::VerticalLinks().Num()));
#if WITH_EDITOR
 extern bool VanthexValidateVisibleStairs(UWorld* World,TArray<FString>& Out);
 Check(VanthexValidateVisibleStairs(World,Lines),TEXT("visible stair treads ascend with the collision ramps and meet both landings"));
#endif

 int32 Nodes=0,Edges=0,Components=0;
 VanthexNavigation::GridV3GraphStats(World,Nodes,Edges,Components);
 Check(Nodes>0 && Edges>0,FString::Printf(TEXT("multilevel static graph: %d supported standing nodes, %d capsule-swept edges"),Nodes,Edges));
 Lines.Add(FString::Printf(TEXT("INFO %d geometric graph components; all required destinations are checked separately below"),Components));

 TArray<FVector> Route;
 double CoreDistances[2]={0.,0.};
 int32 CoreSamples[2]={0,0};
 const FVector CoreGoal=Core?Core->GetActorLocation():GridV4::CoreSpawn();
 for(int32 TeamIndex=0;TeamIndex<2;++TeamIndex) {
  const EVanthexTeam Team=TeamIndex==0?EVanthexTeam::Alpha:EVanthexTeam::Bravo;
  const TCHAR* TeamName=TeamIndex==0?TEXT("Alpha"):TEXT("Bravo");
  for(int32 Slot=0;Slot<3;++Slot) {
   APlayerStart* Start=GetTeamSpawn(Team,Slot);
   Check(Start!=nullptr,FString::Printf(TEXT("%s spawn %d exists"),TeamName,Slot+1));
   if(!Start) continue;
   const FVector Position=Start->GetActorLocation();
   Check(!World->OverlapAnyTestByObjectType(Position,FQuat::Identity,StaticObjects,FCollisionShape::MakeCapsule(42.f,96.f),Params),FString::Printf(TEXT("%s spawn %d actual standing capsule is clear"),TeamName,Slot+1));
   FHitResult FloorHit;
   const bool FloorFound=World->LineTraceSingleByObjectType(FloorHit,Position,Position-FVector(0,0,180),StaticObjects,Params);
   Check(FloorFound && FloorHit.ImpactNormal.Z>0.84f && FMath::Abs(FloorHit.ImpactPoint.Z)<10.f,FString::Printf(TEXT("%s spawn %d has main-level floor support"),TeamName,Slot+1));
   bool Found=VanthexNavigation::FindRoute(World,Position,CoreGoal,Route);
   Check(Found,FString::Printf(TEXT("%s spawn %d reaches Core on correct floor"),TeamName,Slot+1));
   if(Found) {
    const double Length=RouteLength(Position,Route);
    CoreDistances[TeamIndex]+=Length; ++CoreSamples[TeamIndex];
    Lines.Add(FString::Printf(TEXT("METRIC %s spawn %d -> Core: %.2f m; %.2f s at 600 cm/s walk; %.2f s at 900 cm/s sprint; %d waypoints"),TeamName,Slot+1,Length/100.,Length/600.,Length/900.,Route.Num()));
   }
   for(AVanthexExtractionZone* Zone:{ExtractionA.Get(),ExtractionB.Get()}) if(Zone) {
    Found=VanthexNavigation::FindRoute(World,Position,Zone->GetActorLocation(),Route);
    const TCHAR* ZoneName=Zone->GetId()==EVanthexExtractionId::A?TEXT("A"):TEXT("B");
    Check(Found,FString::Printf(TEXT("%s spawn %d reaches extraction %s"),TeamName,Slot+1,ZoneName));
    if(Found) {
     const double Length=RouteLength(Position,Route);
     Lines.Add(FString::Printf(TEXT("METRIC %s spawn %d -> extraction %s: %.2f m; %.2f s nominal walk"),TeamName,Slot+1,ZoneName,Length/100.,Length/600.));
    }
   }
  }
 }

 // Check every cross-team pair, rather than a single convenient central ray.
 for(int32 A=0;A<3;++A) for(int32 B=0;B<3;++B) {
  APlayerStart* Alpha=GetTeamSpawn(EVanthexTeam::Alpha,A);
  APlayerStart* Bravo=GetTeamSpawn(EVanthexTeam::Bravo,B);
  if(!Alpha || !Bravo) continue;
  FHitResult Hit;
  const bool Blocked=World->LineTraceSingleByObjectType(Hit,Alpha->GetActorLocation()+FVector(0,0,64),Bravo->GetActorLocation()+FVector(0,0,64),StaticObjects,Params);
  Check(Blocked,FString::Printf(TEXT("spawn eye-height sightline Alpha %d -> Bravo %d is blocked"),A+1,B+1));
 }

 TSet<int32> Levels;
 for(const GridV4::FRoom& Room:GridV4::Rooms()) {
  Levels.Add(Room.Level);
  const FVector Goal=Room.Center+FVector(0,0,110);
  const bool Found=VanthexNavigation::FindRoute(World,CoreGoal,Goal,Route);
  Check(Found && !Route.IsEmpty() && FMath::Abs(Route.Last().Z-Goal.Z)<20.f,FString::Printf(TEXT("Core reaches room %02d %s at floor %.0f cm"),Room.Id,Room.Name,Room.Center.Z));
  if(Found) Check(VanthexNavigation::FindRoute(World,Goal,CoreGoal,Route),FString::Printf(TEXT("room %02d has a return walking route to Core"),Room.Id));
 }
 Check(Levels.Contains(-1) && Levels.Contains(0) && Levels.Contains(1),TEXT("reachable room set covers all three authored levels"));

 // Each authored connection is tested through its bends, so an unrelated
 // alternate route cannot conceal a sealed corridor junction.
 for(const GridV4::FConnection& Connection:GridV4::Connections()) {
  bool Connected=true;
  FString Detail;
  for(int32 I=1;I<Connection.Points.Num();++I) {
   const FVector Start=Connection.Points[I-1]+FVector(0,0,110),End=Connection.Points[I]+FVector(0,0,110);
   const bool Found=VanthexNavigation::FindRoute(World,Start,End,Route);
   const double MaximumLength=FVector::Dist(Start,End)*1.8+600.;
   if(!Found || RouteLength(Start,Route)>MaximumLength) {
    Connected=false;
    Detail=FString::Printf(TEXT(" segment %d: %s"),I,Found?TEXT("route detours beyond local corridor"):TEXT("no walking route"));
    break;
   }
  }
  Check(Connected,FString::Printf(TEXT("connection %02d (%d -> %d) local bends traversable%s"),Connection.Id,Connection.From,Connection.To,*Detail));
 }

 for(const GridV4::FVerticalLink& Link:GridV4::VerticalLinks()) {
  FString Failure;
  const bool GeometryPass=VanthexNavigation::CheckWalkableSegment(World,Link.Bottom,Link.Top,&Failure);
  Check(GeometryPass,FString::Printf(TEXT("vertical link %d: continuous supported slope with full capsule clearance%s%s"),Link.Id,Failure.IsEmpty()?TEXT(""):TEXT("; "),*Failure));
  for(int32 Direction=0;Direction<2;++Direction) {
   const FVector Start=(Direction==0?Link.Bottom:Link.Top)+FVector(0,0,110);
   const FVector End=(Direction==0?Link.Top:Link.Bottom)+FVector(0,0,110);
   const bool Found=VanthexNavigation::FindRoute(World,Start,End,Route);
   const double Length=Found?RouteLength(Start,Route):0.;
   Check(Found && Length<FVector::Dist(Start,End)+600.,FString::Printf(TEXT("vertical link %d routes %s directly (%.2f m)"),Link.Id,Direction==0?TEXT("up"):TEXT("down"),Length/100.));
  }
 }

 for(AVanthexExtractionZone* Zone:{ExtractionA.Get(),ExtractionB.Get()}) if(Zone) {
  const bool Found=VanthexNavigation::FindRoute(World,CoreGoal,Zone->GetActorLocation(),Route);
  const TCHAR* Name=Zone->GetId()==EVanthexExtractionId::A?TEXT("A"):TEXT("B");
  Check(Found,FString::Printf(TEXT("Core reaches extraction %s"),Name));
  if(Found) {
   const double Length=RouteLength(CoreGoal,Route);
   Check(Length>=7500.,FString::Printf(TEXT("extraction %s requires at least 75 m walking from Core (%.2f m)"),Name,Length/100.));
   Check(FMath::Abs(Zone->GetActorLocation().Z)>500.,TEXT("extraction requires a floor change"));
   FHitResult Sight;
   Check(World->LineTraceSingleByObjectType(Sight,CoreGoal+FVector(0,0,100),Zone->GetActorLocation()+FVector(0,0,140),StaticObjects,Params),TEXT("extraction is screened from the Core sightline"));
   Lines.Add(FString::Printf(TEXT("METRIC Core -> extraction %s: %.2f m; %.2f s nominal walk plus 15.00 s extraction hold"),Name,Length/100.,Length/600.));
  }
 }
 if(CoreSamples[0]==3 && CoreSamples[1]==3) {
  const double Alpha=CoreDistances[0]/3.,Bravo=CoreDistances[1]/3.;
  const double Imbalance=(FMath::Max(Alpha,Bravo)/FMath::Max(1.,FMath::Min(Alpha,Bravo))-1.)*100.;
  Lines.Add(FString::Printf(TEXT("METRIC mean Core route: Alpha %.2f m / %.2f s, Bravo %.2f m / %.2f s; relative distance difference %.1f%%"),Alpha/100.,Alpha/600.,Bravo/100.,Bravo/600.,Imbalance));
  Check(Imbalance<=10.,TEXT("mean spawn-to-Core walking distance differs by no more than 10 percent"));
 } else Check(false,TEXT("six valid routes required for spawn-to-Core fairness measurement"));

 // A deliberate Core pass/drop well is faster than using a staircase. Measure
 // its lower landing separately so the longer walking route is not misleading.
 const FVector LowerLanding(600,800,-490);
 if(ExtractionB) {
  const bool Found=VanthexNavigation::FindRoute(World,LowerLanding,ExtractionB->GetActorLocation(),Route);
  Check(Found,TEXT("Core drop-well landing reaches lower extraction"));
  if(Found) {
   const double Length=RouteLength(LowerLanding,Route);
   Lines.Add(FString::Printf(TEXT("METRIC lower drop landing -> extraction B: %.2f m; excludes approach, jump and fall time"),Length/100.));
   Check(Length>=7000.,TEXT("even the drop shortcut leaves at least 70 m of lower-floor approach"));
  }
 }
 for(int32 Site:{22,15}) {
  int32 Entrances=0;
  for(const auto& C:GridV4::Connections()) if(C.From==Site||C.To==Site) ++Entrances;
  Check(Entrances==2,FString::Printf(TEXT("site room %d has two separately traversed approach connections"),Site));
  const auto& R=GridV4::Rooms()[Site];
  Check(FMath::Min(R.Size.X,R.Size.Y)*.5f-220.f>=300.f,TEXT("capture footprint is recessed at least 3 m from room edges"));
 }

 Lines.Add(TEXT("SCOPE Geometry queries use actual cooked world-static collision, standing capsule radius 42 cm / half-height 96 cm, sampled floor support and bidirectional routes at all three elevations."));
 Lines.Add(TEXT("LIMIT Route lengths are 150 cm graph approximations; nominal travel times exclude acceleration, player decisions, combat, interactions and congestion. No runtime FPS or human competitive balance claim is made."));
 Lines.Add(TEXT("LIMIT These checks do not prove freedom from every possible collision trap, dynamic obstruction or CharacterMovement edge case. The separate gameplay suite and rendered inspection cover additional behavior."));
 Lines.Insert(FString::Printf(TEXT("CHECKS %d"),Checks),0);
 Lines.Insert(Passed?TEXT("OVERALL PASS"):TEXT("OVERALL FAIL"),0);
 const FString Report=FString::Join(Lines,TEXT("\n"));
 const bool Saved=FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("GridV4Validation.txt")));
 UE_LOG(LogVanthex,Display,TEXT("%s"),*Report);
 FPlatformMisc::RequestExitWithStatus(false,Passed && Saved?0:1);
}
