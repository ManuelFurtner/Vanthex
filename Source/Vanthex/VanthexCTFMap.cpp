#include "VanthexTestArena.h"
#include "VanthexFlag.h"
#include "VanthexMapLayout.h"
#include "VanthexArt.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/TextRenderActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerStart.h"

void AVanthexTestArena::BuildCTF()
{
 CubeMesh=VanthexArt::LoadCube();
 auto* Cylinder=VanthexArt::LoadCylinder();
 auto* Sphere=VanthexArt::LoadSphere();
 const FLinearColor Stone(.64f,.53f,.36f),Marble(.79f,.72f,.57f),DarkStone(.35f,.28f,.18f);
 auto Material=[&](FLinearColor Color,float Panel=5000.f) {
  auto* M=VanthexArt::MakeSurface(this,Color,.9f,0,FLinearColor::Black,0,EVanthexSurface::Concrete);
  if(M){M->SetScalarParameterValue(TEXT("Panel"),Panel);M->SetScalarParameterValue(TEXT("Relief"),.12f);M->SetScalarParameterValue(TEXT("Wear"),.65f);}
  return M;
 };
 auto* StoneM=Material(Stone);auto* MarbleM=Material(Marble);auto* DarkM=Material(DarkStone);
 auto* TerracottaM=Material(FLinearColor(.43f,.16f,.075f));
 auto* BronzeM=VanthexArt::MakeSurface(this,FLinearColor(.31f,.22f,.065f),.55f,.55f);
 int32 PartId=0;
 auto Mesh=[&](const TCHAR* Name,UStaticMesh* Shape,FVector P,FVector Size,UMaterialInterface* M,bool Collision=true,FRotator Rotation=FRotator::ZeroRotator) {
  auto* Part=NewObject<UStaticMeshComponent>(this,FName(*FString::Printf(TEXT("Roman_%s_%04d"),Name,PartId++)));
  Part->SetStaticMesh(Shape);Part->SetupAttachment(SceneRoot);Part->SetWorldLocationAndRotation(P,Rotation);Part->SetWorldScale3D(Size/100);
  Part->SetMaterial(0,M);Part->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));Part->SetCastShadow(true);
  Part->ComponentTags.Add(TEXT("CTF_Roman_Architecture"));Part->ComponentTags.Add(Collision?TEXT("Concrete"):TEXT("RomanTrim"));Part->RegisterComponent();return Part;
 };
 auto Box=[&](FVector P,FVector Size,UMaterialInterface* M,bool Collision=true){return Mesh(TEXT("Stone"),CubeMesh,P,Size,M,Collision);};
 // One continuous collision floor. Thin instanced pavers provide real joints without tripping characters.
 Box(FVector(0,0,-20),FVector(5200,8800,40),DarkM);
 FRandomStream Random(4717);
 TArray<UInstancedStaticMeshComponent*> Paving;
 for(int32 I=0;I<6;++I){auto* M=Material(FLinearColor(.48f+I*.025f,.42f+I*.022f,.31f+I*.018f));auto* C=VanthexArt::MakeInstances(this,SceneRoot,CubeMesh,M,nullptr,false);C->ComponentTags.Add(TEXT("RomanPaving"));Paving.Add(C);}
 for(int32 Y=0;Y<55;++Y)for(int32 X=0;X<32;++X) {
  const float PX=-2520+X*160+(Y%2?40:0),PY=-4320+Y*160;
  Paving[Random.RandRange(0,5)]->AddInstance(FTransform(FRotator::ZeroRotator,FVector(PX,PY,1),FVector(1.56f,1.56f,.02f)),true);
 }
 auto Column=[&](FVector P,float Height=480.f,float Radius=52.f,bool Collision=true) {
  Mesh(TEXT("ColumnBase"),Cylinder,P+FVector(0,0,12),FVector(Radius*2.7f,Radius*2.7f,24),MarbleM,Collision);
  Mesh(TEXT("ColumnShaft"),Cylinder,P+FVector(0,0,Height*.5f),FVector(Radius*2,Radius*2,Height-40),MarbleM,Collision);
  Mesh(TEXT("Capital"),Cylinder,P+FVector(0,0,Height-30),FVector(Radius*2.4f,Radius*2.4f,20),MarbleM,Collision);
  Box(P+FVector(0,0,Height-8),FVector(Radius*2.8f,Radius*2.8f,22),MarbleM,Collision);
 };
 auto Arch=[&](FVector P,float Radius,float Spring=270.f,bool AlongX=true) {
  const FVector Axis=AlongX?FVector(1,0,0):FVector(0,1,0);
  for(float Sign:{-1.f,1.f})Column(P+Axis*Radius*Sign,Spring,44);
  constexpr int32 Segments=17;
  for(int32 I=0;I<Segments;++I){const float A=PI*(I+.5f)/Segments;
   const FVector Pos=P+Axis*(FMath::Cos(A)*Radius)+FVector(0,0,Spring+FMath::Sin(A)*Radius);
   const FVector Size=AlongX?FVector(Radius*PI/Segments+8,100,58):FVector(100,Radius*PI/Segments+8,58);
   const FRotator R=AlongX?FRotator(FMath::RadiansToDegrees(A)-90,0,0):FRotator(0,0,90-FMath::RadiansToDegrees(A));
   Mesh(TEXT("ArchStone"),CubeMesh,Pos,Size,I==Segments/2?MarbleM:StoneM,true,R);
  }
 };
 auto Masonry=[&](FVector P,FVector Size) {
  Box(P,Size,StoneM);
  // Projecting cornices and stone courses break up the monolithic silhouette.
  Box(P+FVector(0,0,Size.Z*.5f+10),FVector(Size.X+24,Size.Y+24,26),MarbleM,false);
  for(float Z=-Size.Z*.5f+90;Z<Size.Z*.5f-20;Z+=95)Box(P+FVector(0,0,Z),FVector(Size.X+3,Size.Y+3,4),DarkM,false);
 };
 for(float Side:{-1.f,1.f}) {
  Masonry(FVector(Side*2600,0,350),FVector(80,8800,700));
  Masonry(FVector(0,Side*4400,350),FVector(5200,80,700));
  auto* TeamM=Material(Side>0?FLinearColor(.4f,.085f,.04f):FLinearColor(.075f,.19f,.28f));
  Box(FVector(0,Side*3600,3),FVector(1100,1100,6),DarkM,false);
  // Mosaic border uses muted Roman pigments; flags keep clear team identification.
  for(float Edge:{-1.f,1.f}) {
   Box(FVector(Edge*535,Side*3600,7),FVector(26,1100,5),TeamM,false);
   Box(FVector(0,Side*3600+Edge*535,7),FVector(1100,26,5),TeamM,false);
  }
  for(float X:{-1100.f,1100.f})Masonry(FVector(X,Side*2900,200),FVector(800,100,400));
  for(float X:{-850.f,850.f})Masonry(FVector(X,Side*3700,150),FVector(140,1000,300));
  Masonry(FVector(0,Side*3980,90),FVector(500,120,180));
  Arch(FVector(0,Side*2900,0),610,260);
  for(float X:{-1850.f,1850.f})Arch(FVector(X,Side*2900,0),365,260);
  for(float X:{-1.f,1.f}) {
   Masonry(FVector(X*1150,Side*1550,180),FVector(100,2100,360));
   Masonry(FVector(X*2070,Side*1000,100),FVector(500,160,200));
   Masonry(FVector(X*1720,Side*2050,70),FVector(250,220,140));
   Masonry(FVector(X*650,Side*1650,100),FVector(500,180,200));
  }
  Masonry(FVector(0,Side*650,105),FVector(500,150,210));
  // Temple portico behind each spawn: columns, entablature and a terracotta gable.
  for(float X:{-650.f,-215.f,215.f,650.f})Column(FVector(X,Side*4300,0),570,48);
  Box(FVector(0,Side*4300,590),FVector(1560,240,75),MarbleM,false);
  for(float X:{-1.f,1.f})Mesh(TEXT("Pediment"),CubeMesh,FVector(X*365,Side*4300,745),FVector(830,240,55),TerracottaM,false,FRotator(X*-20,0,0));
  for(int32 I=-6;I<=6;++I)Box(FVector(I*105,Side*4300,640+(6-FMath::Abs(I))*35),FVector(105,120,(6-FMath::Abs(I))*70+30),StoneM,false);
  // Small freestanding votive urns stay outside the three approach paths.
  for(float X:{-2350.f,2350.f}) {
   Box(FVector(X,Side*3500,45),FVector(140,140,90),StoneM);
   Mesh(TEXT("Urn"),Sphere,FVector(X,Side*3500,135),FVector(95,95,115),TerracottaM,false);
   Mesh(TEXT("UrnNeck"),Cylinder,FVector(X,Side*3500,195),FVector(48,48,30),TerracottaM,false);
  }
 }
 // Victory column is the centre landmark and solid cover.
 Masonry(FVector(0,0,140),FVector(220,220,280));Column(FVector(0,0,280),470,60);
 Mesh(TEXT("VictoryOrb"),Sphere,FVector(0,0,790),FVector(90,90,90),BronzeM,false);
 for(float X:{-550.f,550.f})for(float Y:{-320.f,320.f})Masonry(FVector(X,Y,70),FVector(220,180,140));
 for(float X:{-2300.f,2300.f})Masonry(FVector(X,0,80),FVector(200,400,160));
 // Boundary colonnades create a Roman courtyard silhouette and long sunlight shadows.
 for(float Side:{-1.f,1.f}) {
  for(float Y=-4000;Y<=4000;Y+=800) {
   Column(FVector(Side*2500,Y,0),620,48);
   Box(FVector(Side*2500,Y,640),FVector(200,810,55),MarbleM,false);
   Mesh(TEXT("RoofTile"),CubeMesh,FVector(Side*2500,Y,685),FVector(260,810,35),TerracottaM,false);
  }
  // Cypress silhouettes beyond the enclosed, playable courtyard.
  auto* Leaves=Material(FLinearColor(.075f,.12f,.045f));
  for(float Y:{-3200.f,-1200.f,1200.f,3200.f}) {
   Mesh(TEXT("Cypress"),Sphere,FVector(Side*3100,Y,650),FVector(230,230,1000),Leaves,false);
   Mesh(TEXT("TreeTrunk"),Cylinder,FVector(Side*3100,Y,160),FVector(35,35,320),DarkM,false);
  }
 }
 auto Label=[&](FString Value,FVector P,float Yaw,float Size) {
  auto* Sign=GetWorld()->SpawnActor<ATextRenderActor>(P,FRotator(0,Yaw,0));
  if(Sign){auto* T=Sign->GetTextRender();T->SetText(FText::FromString(Value));T->SetTextRenderColor(FColor(68,48,29));T->SetHorizontalAlignment(EHTA_Center);T->SetWorldSize(Size);}
 };
 Label(TEXT("CASTRA ALPHA"),FVector(0,-4168,585),90,62);
 Label(TEXT("CASTRA BRAVO"),FVector(0,4168,585),-90,62);
 Label(TEXT("FORVM"),FVector(0,-113,180),-90,36);
 BuildCTFAtmosphere();
}

void AVanthexTestArena::BuildCTFGameplay()
{
 if(GetNetMode()==NM_Client)return;
 FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 for(auto Team:{EVanthexTeam::Alpha,EVanthexTeam::Bravo}) {
  const float Sign=Team==EVanthexTeam::Bravo?1.f:-1.f;
  for(int32 I=0;I<5;++I) {
   auto* Start=GetWorld()->SpawnActor<APlayerStart>(FVector((I-2)*250,Sign*4150,100),FRotator(0,Sign>0?-90:90,0),P);
   (Team==EVanthexTeam::Bravo?BravoSpawns:AlphaSpawns).Add(Start);
  }
  auto* Flag=GetWorld()->SpawnActor<AVanthexFlag>(AVanthexFlag::StaticClass(),FTransform::Identity,P);Flag->Initialize(Team);
 }
}
