#include "VanthexWeaponAudioMovement.h"

bool FVanthexEquipmentStepClock::Advance(FVector Position,FVector Velocity,float DeltaSeconds,bool bGrounded,bool bAlive,bool bRoundActive,bool bTeleported,bool bCrouched)
{
 const FVector Displacement=Position-Previous;
 const bool HadSample=bSampled;
 Previous=Position; bSampled=true;
 const float Speed=float(Velocity.Size2D());
 const float Travel=float(Displacement.Size2D());
 if(!HadSample || Position.ContainsNaN() || Velocity.ContainsNaN() || !FMath::IsFinite(DeltaSeconds)
  || DeltaSeconds<=0.f || DeltaSeconds>.25f || !bGrounded || !bAlive || !bRoundActive || bTeleported
  || Speed<10.f || Travel<.05f || Travel>FMath::Max(100.f,Speed*DeltaSeconds*2.f+20.f) || FMath::Abs(Displacement.Z)>100.) {
  Distance=0.f; SinceStep=0.f; return false;
 }
 Distance+=Travel; SinceStep+=DeltaSeconds;
 const float Stride=bCrouched?120.f:(Speed>700.f?210.f:170.f);
 if(Distance<Stride || SinceStep<.18f) return false;
 // At most one event per update; stalls, teleports and airborne motion cannot accumulate a burst.
 Distance=FMath::Fmod(Distance,Stride); SinceStep=0.f;
 return true;
}
