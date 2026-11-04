#include "VanthexWeaponPresentation.h"
#include "VanthexImportedWeaponSet.h"
#include "Vanthex.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

FVector VanthexWeaponPresentation::RightPalm(FName WeaponId)
{
 if(WeaponId==TEXT("Knife"))return FVector(-4,0,0);
 if(WeaponId==TEXT("Shotgun"))return FVector(-3,0,-2.5);
 return FVector(-1,0,-3.2);
}
FVector VanthexWeaponPresentation::LeftPalm(FName WeaponId,float ReloadProgress)
{
 const FName Id=ModelId(WeaponId);
 FVector Support(13,0,-1.5);
 if(Id==TEXT("Pistol")||Id==TEXT("P250"))Support=FVector(-1,-4,-6.2);
 else if(Id==TEXT("MP7"))Support=FVector(5,0,-2.5);
 else if(Id==TEXT("MP5"))Support=FVector(12,0,-1.8);
 else if(Id==TEXT("AK"))Support=FVector(16,0,-1.2);
 else if(Id==TEXT("AUG"))Support=FVector(8,0,-4);
 else if(Id==TEXT("DMR"))Support=FVector(15,0,-1.8);
 else if(Id==TEXT("Shotgun"))Support=FVector(14,0,-1.5);
 if(ReloadProgress>=0&&ReloadProgress<1) {
  const float Reach=FMath::SmoothStep(.06f,.17f,ReloadProgress)*(1-FMath::SmoothStep(.68f,.88f,ReloadProgress));
  const float Pull=FMath::SmoothStep(.12f,.25f,ReloadProgress)*(1-FMath::SmoothStep(.48f,.65f,ReloadProgress));
  const FVector Magazine=FVector(3,-2,-5)+FVector(-1.8,-1.5,-9)*Pull;
  Support=FMath::Lerp(Support,Magazine,double(Reach));
 }
 return Support;
}
float VanthexWeaponPresentation::WorldScale(FName Id)
{
 return Id==TEXT("Pistol")||Id==TEXT("P250")||Id==TEXT("Knife")?1.f:1.35f;
}

FVanthexAimPresentation VanthexWeaponPresentation::Aim(FName WeaponId)
{
 const FName Id=ModelId(WeaponId); FVanthexAimPresentation P;
 if(const auto* Imported=UVanthexImportedWeaponSet::Find(Id);Imported&&Id!=TEXT("Knife")) {
  P.Sight=Imported->bReflexSight?EVanthexSight::Reflex:EVanthexSight::Iron;
  P.FieldOfView=Imported->bReflexSight?64.f:72.f;
  P.SightPoint=Imported->SightPoint; P.Rotation.Pitch=Imported->SightPitch;
  const FVector SightAxis=P.Rotation.RotateVector(P.SightPoint);
  P.CameraRelativeOrigin=FVector(Imported->bReflexSight?18:24,-SightAxis.Y,-SightAxis.Z);
  return P;
 }
 // Coordinates come from the authored source geometry; +X is the bore direction.
 if(Id==TEXT("M4")||Id==TEXT("MP7")) {
  P.Sight=EVanthexSight::Reflex; P.FieldOfView=64;
  P.SightPoint=Id==TEXT("M4")?FVector(3,0,7.85):FVector(1,0,6.65);
  P.CameraRelativeOrigin=FVector(18,0,-P.SightPoint.Z);
 } else if(Id==TEXT("AUG")||Id==TEXT("DMR")) {
  P.Sight=EVanthexSight::Scope; P.FieldOfView=Id==TEXT("AUG")?45.f:28.f;
  P.SightPoint=Id==TEXT("AUG")?FVector(-3,0,7.5):FVector(3,0,7.4);
  P.CameraRelativeOrigin=FVector(23,0,-P.SightPoint.Z); P.MovementScale=.45f;
 } else {
  P.SightPoint=Id==TEXT("MP5")?FVector(20,0,4.5):Id==TEXT("AK")?FVector(27,0,6.0):FVector(7.8,0,3.93);
  // The bead must clear the receiver's highest point at local (6,0,3.6).
  if(Id==TEXT("Shotgun")) { P.SightPoint=FVector(33,0,2.95); P.Rotation.Pitch=2.1f; }
  P.CameraRelativeOrigin=FVector(Id==TEXT("Pistol")||Id==TEXT("P250")?28:24,0,-P.Rotation.RotateVector(P.SightPoint).Z);
 }
 return P;
}

FName VanthexWeaponPresentation::ModelId(FName Id) { return Id==TEXT("LegacyRifle")?FName(TEXT("M4")):Id; }
FVector VanthexWeaponPresentation::Muzzle(FName WeaponId)
{
 if(const auto* Imported=UVanthexImportedWeaponSet::Find(WeaponId)) return Imported->Muzzle;
 const FName Id=ModelId(WeaponId);
 if(Id==TEXT("Pistol")||Id==TEXT("P250")) return FVector(9.55,0,1.8);
 if(Id==TEXT("MP5")) return FVector(23.05,0,1.6);
 if(Id==TEXT("MP7")) return FVector(12.75,0,1.6);
 if(Id==TEXT("AK")) return FVector(31.05,0,1.7);
 if(Id==TEXT("AUG")) return FVector(26.05,0,2);
 if(Id==TEXT("DMR")) return FVector(38.05,0,1.5);
 if(Id==TEXT("Shotgun")) return FVector(35.05,0,2);
 if(Id==TEXT("Knife")) return FVector(22,0,1);
 return FVector(30.05,0,1.5);
}
void VanthexWeaponPresentation::Build(AActor* Owner,USceneComponent* Parent,FName WeaponId,bool FirstPerson,
 TArray<TObjectPtr<UStaticMeshComponent>>& Parts,FVector& OutMuzzle)
{
 for(const auto& Part:Parts) if(Part) Part->DestroyComponent();
 Parts.Reset(); OutMuzzle=Muzzle(WeaponId); if(!Owner||!Parent) return;
 const FName Id=ModelId(WeaponId);
 const auto* Imported=UVanthexImportedWeaponSet::Find(Id);
 for(const TCHAR* Suffix:{TEXT("Body"),TEXT("Magazine"),TEXT("Bolt")}) {
  if(Id==TEXT("Knife")&&FCString::Strcmp(Suffix,TEXT("Body"))!=0) continue;
  const FString Name=FString::Printf(TEXT("SM_%s_%s"),*Id.ToString(),Suffix);
  const FString Path=FString::Printf(TEXT("/Game/Vanthex/Weapons/Meshes/%s.%s"),*Name,*Name);
  auto* Mesh=Imported?(FCString::Strcmp(Suffix,TEXT("Body"))==0?Imported->Body.Get():
   FCString::Strcmp(Suffix,TEXT("Magazine"))==0?Imported->Magazine.Get():Imported->Bolt.Get()):LoadObject<UStaticMesh>(nullptr,*Path);
  // A source without a separately rigged bolt must not receive another model's bolt geometry.
  if(Imported&&!Mesh) continue;
  if(!Mesh) { UE_LOG(LogVanthex,Error,TEXT("Missing authored weapon asset %s; run VanthexBuildWeaponModels"),*Path); continue; }
  auto* Part=NewObject<UStaticMeshComponent>(Owner); Part->SetupAttachment(Parent); Part->SetStaticMesh(Mesh);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetGenerateOverlapEvents(false);
  Part->SetOnlyOwnerSee(FirstPerson); Part->SetOwnerNoSee(!FirstPerson);
  Part->SetCastShadow(!FirstPerson); Part->SetCastHiddenShadow(!FirstPerson);
  Part->ComponentTags.Add(FName(FString(TEXT("WeaponPart_"))+Suffix));
  Part->ComponentTags.Add(TEXT("VanthexAuthoredWeapon"));
  if(Imported) Part->ComponentTags.Add(TEXT("VanthexImportedWeapon"));
  Owner->AddInstanceComponent(Part); Part->RegisterComponent(); Parts.Add(Part);
 }
 Animate(Parts,Id,-1,0);
}

void VanthexWeaponPresentation::Animate(const TArray<TObjectPtr<UStaticMeshComponent>>& Parts,FName WeaponId,float Progress,float ShotPulse)
{
 const FName Id=ModelId(WeaponId); const bool Shotgun=Id==TEXT("Shotgun");
 const bool Reload=Progress>=0.f&&Progress<1.f;
 float Extract=0;
 if(Reload) {
  if(Progress<.25f) Extract=FMath::SmoothStep(.12f,.25f,Progress);
  else if(Progress<.48f) Extract=1;
  else Extract=1-FMath::SmoothStep(.48f,.65f,Progress);
 }
 const float Bolt=Reload?FMath::Sin(FMath::Clamp((Progress-.74f)/.16f,0.f,1.f)*PI):FMath::Clamp(ShotPulse,0.f,1.f);
 for(const auto& Part:Parts) if(Part) {
  if(Part->ComponentHasTag(TEXT("WeaponPart_Magazine"))) {
   if(Shotgun) {
    // The tube-fed shell carrier is only visible during insertion.
    Part->SetHiddenInGame(!Reload||Progress<.18f||Progress>.67f);
    Part->SetRelativeLocation(FVector(0,-3*Extract,-5*Extract));
    Part->SetRelativeRotation(FRotator(0,0,-25*Extract));
   } else {
    Part->SetHiddenInGame(Reload&&Progress>.27f&&Progress<.46f);
    Part->SetRelativeLocation(FVector(-1.8f*Extract,-1.5f*Extract,-9.f*Extract));
    Part->SetRelativeRotation(FRotator(12*Extract,0,-8*Extract));
   }
  } else if(Part->ComponentHasTag(TEXT("WeaponPart_Bolt"))) {
   Part->SetRelativeLocation(FVector(-(Shotgun?3.2f:Id==TEXT("Pistol")||Id==TEXT("P250")?1.6f:1.f)*Bolt,0,0));
  }
 }
}
