#include "VanthexTestArena.h"
#include "VanthexGridV4Layout.h"
#include "VanthexCore.h"
#include "VanthexExtractionZone.h"
#include "GameFramework/PlayerStart.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

void AVanthexTestArena::BuildGridV4Gameplay()
{
 auto* World=GetWorld();
 FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 for(int32 Slot=0;Slot<3;++Slot) {
  AlphaSpawns.Add(World->SpawnActor<APlayerStart>(GridV4::AlphaSpawn(Slot),FRotator(0,0,0),P));
  BravoSpawns.Add(World->SpawnActor<APlayerStart>(GridV4::BravoSpawn(Slot),FRotator(0,180,0),P));
 }
 if(GetNetMode()!=NM_Client) {
  Core=World->SpawnActor<AVanthexCore>(GridV4::CoreSpawn(),FRotator::ZeroRotator,P);
  if(Core) Core->SetHome(GridV4::CoreSpawn());
 }
 ExtractionA=World->SpawnActor<AVanthexExtractionZone>(GridV4::ExtractionA(),FRotator::ZeroRotator,P);
 ExtractionB=World->SpawnActor<AVanthexExtractionZone>(GridV4::ExtractionB(),FRotator::ZeroRotator,P);
 if(ExtractionA) ExtractionA->Setup(EVanthexExtractionId::A,FVector(220,220,220));
 if(ExtractionB) ExtractionB->Setup(EVanthexExtractionId::B,FVector(220,220,220));
 // SHIFT locations are authored scenery only. This level does not enable M4.
 if(!IsRunningCommandlet()) if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/Cue_MainAmbient.Cue_MainAmbient"))) {
  auto* Audio=NewObject<UAudioComponent>(this); Audio->SetupAttachment(SceneRoot);
  Audio->SetSound(Sound); Audio->SetVolumeMultiplier(0.12f); Audio->bAllowSpatialization=false;
  Audio->RegisterComponent(); Audio->Play();
 }
 UE_LOG(LogTemp,Display,TEXT("GRID V4 AEGIS: six deployment points, neutral Core, two shared fifteen-second extraction zones"));
}
