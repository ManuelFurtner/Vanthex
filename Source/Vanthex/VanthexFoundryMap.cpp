#include "VanthexTestArena.h"
#include "VanthexShiftDevice.h"
#include "Vanthex.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "VanthexMapLayout.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"

void AVanthexTestArena::BuildFoundryMap()
{
 if(GetWorld()->GetMapName().EndsWith(TEXT("Foundry")) && !FParse::Param(FCommandLine::Get(),TEXT("BakeFoundry"))) { BuildFoundryGameplay(); return; }
 // Retain the verified gameplay foundation and replace its presentation. New
 // collidable workshop/cargo layouts below turn the wings into distinct routes.
 BuildGridMap();
 UWorld* World=GetWorld();
 UMaterialInterface* Concrete=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/MI_ConcreteWall01.MI_ConcreteWall01"));
 UMaterialInterface* Metal=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/MI_Column_Painted_01.MI_Column_Painted_01"));
 UMaterialInterface* Floor=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/MI_ConcreteFloor_02.MI_ConcreteFloor_02"));
 TArray<UStaticMeshComponent*> Existing; GetComponents(Existing);
 for(auto* Part:Existing) {
  if(Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision) Part->SetVisibility(false);
  else {
   const FVector Size=Part->Bounds.BoxExtent*2;
   Part->SetMaterial(0,Size.Z<80 ? Floor : Size.Z>300 ? Concrete : Metal);
   // Texture-rich factory modules replace visible blockout cover and floor.
   if(Size.X>19000 || (Size.Z>100 && Size.Z<300 && Size.X>200 && Size.Y>100)) Part->SetVisibility(false);
  }
 }
 TMap<FString,UInstancedStaticMeshComponent*> Groups;
 int32 Instances=0,Missing=0;
 auto MeshPath=[](const FString& Name) {
  const FString Sub=Name==TEXT("SM_Crane01")||Name==TEXT("SM_CraneCabin01") ? TEXT("Crane/") : Name==TEXT("SM_RoboArm04") ? TEXT("Robot/") : TEXT("");
  return TEXT("/Game/Meshes/")+Sub+Name+TEXT(".")+Name;
 };
 auto GetGroup=[&](const FString& Name)->UInstancedStaticMeshComponent* {
  if(auto** Found=Groups.Find(Name)) return *Found;
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*MeshPath(Name));
  if(!Mesh) { ++Missing; UE_LOG(LogVanthex,Error,TEXT("Missing factory mesh %s"),*Name); return nullptr; }
  auto* Group=NewObject<UInstancedStaticMeshComponent>(this,FName(*FString::Printf(TEXT("Foundry_%s"),*Name)));
  Group->SetStaticMesh(Mesh); Group->SetupAttachment(SceneRoot); Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Group->SetCastShadow(true); Group->ComponentTags.Add(TEXT("FactoryKit")); Group->RegisterComponent(); Groups.Add(Name,Group); return Group;
 };
 // Placement is in centimetres; offsets are corrected for each kit mesh pivot.
 auto Fit=[&](const FString& Name,FVector Center,FVector Size,float Yaw=0.f) {
  auto* G=GetGroup(Name); if(!G) return;
  const FBox B=G->GetStaticMesh()->GetBoundingBox(); const FVector Native=B.GetSize();
  const FVector Scale(Size.X/FMath::Max(1.,Native.X),Size.Y/FMath::Max(1.,Native.Y),Size.Z/FMath::Max(1.,Native.Z));
  const FRotator R(0,Yaw,0); G->AddInstance(FTransform(R,Center-R.RotateVector(B.GetCenter()*Scale),Scale),true); ++Instances;
 };
 auto Prop=[&](const FString& Name,FVector Ground,float Scale=1.f,float Yaw=0.f,bool Solid=false) {
  auto* G=GetGroup(Name); if(!G) return;
  const FBox B=G->GetStaticMesh()->GetBoundingBox(); const FRotator R(0,Yaw,0);
  const FVector Offset(B.GetCenter().X,B.GetCenter().Y,B.Min.Z);
  G->AddInstance(FTransform(R,Ground-R.RotateVector(Offset*Scale),FVector(Scale)),true); ++Instances;
  if(Solid) {
   auto* Proxy=SpawnBlock(Ground+FVector(0,0,B.GetSize().Z*Scale*0.5f),B.GetSize()*Scale/100.f,FLinearColor::Gray,0.8f,0.2f,EVanthexSurface::BareSteel);
   Proxy->SetWorldRotation(R); Proxy->SetVisibility(false); Proxy->ComponentTags.Add(TEXT("FoundryCover"));
  }
 };
 const FLinearColor Cyan(0.22f,0.66f,1.f),Amber(1.f,0.42f,0.12f);
 auto Point=[&](FVector P,FLinearColor Color,float Intensity,float Radius,int32 Circuit=-1) {
  auto* Actor=World->SpawnActor<APointLight>(P,FRotator::ZeroRotator);
  auto* L=Cast<UPointLightComponent>(Actor->GetLightComponent()); L->SetMobility(EComponentMobility::Movable);
  L->SetLightColor(Color); L->SetIntensity(Intensity); L->SetAttenuationRadius(Radius); L->SetCastShadows(false);
  if(Circuit>=0) Actor->Tags.Add(FName(*FString::Printf(TEXT("Circuit%d"),Circuit)));
 };
 // Full-sized factory floor modules retain their authored worn concrete UVs.
 for(int32 X=-6;X<=6;++X) for(int32 Y=-7;Y<=7;++Y)
  Fit(TEXT("SM_FactoryFloorLarge01"),FVector(X*1600,Y*1600,-3),FVector(1600,1600,10));
 // Tile architectural pieces at their authored scale instead of stretching UVs
 // across whole walls. The original solid collision stays behind the cladding.
 for(auto* Part:Existing) {
  if(Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision || !Part->IsVisible() || !Part->GetComponentRotation().Equals(FRotator::ZeroRotator,0.1f)) continue;
  const FVector Size=Part->Bounds.BoxExtent*2,Center=Part->Bounds.Origin;
  const bool AlongX=Size.X>Size.Y; const float Length=AlongX?Size.X:Size.Y,Thickness=AlongX?Size.Y:Size.X;
  if(Size.Z<300 || Thickness>160 || Length<300 || Length>15000) continue;
  Part->SetVisibility(false);
  for(float H=0;H<Size.Z;H+=200) for(float W=0;W<Length;W+=400) {
   const float Span=FMath::Min(400.f,Length-W),Height=FMath::Min(200.f,float(Size.Z)-H);
   const FVector Offset=AlongX?FVector(-Length/2+W+Span/2,0,-Size.Z/2+H+Height/2):FVector(0,-Length/2+W+Span/2,-Size.Z/2+H+Height/2);
   Fit(TEXT("SM_ConcreteWall"),Center+Offset,FVector(Span,Thickness+2,Height),AlongX?0:90);
  }
 }
 for(float X:{-2500.f,2500.f}) for(int32 Y=-16;Y<=16;++Y) Prop(TEXT("SM_PedestrianPath01"),FVector(X,Y*300,3),1.f);
 // A monumental central nave: steel columns, roof trusses, glazing and cranes.
 for(float Y:{-5200.f,-3600.f,-2000.f,0.f,2000.f,3600.f,5200.f}) {
  for(float X:{-4300.f,4300.f}) {
   Fit(TEXT("SM_Column_02"),FVector(X,Y,950),FVector(120,180,1900));
   auto* P=SpawnBlock(FVector(X,Y,950),FVector(1.2f,1.8f,19),FLinearColor::Gray,0.8f,0.2f,EVanthexSurface::BareSteel); P->SetVisibility(false);
  }
  Fit(TEXT("SM_MetalBeam01"),FVector(0,Y,1840),FVector(8700,90,110));
  for(float X:{-2700.f,2700.f}) Fit(TEXT("SM_LargeRoofMerged01"),FVector(X,Y,2080),FVector(3200,1500,600));
  for(float X:{-3100.f,3100.f}) {
   Prop(TEXT("SM_LampSet_01"),FVector(X,Y,1650),3.f);
   Point(FVector(X,Y,1400),Y<0?Cyan:Amber,70000,2700);
  }
 }
 for(float X:{-1100.f,1100.f}) Fit(TEXT("SM_MetalBeam01"),FVector(X,0,1800),FVector(12000,100,130),90);
 for(float Y:{-4500.f,-3000.f,-1500.f,0.f,1500.f,3000.f,4500.f}) Point(FVector(0,Y,750),FLinearColor(0.65f,0.78f,1.f),22000,1900);
 for(float X:{-4450.f,4450.f}) for(float Y:{-4400.f,-2800.f,2800.f,4400.f}) {
  Fit(TEXT("SM_LargeWindowFramed"),FVector(X,Y,1330),FVector(1500,35,900),90);
 }
 for(float Y:{-2200.f,2200.f}) {
  Prop(TEXT("SM_Crane01"),FVector(0,Y,1350),1.5f,90);
  // Parked crane trolley stays above every traversable bridge.
 }
 // Hero reactor machinery, clear central access and elevated flanking bridge.
 for(float Side:{-1.f,1.f}) {
  Prop(TEXT("SM_Boiler_01"),FVector(Side*1050,750,0),1.05f,Side*90,true);
  Prop(TEXT("SM_Pipes_round_section1"),FVector(Side*1900,1000,500),1.2f,90);
  Prop(TEXT("SM_IndustrialRobot01_01"),FVector(Side*950,-700,0),0.9f,Side*90,true);
  Point(FVector(Side*800,100,380),Side<0?Cyan:Amber,13000,1500);
 }
 // Replace every hidden low cover block with a material-rich cargo asset.
 for(auto* Part:Existing) if(Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision && !Part->IsVisible()) {
  const FVector Size=Part->Bounds.BoxExtent*2;
  if(Size.Z>100 && Size.Z<300 && Size.X>200 && Size.Y>100)
   Fit(TEXT("SM_Boxes_group_01"),Part->Bounds.Origin,Size);
 }
 // Two production halls with robot cells and offset conveyor cover.
 for(float Side:{-1.f,1.f}) for(int32 Row=0;Row<5;++Row) {
  const float Y=-3600+Row*1800;
  const FVector Cell(Side*3200,Y,0);
  Prop(TEXT("SM_AssemblyLine01"),Cell,1.4f,90,true);
  Prop(TEXT("SM_RoboArm04"),Cell+FVector(Side*380,180,0),1.3f,Side<0?0:180,true);
  Prop(TEXT("SM_AssemblyLineControl01"),Cell+FVector(Side*400,-270,0),1.f,Side<0?0:180);
  Prop(TEXT("SM_Boxes_group_06"),Cell+FVector(-Side*350,260,0),1.5f,Row*90,true);
 }
 // Four wings: freight storage, utilities, robot assembly, and dispatch.
 for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) {
  const int32 Circuit=(SX>0?1:0)+(SY>0?2:0); const FVector Hub(SX*7000,SY*6500,0);
  for(float DX:{-1600.f,1600.f}) for(float DY:{-2600.f,1500.f,2600.f}) {
   const FVector C=Hub+FVector(DX,DY,0);
   for(float Offset:{-300.f,300.f}) Prop(TEXT("SM_StorageShelves_group03"),C+FVector(Offset,260,0),0.55f,0);
   Prop(TEXT("SM_Barrels01_Merged"),C+FVector(100,-380,0),0.55f,90);
   Fit(TEXT("SM_MetalWall_02"),C+FVector(0,0,210),FVector(1100,95,420));
   Point(C+FVector(0,-250,350),Circuit%2?Amber:Cyan,12000,1100,Circuit);
  }
  // New cargo pockets between the outer perimeter and lift hubs, with two exits.
  for(float DY:{-1800.f,900.f}) {
   Prop(TEXT("SM_Container01_01"),Hub+FVector(-600,DY,0),1.f,90,true);
   Prop(TEXT("SM_StorageShelves_group01"),Hub+FVector(800,DY+250,0),0.8f,90,true);
   Prop(TEXT("SM_Boxes_group_01"),Hub+FVector(600,DY-250,0),1.f,0,true);
  }
  Prop(TEXT("SM_ForkLift"),Hub+FVector(-1000,-2100,0),1.f,SY<0?40:220,true);
  Prop(TEXT("SM_DeskControl_01"),Hub+FVector(-270,180,625),1.f,90);
  Prop(TEXT("SM_ElectricalPanel_01"),FVector(SX*6500+125,SY*6200,0),1.f,180);
  Prop(TEXT("SM_Pipes_round_section1"),Hub+FVector(0,1850,0),1.2f);
  for(float DX:{-300.f,300.f}) Prop(TEXT("SM_Boiler_01"),Hub+FVector(DX,1900,0),0.75f,180);
  // Real grating on the lift decks and platform edge trim.
  Fit(TEXT("SM_Platform_01"),Hub+FVector(0,0,607),FVector(1200,1000,25));
  for(float DY:{-2800.f,0.f,2800.f}) {
   Fit(TEXT("SM_MetalBeam01"),Hub+FVector(0,DY,1250),FVector(3900,70,85));
   Prop(TEXT("SM_LampSet_01"),Hub+FVector(0,DY,1220),3.f);
   Point(Hub+FVector(0,DY,1000),Circuit%2?Amber:Cyan,50000,2600,Circuit);
  }
  const TCHAR* Names[]={TEXT("01 / FREIGHT"),TEXT("02 / UTILITIES"),TEXT("03 / ASSEMBLY"),TEXT("04 / DISPATCH")};
  VanthexArt::SpawnSign(this,SceneRoot,Names[Circuit],Hub+FVector(0,-1000,800),FRotator(0,-90,0),600,50,Circuit%2?Amber:Cyan);
 }
 // Rear rail yard and loading bays create a new long flank with interrupted views.
 for(float Side:{-1.f,1.f}) {
  Prop(TEXT("SM_Train01"),FVector(-2500,Side*10500,0),1.f,0,true);
  Prop(TEXT("SM_TrainCar01"),FVector(1500,Side*10500,0),1.f,0,true);
  for(float X:{-3000.f,-1000.f,1000.f,3000.f}) {
   Prop(TEXT("SM_ContainerP4_01"),FVector(X,Side*7300,0),0.8f,90,true);
   Point(FVector(X,Side*7500,650),Amber,16000,1800);
  }
  Prop(TEXT("SM_Background2_BoxBuilding"),FVector(Side*12500,0,0),3.f);
  Prop(TEXT("SM_Background1_Tower01"),FVector(Side*11000,9000,0),1.f);
  Prop(TEXT("SM_Background2_Hangar"),FVector(0,Side*16500,-50),0.6f,90);
 }
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("VANTHEX / FOUNDRY"),FVector(0,-4450,850),FRotator(0,-90,0),1000,95,Amber);
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("CORE / REACTOR 07"),FVector(0,-950,900),FRotator(0,-90,0),900,65,Cyan);
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("TRANSFER A"),FVector(-3500,1600,390),FRotator(0,-90,0),600,55,Cyan);
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("TRANSFER B"),FVector(3500,-1600,390),FRotator(0,90,0),600,55,Amber);
 // Twilight outside, pooled warm working lights inside. Emergency paths stay readable.
 for(TActorIterator<ADirectionalLight> It(World);It;++It) {
  It->SetActorRotation(FRotator(-12,-35,0)); auto* L=It->GetLightComponent(); L->SetIntensity(2.2f); L->SetLightColor(FLinearColor(0.5f,0.65f,1.f));
 }
 for(TActorIterator<ASkyLight> It(World);It;++It) It->GetLightComponent()->SetIntensity(1.f);
 for(TActorIterator<AExponentialHeightFog> It(World);It;++It) {
  auto* F=It->GetComponent(); F->SetFogDensity(0.012f); F->SetFogMaxOpacity(0.6f); F->SetVolumetricFog(true);
  F->SetFogInscatteringColor(FLinearColor(0.12f,0.18f,0.25f));
 }
 for(TActorIterator<APostProcessVolume> It(World);It;++It) {
  auto& S=It->Settings; S.AutoExposureMinBrightness=0.5f; S.AutoExposureMaxBrightness=0.5f; S.AutoExposureBias=0.3f;
  S.BloomIntensity=0.3f; S.ColorSaturation=FVector4(0.94f,0.94f,0.94f,1); S.VignetteIntensity=0.18f;
 }
 if(!IsRunningCommandlet()) if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Cue_MainAmbient.Cue_MainAmbient"))) {
  auto* Audio=NewObject<UAudioComponent>(this); Audio->SetupAttachment(SceneRoot); Audio->SetSound(Sound); Audio->SetVolumeMultiplier(0.22f); Audio->bAllowSpatialization=false; Audio->RegisterComponent(); Audio->Play();
 }
 UE_LOG(LogVanthex,Display,TEXT("FOUNDRY built: %d kit mesh families, %d instances, %d missing assets"),Groups.Num(),Instances,Missing);
}

void AVanthexTestArena::BuildFoundryGameplay()
{
 auto* World=GetWorld(); FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 for(int32 Slot=0;Slot<3;++Slot) for(float Side:{-1.f,1.f}) {
  auto* Start=World->SpawnActor<APlayerStart>(FVector((Slot-1)*400,Side*5300,110),FRotator(0,Side<0?90:-90,0),P);
  (Side<0?AlphaSpawns:BravoSpawns).Add(Start);
 }
 auto Device=[&](FVector Position,uint8 Kind,int32 Circuit,float Width,float Travel,float Yaw=0.f) {
  if(GetNetMode()==NM_Client) return;
  auto* D=World->SpawnActor<AVanthexShiftDevice>(Position,FRotator(0,Yaw,0),P);
  D->Circuit=Circuit; D->DoorWidth=Width; D->LiftTravel=Travel; D->Configure(Kind);
 };
 if(GetNetMode()!=NM_Client) { Core=World->SpawnActor<AVanthexCore>(FVector(0,0,50),FRotator::ZeroRotator,P); Core->SetHome(FVector(0,0,50)); }
 Device(FVector(2200,0,140),0,0,600,400,90); Device(FVector(-3050,300,50),1,0,600,400); Device(FVector(1600,-920,10),2,0,600,400);
 for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) {
  int32 Circuit=(SX>0?1:0)+(SY>0?2:0); const FVector Hub(SX*7000,SY*6500,0);
  Device(Hub+FVector(0,-710,10),2,Circuit,400,600); Device(FVector(SX*6500,SY*6200,80),3,Circuit,300,220); Device(Hub+FVector(-800,0,50),1,Circuit,300,220);
 }
 ExtractionA=World->SpawnActor<AVanthexExtractionZone>(VanthexMap::ExtractionA(),FRotator::ZeroRotator,P);
 ExtractionB=World->SpawnActor<AVanthexExtractionZone>(VanthexMap::ExtractionB(),FRotator::ZeroRotator,P);
 ExtractionA->Setup(EVanthexExtractionId::A,FVector(350,350,220)); ExtractionB->Setup(EVanthexExtractionId::B,FVector(350,350,220));
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("VANTHEX / FOUNDRY"),FVector(0,-4450,850),FRotator(0,-90,0),1000,95,FLinearColor(1,0.42f,0.12f));
 VanthexArt::SpawnSign(this,SceneRoot,TEXT("CORE / REACTOR 07"),FVector(0,-950,900),FRotator(0,-90,0),900,65,FLinearColor(0.22f,0.66f,1));
 if(!IsRunningCommandlet()) if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Cue_MainAmbient.Cue_MainAmbient"))) {
  auto* Audio=NewObject<UAudioComponent>(this); Audio->SetupAttachment(SceneRoot); Audio->SetSound(Sound); Audio->SetVolumeMultiplier(0.22f); Audio->bAllowSpatialization=false; Audio->RegisterComponent(); Audio->Play();
 }
 UE_LOG(LogVanthex,Display,TEXT("FOUNDRY authored level loaded; gameplay objectives and 15 devices initialized"));
}
