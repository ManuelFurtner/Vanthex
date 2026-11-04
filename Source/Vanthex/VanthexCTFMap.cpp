#include "VanthexTestArena.h"
#include "VanthexFlag.h"
#include "VanthexMapLayout.h"
#include "VanthexArt.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerStart.h"
void AVanthexTestArena::BuildCTF()
{
 CubeMesh=VanthexArt::LoadCube();
 const FLinearColor Concrete(.2f,.23f,.27f),Steel(.07f,.1f,.13f),Floor(.11f,.13f,.15f);
 auto Box=[&](FVector P,FVector Size,FLinearColor Color){return SpawnBlock(P,Size/100,Color,.75f,.15f,EVanthexSurface::Concrete);};
 Box(FVector(0,0,-20),FVector(5200,8800,40),Floor);
 for(float Side:{-1.f,1.f}) {
  Box(FVector(Side*2600,0,250),FVector(80,8800,500),Concrete);
  Box(FVector(0,Side*4400,250),FVector(5200,80,500),Concrete);
  const FLinearColor Team=Side>0?FLinearColor(.8f,.22f,.04f):FLinearColor(.03f,.35f,.65f);
  Box(FVector(0,Side*3600,3),FVector(1100,1100,6),Team);
  // Three defended gates from each base; no direct base-to-base sightline.
  for(float X:{-1100.f,1100.f})Box(FVector(X,Side*2900,200),FVector(800,100,400),Concrete);
  for(float X:{-850.f,850.f})Box(FVector(X,Side*3700,150),FVector(140,1000,300),Concrete);
  Box(FVector(0,Side*3980,90),FVector(500,120,180),Steel);
  for(float X:{-1.f,1.f}) {
   // Solid lane separators leave crosslinks at the centre and base entries.
   Box(FVector(X*1150,Side*1550,180),FVector(100,2100,360),Concrete);
   Box(FVector(X*2070,Side*1000,100),FVector(500,160,200),Team);
   Box(FVector(X*1720,Side*2050,70),FVector(250,220,140),Steel);
   Box(FVector(X*650,Side*1650,100),FVector(500,180,200),Steel);
  }
  Box(FVector(0,Side*650,105),FVector(500,150,210),Concrete);
 }
 // Central combat square: four cover islands and a tall central landmark.
 Box(FVector(0,0,200),FVector(220,220,400),Steel);
 for(float X:{-550.f,550.f})for(float Y:{-320.f,320.f})Box(FVector(X,Y,70),FVector(220,180,140),Concrete);
 for(float X:{-2300.f,2300.f})Box(FVector(X,0,80),FVector(200,400,160),Steel);
 BuildAtmosphere();
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
