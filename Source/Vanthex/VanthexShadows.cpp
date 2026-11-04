#include "VanthexShadows.h"
#include "VanthexGridV4Layout.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void VanthexShadows::ConfigureKeyLight(UPointLightComponent* Light)
{
 if(!Light) return;
 Light->SetCastShadows(true);
 // A small physical emitter gives VSM contact hardening without very broad,
 // noisy penumbras. Short screen-space contact shadows supplement small props.
 Light->SetSourceRadius(6.f);
 Light->ContactShadowLength=.02f;
 Light->MarkRenderStateDirty();
}

int32 VanthexShadows::ApplyGridV4(UWorld* World)
{
 if(!World || World->GetNetMode()==NM_DedicatedServer) return 0;
 int32 Updated=0;
 for(TActorIterator<APointLight> It(World);It;++It)
 {
  if(!It->ActorHasTag(TEXT("GridV4Light"))) continue;
  auto* Light=Cast<UPointLightComponent>(It->GetLightComponent());
  if(!Light) continue;
  bool RoomKey=false;
  for(const auto& Room:GridV4::Rooms())
  {
   const FVector KeyPosition=Room.Center+FVector(0,0,Room.Id==3?900:330);
   if(It->GetActorLocation().Equals(KeyPosition,2.f)) { RoomKey=true; break; }
  }
  // Position lookup deliberately supports pre-existing baked maps without new
  // tags. Restrict changes to the authored room key and already-shadowed lights.
  if(RoomKey || Light->CastShadows)
  {
   ConfigureKeyLight(Light); ++Updated;
  }
 }
 return Updated;
}
