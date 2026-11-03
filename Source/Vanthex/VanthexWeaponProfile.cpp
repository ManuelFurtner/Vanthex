#include "VanthexWeaponProfile.h"
#include "VanthexArt.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"

float FVanthexWeaponProfile::DamageAtDistance(float Distance) const
{
 // Preserve exact clamp endpoints instead of losing a float ULP in 1+(Min-1).
 if(Distance<=FalloffStart) return BodyDamage;
 if(Distance>=FalloffEnd) return BodyDamage*MinimumDamageScale;
 const float Alpha = FMath::Clamp((Distance - FalloffStart) / FMath::Max(1.f, FalloffEnd - FalloffStart), 0.f, 1.f);
 return BodyDamage * FMath::Lerp(1.f, MinimumDamageScale, Alpha);
}

FVector2D FVanthexWeaponProfile::RecoilForShot(int32 BurstIndex) const
{
 static constexpr float Pattern[] = {0.f,.45f,-.65f,.8f,1.f,-.9f,-.55f,.75f};
 return FVector2D(RecoilPitch * (1.f + .06f * FMath::Min(BurstIndex, 8)), RecoilYaw * Pattern[FMath::Abs(BurstIndex) % UE_ARRAY_COUNT(Pattern)]);
}

FVector FVanthexWeaponProfile::ShotDirection(const FRotator& Aim, uint32 Sequence, int32 Pellet, float Speed) const
{
 const float Cone = SpreadDegrees + MovingSpreadDegrees * FMath::Clamp(Speed / 600.f, 0.f, 1.f);
 FRandomStream Random(FCrc::StrCrc32(*Id.ToString()) ^ Sequence * 196613u ^ uint32(Pellet + 1) * 7919u);
 return Random.VRandCone(Aim.Vector(), FMath::DegreesToRadians(Cone));
}

const TArray<FVanthexWeaponProfile>& VanthexWeapons::All()
{
 static const TArray<FVanthexWeaponProfile> Profiles = [] {
  TArray<FVanthexWeaponProfile> Result;
  auto Add = [&](const TCHAR* Id, bool Sidearm, bool Auto, EVanthexFirearmShape Shape, int32 Mag, int32 Reserve,
   float Damage, float Head, float Interval, float Reload, float Near, float Far, float Minimum, float Spread, float Moving, float Kick, float Yaw, int32 Pellets = 1) {
   FVanthexWeaponProfile P; P.Id=Id; P.bSidearm=Sidearm; P.FireMode=Auto?EVanthexFireMode::Automatic:EVanthexFireMode::SemiAutomatic;
   P.Shape=Shape; P.MagazineSize=Mag; P.ReserveAmmo=Reserve; P.BodyDamage=Damage; P.HeadshotMultiplier=Head;
   P.FireInterval=Interval; P.ReloadDuration=Reload; P.FalloffStart=Near; P.FalloffEnd=Far; P.MinimumDamageScale=Minimum;
   P.SpreadDegrees=Spread; P.MovingSpreadDegrees=Moving; P.RecoilPitch=Kick; P.RecoilYaw=Yaw; P.Pellets=Pellets;
   if(Shape==EVanthexFirearmShape::Shotgun) { P.TraceRange=5000.f; P.bSupersonic=false; }
   Result.Add(P);
  };
  Add(TEXT("Pistol"),true,false,EVanthexFirearmShape::Pistol,12,48,28,3,.22f,1.35f,1500,4500,.55f,.10f,.8f,.8f,.08f);
  Add(TEXT("P250"),true,false,EVanthexFirearmShape::Pistol,13,52,36,2.5f,.20f,1.55f,1800,5000,.60f,.12f,.9f,1.2f,.12f);
  Add(TEXT("MP5"),false,true,EVanthexFirearmShape::SMG,30,120,24,2,.086f,2.1f,1200,3500,.50f,.35f,1.2f,.45f,.18f);
  Add(TEXT("MP7"),false,true,EVanthexFirearmShape::SMG,40,160,22,2.1f,.0625f,1.9f,1000,3200,.45f,.30f,1.f,.35f,.15f);
  Add(TEXT("M4"),false,true,EVanthexFirearmShape::Rifle,30,90,34,2.5f,.10f,2.2f,2500,9000,.65f,.12f,1.3f,.7f,.15f);
  Add(TEXT("AK"),false,true,EVanthexFirearmShape::Rifle,30,90,38,2.6f,.115f,2.4f,3000,10000,.65f,.16f,1.5f,1.05f,.25f);
  Add(TEXT("AUG"),false,true,EVanthexFirearmShape::Bullpup,30,90,32,3,.095f,2.45f,3500,12000,.75f,.06f,1.15f,.48f,.08f);
  Add(TEXT("DMR"),false,false,EVanthexFirearmShape::Marksman,12,48,58,2.2f,.40f,2.6f,5000,15000,.8f,.03f,1.8f,2.f,.05f);
  Add(TEXT("Shotgun"),false,false,EVanthexFirearmShape::Shotgun,8,32,11,1.25f,.85f,2.8f,700,2400,.22f,3.2f,1.f,2.5f,.1f,8);
  // Explicit diagnostic fixture only; never offered by the economy catalog.
  Add(TEXT("LegacyRifle"),false,true,EVanthexFirearmShape::Rifle,30,210,40,2,.15f,1.6f,15000,16000,1,0,0,0,0);
  return Result;
 }();
 return Profiles;
}

const FVanthexWeaponProfile* VanthexWeapons::Find(FName Id)
{
 return All().FindByPredicate([Id](const FVanthexWeaponProfile& P){ return P.Id==Id; });
}

void VanthexWeapons::BuildVisual(AActor* Owner, USceneComponent* Parent, FName Id, bool FirstPerson,
 TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FVector& Muzzle, UMaterialInterface* Accent)
{
 for(const auto& Part:Parts) if(Part) Part->DestroyComponent();
 Parts.Reset();
 const auto* P=Find(Id); if(!Owner||!Parent||!P) return;
 auto* Metal=VanthexArt::MakeSurface(Owner,FLinearColor(.075f,.085f,.095f),.48f,.72f);
 auto* Grip=VanthexArt::MakeSurface(Owner,FLinearColor(.025f,.03f,.035f),.8f,.05f);
 if(!Accent) Accent=VanthexArt::MakeEnergy(Owner,FLinearColor(.1f,.45f,.7f),.4f);
 auto Add=[&](UStaticMesh* Mesh,FVector At,FVector Scale,UMaterialInterface* Material,FRotator Rotation=FRotator::ZeroRotator) {
  auto* Part=NewObject<UStaticMeshComponent>(Owner); Part->SetupAttachment(Parent); Part->SetStaticMesh(Mesh);
  Part->SetRelativeLocationAndRotation(At,Rotation); Part->SetRelativeScale3D(Scale); Part->SetMaterial(0,Material);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetOnlyOwnerSee(FirstPerson); Part->SetOwnerNoSee(!FirstPerson);
  Part->SetCastHiddenShadow(!FirstPerson); Owner->AddInstanceComponent(Part); Part->RegisterComponent(); Parts.Add(Part);
  return Part;
 };
 auto* Cube=VanthexArt::LoadCube();
 Muzzle=FVector(23,0,1.1f);
 if(P->Shape==EVanthexFirearmShape::Pistol) {
  Add(Cube,FVector(2,0,1),FVector(.18f,.035f,.045f),Metal);
  Add(Cube,FVector(0,0,-1.5f),FVector(.12f,.038f,.035f),Grip);
  Add(Cube,FVector(-1,0,-4),FVector(.045f,.035f,.08f),Grip,FRotator(-12,0,0));
  Add(Cube,FVector(7,0,3.5f),FVector(.012f,.01f,.012f),Accent);
  Muzzle=FVector(12,0,1);
 } else if(P->Shape==EVanthexFirearmShape::SMG) {
  Add(Cube,FVector(0,0,0),FVector(.20f,.05f,.07f),Metal);
  Add(Cube,FVector(-1,0,-5),FVector(.04f,.035f,.10f),Grip);
  Add(Cube,FVector(-14,0,0),FVector(.08f,.035f,.045f),Grip);
  Add(Cube,FVector(14,0,1),FVector(.09f,.02f,.02f),Metal);
  Add(Cube,FVector(6,0,-5),FVector(.025f,.035f,.06f),Grip);
  Add(Cube,FVector(0,0,5),FVector(.08f,.025f,.02f),Accent);
  Muzzle=FVector(19,0,1);
 } else if(P->Shape==EVanthexFirearmShape::Bullpup) {
  Add(Cube,FVector(-3,0,0),FVector(.25f,.055f,.075f),Metal);
  Add(Cube,FVector(-12,0,-5),FVector(.045f,.035f,.10f),Grip);
  Add(Cube,FVector(-1,0,-3.2f),FVector(.04f,.035f,.07f),Grip);
  Add(Cube,FVector(14,0,1),FVector(.19f,.02f,.02f),Metal);
  Add(Cube,FVector(-3,0,6),FVector(.12f,.035f,.035f),Grip);
  Add(Cube,FVector(3,0,6),FVector(.012f,.025f,.025f),Accent);
  Muzzle=FVector(24,0,1);
 } else {
  auto* Mesh=VanthexArt::LoadKit(TEXT("SM_Rifle"));
  auto* Body=Add(Mesh?Mesh:Cube,FVector::ZeroVector,Mesh?FVector(1):FVector(.4f,.05f,.06f),Metal);
  if(Mesh) Body->SetMaterial(1,Accent);
  if(P->Shape==EVanthexFirearmShape::Marksman) {
   Add(Cube,FVector(0,0,7),FVector(.18f,.035f,.035f),Grip); Add(Cube,FVector(26,0,1),FVector(.14f,.02f,.02f),Metal); Muzzle.X=34;
  } else if(P->Shape==EVanthexFirearmShape::Shotgun) {
   Add(Cube,FVector(16,0,0),FVector(.24f,.045f,.04f),Metal); Add(Cube,FVector(10,0,-3),FVector(.12f,.05f,.035f),Grip); Muzzle.X=29;
  }
 }
}
