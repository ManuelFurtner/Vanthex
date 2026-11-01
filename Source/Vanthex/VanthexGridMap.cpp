#include "VanthexTestArena.h"
#include "VanthexMapLayout.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexShiftDevice.h"
#include "Vanthex.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "VanthexNavigation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

void AVanthexTestArena::BuildGridMap()
{
 UWorld* World=GetWorld();
 CubeMesh=VanthexArt::LoadCube();
 const FLinearColor Concrete(0.30f,0.33f,0.36f), Steel(0.13f,0.17f,0.20f), Floor(0.10f,0.12f,0.14f);
 const FLinearColor Cyan(0.22f,0.80f,0.95f), Amber(1.0f,0.53f,0.16f);
 auto Box=[&](FVector Center,FVector Size,FLinearColor Color,EVanthexSurface Surface=EVanthexSurface::Concrete) {
  return SpawnBlock(Center,Size/100.f,Color,0.78f,Surface==EVanthexSurface::Concrete?0.02f:0.55f,Surface);
 };
 auto Wall=[&](FVector Center,FVector Size) { return Box(Center,Size,Concrete); };
 Box(FVector(0,0,-30),FVector(20000,24000,60),Floor,EVanthexSurface::Floor);
 // Continuous outer shell. North/south service halls and side galleries meet at crossings.
 for(float Side:{-1.f,1.f}) {
  Wall(FVector(Side*10000,0,600),FVector(80,24080,1200));
  Wall(FVector(0,Side*12000,600),FVector(20000,80,1200));
  Wall(FVector(0,Side*4500,250),FVector(2200,100,500));
  for(float X:{-2200.f,2200.f}) Wall(FVector(X,Side*2350,400),FVector(100,2300,800));
  // Staggered chest-high and standing cover keep the main hall from being a long shooting lane.
  Box(FVector(Side*900,Side*2800,110),FVector(650,180,220),Steel,EVanthexSurface::PaintedSteel);
  Box(FVector(-Side*1100,Side*1600,70),FVector(600,280,140),Steel,EVanthexSurface::PaintedSteel);
  Box(FVector(Side*3450,Side*800,100),FVector(700,220,200),Steel,EVanthexSurface::PaintedSteel);
  Box(FVector(Side*4050,-Side*3600,75),FVector(600,400,150),Steel,EVanthexSurface::PaintedSteel);
  // Side-gallery room dividers have a wide offset opening.
  Wall(FVector(Side*4200,Side*1500,250),FVector(1500,100,500));
  Wall(FVector(Side*2800,-Side*1500,250),FVector(1000,100,500));
 }
 for(float Side:{-1.f,1.f}) Wall(FVector(2200,Side*750,400),FVector(100,900,800));
 // Reactor hall: structural piers, a service gallery and a traversable cross bridge.
 for(float X:{-700.f,700.f}) for(float Y:{-700.f,700.f}) Wall(FVector(X,Y,550),FVector(120,120,1100));
 Box(FVector(-1600,0,400),FVector(500,4400,40),Steel,EVanthexSurface::BareSteel);
 Box(FVector(0,0,400),FVector(3200,420,40),Steel,EVanthexSurface::BareSteel);
 Box(FVector(1600,0,400),FVector(500,1600,40),Steel,EVanthexSurface::BareSteel);
 auto Ramp=[&](FVector Start,FVector End,float Width) {
  const FVector Delta=End-Start;
  auto* Mesh=Box((Start+End)*0.5f,FVector(Delta.Size(),Width,30),Steel,EVanthexSurface::BareSteel);
  Mesh->SetWorldRotation(Delta.Rotation());
 };
 Ramp(FVector(-1600,-3500,0),FVector(-1600,-2200,400),500);
 Ramp(FVector(-1600,3500,0),FVector(-1600,2200,400),500);
 Ramp(FVector(1600,2100,0),FVector(1600,800,400),500);
 // Rails are real collision, with openings at the ramps and bridge junction.
 for(float Y:{-1400.f,1400.f}) {
  Box(FVector(-1860,Y,480),FVector(35,1500,120),Steel,EVanthexSurface::BareSteel);
  Box(FVector(-1340,Y,480),FVector(35,1500,120),Steel,EVanthexSurface::BareSteel);
 }
 for(float Y:{-230.f,230.f}) Box(FVector(0,Y,480),FVector(2700,35,120),Steel,EVanthexSurface::BareSteel);

 // Repeated visual details use shared instanced meshes and materials, with no extra collision.
 auto* Dark=VanthexArt::MakeSurface(this,Steel,0.7f,0.55f);
 auto* Glow=VanthexArt::MakeEnergy(this,Cyan,0.7f);
 auto* Warm=VanthexArt::MakeEnergy(this,Amber,0.7f);
 auto* Panels=VanthexArt::MakeInstances(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_WallPanel")),Dark,Glow,true);
 auto* Crates=VanthexArt::MakeInstances(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_Crate")),Dark,nullptr,true);
 auto* Fixtures=VanthexArt::MakeInstances(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_Light")),Dark,Glow,false);
 auto* Pipes=VanthexArt::MakeInstances(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_Pipe")),Dark,nullptr,true);
 auto* Cabinets=VanthexArt::MakeInstances(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_Cabinet")),Dark,Glow,true);
 auto Place=[](UInstancedStaticMeshComponent* Mesh,FVector Position,FRotator Rotation,FVector Scale) { if(Mesh) Mesh->AddInstance(FTransform(Rotation,Position,Scale),true); };
 for(int32 I=-58;I<=58;++I) for(float Side:{-1.f,1.f}) Place(Panels,FVector(Side*9940,I*200,300),FRotator(0,Side<0?0:180,0),FVector(1,1,2.4f));
 for(int32 I=-48;I<=48;++I) for(float Side:{-1.f,1.f}) Place(Panels,FVector(I*200,Side*11940,300),FRotator(0,Side<0?90:-90,0),FVector(1,1,2.4f));
 for(float Side:{-1.f,1.f}) for(float Y:{-3500.f,-1700.f,0.f,1700.f,3500.f}) {
  Place(Crates,FVector(Side*4700,Y,0),FRotator(0,Side*90,0),FVector(1.3f));
  Place(Fixtures,FVector(Side*3000,Y,850),FRotator::ZeroRotator,FVector(3,1,1));
  for(float Offset:{-180.f,180.f}) VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(Side*3000+Offset,Y,950),FRotator::ZeroRotator,FVector(0.025f,0.025f,2),Dark,false);
  Place(Cabinets,FVector(Side*4800,Y,0),FRotator(0,Side<0?-90:90,0),FVector(2,2,2));
 }
 for(float Side:{-1.f,1.f}) for(int32 I=-13;I<=13;++I) for(float Z:{600.f,680.f,760.f}) Place(Pipes,FVector(Side*4720,I*400,Z),FRotator(0,90,0),FVector(4,3,3));
 // Sign posts and reactor brackets make the equipment read as installed infrastructure.
 for(const FVector Sign:{FVector(-3500,1600,390),FVector(3500,-1600,390)}) for(float Side:{-1.f,1.f}) VanthexArt::AddMesh(this,SceneRoot,CubeMesh,Sign+FVector(Side*320,0,-195),FRotator::ZeroRotator,FVector(0.12f,0.12f,3.9f),Dark,true);
 VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(0,-900,840),FRotator::ZeroRotator,FVector(15,0.2f,0.2f),Dark,true);
 for(float Side:{-1.f,1.f}) VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(Side*700,-800,840),FRotator::ZeroRotator,FVector(0.2f,2,0.2f),Dark,true);
 // Overhead service ribs and partial roofs leave illuminated skylight slots.
 for(float Y:{-5100.f,-3500.f,-1700.f,0.f,1700.f,3500.f,5100.f}) {
  VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(0,Y,1050),FRotator::ZeroRotator,FVector(98,0.8f,0.8f),Dark,true);
  VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(0,Y,1080),FRotator::ZeroRotator,FVector(44,8,0.3f),Dark,true);
 }
 // Route markings, team bays and original wayfinding.
 for(float Side:{-1.f,1.f}) {
  auto* TeamGlow=Side<0?Glow:Warm;
  for(float X:{-3500.f,3500.f}) VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(X,0,2),FRotator::ZeroRotator,FVector(0.08f,100,0.02f),TeamGlow,false);
  VanthexArt::SpawnSign(this,SceneRoot,Side<0?TEXT("ALPHA"):TEXT("BRAVO"),FVector(0,Side*4450,360),FRotator(0,Side<0?-90:90,0),650,90,Side<0?Cyan:Amber);
 }
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("GRID / REACTOR"),FVector(0,-900,750),FRotator(0,-90,0),1000,110,Cyan);
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("TRANSFER A"),FVector(-3500,1600,390),FRotator(0,-90,0),600,75,Cyan);
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("TRANSFER B"),FVector(3500,-1600,390),FRotator(0,90,0),600,75,Amber);
 VanthexArt::AddMesh(this,SceneRoot,VanthexArt::LoadKit(TEXT("SM_Ring")),FVector(0,0,800),FRotator::ZeroRotator,FVector(6,6,2),Glow,false);


 // Four outer districts: each has six two-exit maintenance rooms, cargo cover,
 // an upper service deck and a ramp independent of electrical power.
 for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) {
  const FVector Hub(SX*7000,SY*6500,0);
  for(float DX:{-1600.f,1600.f}) for(float DY:{-2600.f,0.f,2600.f}) {
   const FVector C=Hub+FVector(DX,DY==0 ? 1500.f : DY,0);
   Wall(C+FVector(0,0,210),FVector(1100,90,420));
   Wall(C+FVector(-550,350,210),FVector(90,700,420));
   Wall(C+FVector(550,-350,210),FVector(90,700,420));
   Box(C+FVector(-260,400,70),FVector(240,220,140),Steel,EVanthexSurface::PaintedSteel);
   Box(C+FVector(260,-400,110),FVector(240,220,220),Steel,EVanthexSurface::PaintedSteel);
   Box(C+FVector(0,0,450),FVector(1200,1600,40),Steel,EVanthexSurface::BareSteel);
   Place(Cabinets,C+FVector(-430,120,0),FRotator::ZeroRotator,FVector(1.5f));
  }
  // Platform north of its lift; 600 cm rise, with a 20 m accessible ramp on the east.
  Box(Hub+FVector(0,0,600),FVector(1200,1000,40),Steel,EVanthexSurface::BareSteel);
  for(float DX:{-540.f,540.f}) for(float DY:{-430.f,430.f}) Wall(Hub+FVector(DX,DY,300),FVector(50,50,600));
  for(float DX:{-280.f,280.f}) Box(Hub+FVector(DX,-510,690),FVector(20,20,180),Steel,EVanthexSurface::BareSteel);
  Ramp(Hub+FVector(2600,0,0),Hub+FVector(600,0,600),450);
  for(float DX:{-620.f,620.f}) Box(Hub+FVector(DX,310,680),FVector(35,380,120),Steel,EVanthexSurface::BareSteel);
  Box(Hub+FVector(0,520,680),FVector(1200,35,120),Steel,EVanthexSurface::BareSteel);
  for(float DX:{-230.f,230.f}) Box(Hub+FVector(DX,-710,340),FVector(35,35,680),Steel,EVanthexSurface::BareSteel);
  VanthexArt::SpawnSign(this,SceneRoot,TEXT("F / LIFT - POWER GRID"),Hub+FVector(0,-510,760),FRotator(0,-90,0),550,65,Cyan);
  const int32 Circuit=(SX>0?1:0)+(SY>0?2:0);
  // Generator bank and overhead power bus make each circuit visible in the level.
  for(float DX:{-300.f,0.f,300.f}) {
   Box(Hub+FVector(DX,1800,200),FVector(220,320,400),Steel,EVanthexSurface::BareSteel);
   VanthexArt::AddMesh(this,SceneRoot,CubeMesh,Hub+FVector(DX,1635,210),FRotator::ZeroRotator,FVector(1.5f,0.05f,2.8f),Glow,false);
  }
  for(float DY:{-500.f,500.f,1500.f}) {
   Box(Hub+FVector(-900,DY,460),FVector(40,40,920),Steel,EVanthexSurface::BareSteel);
   VanthexArt::AddMesh(this,SceneRoot,CubeMesh,Hub+FVector(-900,DY,920),FRotator::ZeroRotator,FVector(0.12f,10,0.12f),Warm,false);
  }
  for(float Facing:{-1.f}) {
   auto* Label=NewObject<UTextRenderComponent>(this); Label->SetupAttachment(SceneRoot);
   Label->SetWorldLocation(FVector(SX*6500,SY*6200+Facing*36,205)); Label->SetWorldRotation(FRotator(0,Facing*90,0));
   Label->SetText(FText::FromString(FString::Printf(TEXT("SECTOR %d / F: HACK"),Circuit+1)));
   Label->SetWorldSize(22); Label->SetHorizontalAlignment(EHTA_Center); Label->SetTextRenderColor(FColor(255,190,65)); Label->RegisterComponent();
  }
  // Cargo screens break long sightlines across the northern/southern extensions.
  if(SX<0) for(float X:{-3200.f,-1000.f,1200.f,3400.f}) {
   Box(FVector(X,SY*8500,140),FVector(800,350,280),Steel,EVanthexSurface::PaintedSteel);
   Wall(FVector(X+400,SY*9600,220),FVector(100,900,440));
  }
  for(float Y:{-10000.f,-8000.f,-6000.f,6000.f,8000.f,10000.f})
   VanthexArt::AddMesh(this,SceneRoot,CubeMesh,FVector(SX*4300,Y,3),FRotator::ZeroRotator,FVector(0.1f,10,0.02f),Glow,false);
 }
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 for(int32 Slot=0;Slot<3;++Slot) for(float Side:{-1.f,1.f}) {
  auto* Start=World->SpawnActor<APlayerStart>(FVector((Slot-1)*400,Side*5300,110),FRotator(0,Side<0?90:-90,0),Params);
  (Side<0?AlphaSpawns:BravoSpawns).Add(Start);
 }
 if(GetNetMode()!=NM_Client) {
  Core=World->SpawnActor<AVanthexCore>(FVector(0,0,50),FRotator::ZeroRotator,Params); if(Core) Core->SetHome(FVector(0,0,50));
  const FVector Devices[]={FVector(2200,0,140),FVector(-3050,300,50),FVector(1600,-920,10)};
  for(uint8 Kind=0;Kind<3;++Kind) { auto* Device=World->SpawnActor<AVanthexShiftDevice>(Devices[Kind],FRotator::ZeroRotator,Params); if(Device) { Device->DoorWidth=600; Device->LiftTravel=400; Device->Circuit=0; Device->Configure(Kind); if(Kind==0) Device->SetActorRotation(FRotator(0,90,0)); } }
 }

 for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) {
  const int32 Circuit=(SX>0?1:0)+(SY>0?2:0);
  const FVector Hub(SX*7000,SY*6500,0);
  if(GetNetMode()!=NM_Client) {
   auto* Lift=World->SpawnActor<AVanthexShiftDevice>(Hub+FVector(0,-710,10),FRotator::ZeroRotator,Params);
   Lift->DoorWidth=400; Lift->LiftTravel=600; Lift->Circuit=Circuit; Lift->Configure(2);
   auto* Terminal=World->SpawnActor<AVanthexShiftDevice>(FVector(SX*6500,SY*6200,80),FRotator::ZeroRotator,Params);
   Terminal->Circuit=Circuit; Terminal->Configure(3);
   auto* Lamp=World->SpawnActor<AVanthexShiftDevice>(Hub+FVector(-800,0,50),FRotator::ZeroRotator,Params);
   Lamp->Circuit=Circuit; Lamp->Configure(1);
  }
  for(float DY:{-2500.f,0.f,2500.f}) {
   auto* Lamp=World->SpawnActor<APointLight>(Hub+FVector(0,DY,900),FRotator::ZeroRotator,Params);
   Lamp->Tags.Add(FName(*FString::Printf(TEXT("Circuit%d"),Circuit)));
   auto* L=Cast<UPointLightComponent>(Lamp->GetLightComponent());
   L->SetMobility(EComponentMobility::Movable); L->SetIntensity(70000); L->SetAttenuationRadius(4000); L->SetCastShadows(false);
  }
 }
 ExtractionA=World->SpawnActor<AVanthexExtractionZone>(VanthexMap::ExtractionA(),FRotator::ZeroRotator,Params);
 ExtractionB=World->SpawnActor<AVanthexExtractionZone>(VanthexMap::ExtractionB(),FRotator::ZeroRotator,Params);
 if(ExtractionA) ExtractionA->Setup(EVanthexExtractionId::A,FVector(350,350,220));
 if(ExtractionB) ExtractionB->Setup(EVanthexExtractionId::B,FVector(350,350,220));
 BuildAtmosphere();
 for(const FVector Location:{FVector(0,0,1000),FVector(0,-4900,1000),FVector(0,4900,1000),FVector(-3500,2300,900),FVector(3500,-2300,900),FVector(-3500,-2300,900),FVector(3500,2300,900)}) {
  auto* Light=World->SpawnActor<APointLight>(Location,FRotator::ZeroRotator,Params);
  if(auto* Component=Cast<UPointLightComponent>(Light->GetLightComponent())) { Component->SetMobility(EComponentMobility::Movable); Component->SetIntensity(50000); Component->SetAttenuationRadius(4200); Component->SetCastShadows(false); Component->SetLightColor(FLinearColor(0.8f,0.88f,1)); }
 }
 UE_LOG(LogVanthex,Display,TEXT("GRID map built: 200 x 240 metres, four powered outer districts, five lifts, 24 maintenance hideouts"));
}

void AVanthexTestArena::ValidateGridMap()
{
 TArray<FString> Lines; bool Passed=true;
 auto Check=[&](bool Value,const FString& Name) { Passed &= Value; Lines.Add((Value?TEXT("PASS "):TEXT("FAIL "))+Name); };
 Check(AlphaSpawns.Num()==3 && BravoSpawns.Num()==3,TEXT("six team starts"));
 Check(Core && ExtractionA && ExtractionB,TEXT("core and both extraction zones"));
 TArray<FVector> Route;
 for(EVanthexTeam Team:{EVanthexTeam::Alpha,EVanthexTeam::Bravo}) for(int32 Slot=0;Slot<3;++Slot) {
  auto* Start=GetTeamSpawn(Team,Slot); if(!Start) {Check(false,TEXT("missing spawn"));continue;}
  const bool Found=VanthexNavigation::FindRoute(GetWorld(),Start->GetActorLocation(),FVector(0,0,100),Route);
  Check(Found,FString::Printf(TEXT("spawn %d/%d reaches reactor (%d waypoints)"),int32(Team),Slot,Route.Num()));
  FCollisionQueryParams Params(SCENE_QUERY_STAT(MapSpawn),false); FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic);
  Check(!GetWorld()->OverlapAnyTestByObjectType(Start->GetActorLocation(),FQuat::Identity,Objects,FCollisionShape::MakeCapsule(42,96),Params),TEXT("spawn capsule clear"));
 }
 for(auto* Zone:{ExtractionA.Get(),ExtractionB.Get()}) if(Zone) {
  Check(VanthexNavigation::FindRoute(GetWorld(),FVector(0,0,100),Zone->GetActorLocation()+FVector(0,0,80),Route),FString::Printf(TEXT("reactor reaches extraction %d"),int32(Zone->GetId())));
 }
 FCollisionQueryParams Params(SCENE_QUERY_STAT(MapCover),false); FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); FHitResult Hit;
 Check(GetWorld()->LineTraceSingleByObjectType(Hit,FVector(0,-5300,174),FVector(0,5300,174),Objects,Params),TEXT("spawn-to-spawn sightline blocked"));
 for(float Y:{-3100.f,-2700.f,2700.f,3100.f}) {
  const bool FloorHit=GetWorld()->LineTraceSingleByObjectType(Hit,FVector(-1600,Y,900),FVector(-1600,Y,-100),Objects,Params);
  Check(FloorHit && Hit.ImpactPoint.Z>50 && Hit.ImpactPoint.Z<400 && Hit.ImpactNormal.Z>0.7f,TEXT("west ramp has walkable slope"));
 }
 for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) {
  const FVector Hub(SX*7000,SY*6500,0);
  Check(VanthexNavigation::FindRoute(GetWorld(),FVector(0,0,100),Hub+FVector(0,-1100,100),Route),TEXT("outer lift landing reachable"));
  const FVector Sample=Hub+FVector(1600,0,0);
  const bool RampHit=GetWorld()->LineTraceSingleByObjectType(Hit,Sample+FVector(0,0,1000),Sample-FVector(0,0,100),Objects,Params);
  Check(RampHit && Hit.ImpactPoint.Z>250 && Hit.ImpactPoint.Z<350 && Hit.ImpactNormal.Z>0.7f,TEXT("outer emergency ramp has walkable slope"));
 }
 int32 Devices=0,Lifts=0,Terminals=0;
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) {
  ++Devices; Lifts+=It->Kind==2; Terminals+=It->Kind==3;
  const FVector Before=It->GetActorLocation(); It->ResetDevice();
  Check(It->GetActorLocation().Equals(Before,1.f),TEXT("device retains spawn on round reset"));
  if(It->Kind==3) {
   Check(VanthexNavigation::FindRoute(GetWorld(),FVector(0,0,100),It->GetActorLocation()+FVector(0,200,20),Route),TEXT("outer power terminal reachable"));
   It->SetCircuitPower(false);
   for(TActorIterator<AVanthexShiftDevice> Other(GetWorld());Other;++Other) if(Other->Circuit==It->Circuit) Check(!Other->bPowered,TEXT("circuit outage reaches linked device"));
   It->SetCircuitPower(true);
   Check(It->bPowered,TEXT("terminal restores circuit"));
  }
 }
 Check(Devices==15 && Lifts==5 && Terminals==4,TEXT("15 devices: five lifts and four power terminals"));
 if(Core) { Core->SetActorLocation(FVector(4000,0,100)); for(int32 I=0;I<4;++I) Core->Tick(0.5f); Check(Core->GetActorLocation().X>3900,TEXT("core retained beyond old arena boundary")); Core->ResetToSpawn(); }
 Lines.Insert(Passed?TEXT("OVERALL PASS"):TEXT("OVERALL FAIL"),0);
 const FString Report=FString::Join(Lines,TEXT("\n"));
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("VanthexMapTest.txt")));
 UE_LOG(LogVanthex,Display,TEXT("%s"),*Report);
 FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}


