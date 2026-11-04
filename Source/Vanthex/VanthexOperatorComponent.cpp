#include "VanthexOperatorComponent.h"
#include "VanthexOperatorAnimInstance.h"
#include "VanthexCharacter.h"
#include "HitscanWeapon.h"
#include "VanthexWeaponProfile.h"
#include "VanthexWeaponPresentation.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexArt.h"
#include "HealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

UVanthexOperatorComponent::UVanthexOperatorComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
}

void UVanthexOperatorComponent::BeginPlay()
{
 Super::BeginPlay();
 Character=Cast<AVanthexCharacter>(GetOwner());
 if(!Character) return;
 auto* Mesh=Character->GetMesh();
 AddTickPrerequisiteComponent(Mesh);
 auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Meshes/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
 if(!Asset) return;
 Mesh->SetSkeletalMesh(Asset);
 Mesh->SetRelativeLocationAndRotation(FVector(0,0,-96),FRotator(0,-90,0));
 // The imported rig's articulated bodies follow the final animation pose. The
 // movement capsule and old coarse boxes must not steal hits from those bodies.
 auto* HitAsset=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/Meshes/Mannequin/Character/Mesh/SK_Mannequin_PhysicsAsset.SK_Mannequin_PhysicsAsset"));
 if(HitAsset) {
  Mesh->SetPhysicsAsset(HitAsset);
  Mesh->SetCollisionObjectType(ECC_Pawn);
  Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
  Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
  Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  Mesh->SetAllBodiesSimulatePhysics(false);
  Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 }
 Mesh->SetOwnerNoSee(true);
 Mesh->SetCastShadow(true);
 Mesh->SetCastHiddenShadow(true);
 Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Mesh->SetAnimInstanceClass(UVanthexOperatorAnimInstance::StaticClass());
 Mesh->SetVisibility(true);
 Mesh->ComponentTags.AddUnique(TEXT("VanthexOperator"));
 // Keep coarse proxies only as a fallback when the rig's physics asset is absent.
 TInlineComponentArray<UStaticMeshComponent*> OldParts;
 Character->GetComponents(OldParts);
 for(auto* Part:OldParts) if(Part->ComponentHasTag(TEXT("Body"))||Part->ComponentHasTag(TEXT("Head"))) {
  Part->SetVisibility(false); Part->SetCastShadow(false);
  if(HitAsset)Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 }

 auto* ArmorBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Vanthex/Operators/ARC/M_ARC_Armor.M_ARC_Armor"));
 ArmorMaterial=UMaterialInstanceDynamic::Create(ArmorBase?ArmorBase:VanthexArt::GetSurfaceMaterial(),this);
 if(ArmorMaterial) Mesh->SetMaterial(0,ArmorMaterial);
 auto* Dark=VanthexArt::MakeSurface(this,FLinearColor(.025f,.035f,.045f),.36f,.65f,FLinearColor::Black,0,EVanthexSurface::DarkMetal);
 auto* Helmet=VanthexArt::MakeSurface(this,FLinearColor(.12f,.16f,.21f),.6f,.25f,FLinearColor::Black,0,EVanthexSurface::PaintedSteel);
 AccentMaterial=VanthexArt::MakeEnergy(this,FLinearColor(.05f,.5f,1.f),.8f);
 // Replace the template chest logo; ARC receives its own physical nameplate.
 if(Mesh->GetNumMaterials()>1) Mesh->SetMaterial(1,Dark);

 // An owner-only body keeps the authoritative world mesh and its complete shadow
 // intact. A separate pose permits hiding the camera-intersecting head/arms only
 // in first person, including when a roster pawn changes from bot to human.
 FirstPersonBody=NewObject<USkeletalMeshComponent>(Character,TEXT("ARC_FirstPersonBody"));
 FirstPersonBody->SetupAttachment(Mesh);
 FirstPersonBody->SetRelativeLocation(FVector(0,-32,0));
 FirstPersonBody->SetSkeletalMesh(Asset);
 FirstPersonBody->SetAnimInstanceClass(UVanthexOperatorAnimInstance::StaticClass());
 FirstPersonBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 FirstPersonBody->SetGenerateOverlapEvents(false);
 FirstPersonBody->SetOnlyOwnerSee(true); FirstPersonBody->SetCastShadow(false);
 FirstPersonBody->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 FirstPersonBody->SetMaterial(0,ArmorMaterial); if(Mesh->GetNumMaterials()>1) FirstPersonBody->SetMaterial(1,Dark);
 Character->AddInstanceComponent(FirstPersonBody); FirstPersonBody->RegisterComponent();
 for(const FName Bone:{FName(TEXT("neck_01")),FName(TEXT("upperarm_l")),FName(TEXT("upperarm_r"))})
  FirstPersonBody->HideBoneByName(Bone,PBO_None);
 FirstPersonBody->ComponentTags.Add(TEXT("VanthexFirstPersonBody"));

 // Camera-relative arms use the actual first-person equipment transform, including
 // ADS, recoil and reload. The separate body below the camera keeps its locomotion.
 FirstPersonArms=NewObject<USkeletalMeshComponent>(Character,TEXT("ARC_FirstPersonArms"));
 FirstPersonArms->SetupAttachment(Character->GetFirstPersonCamera());
 FirstPersonArms->SetRelativeLocationAndRotation(FVector(8,0,-160),FRotator(0,-90,0));
 auto* ArmsAsset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Vanthex/Operators/ARC/SK_ARC_FirstPersonArms.SK_ARC_FirstPersonArms"));
 FirstPersonArms->SetSkeletalMesh(ArmsAsset?ArmsAsset:Asset);
 FirstPersonArms->ComponentTags.Add(TEXT("VanthexFirstPersonArms"));
 FirstPersonArms->SetAnimInstanceClass(UVanthexOperatorAnimInstance::StaticClass());
 FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 FirstPersonArms->SetGenerateOverlapEvents(false);
 FirstPersonArms->SetOnlyOwnerSee(true); FirstPersonArms->SetCastShadow(false);
 FirstPersonArms->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 FirstPersonArms->SetMaterial(0,ArmorMaterial); if(Mesh->GetNumMaterials()>1)FirstPersonArms->SetMaterial(1,Dark);
 Character->AddInstanceComponent(FirstPersonArms);FirstPersonArms->RegisterComponent();
 for(const FName Bone:{FName(TEXT("neck_01")),FName(TEXT("thigh_l")),FName(TEXT("thigh_r"))})FirstPersonArms->HideBoneByName(Bone,PBO_None);
 FirstPersonArms->PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 FirstPersonArms->AddTickPrerequisiteComponent(this);

 const FReferenceSkeleton& Skeleton=Asset->GetRefSkeleton();
 TArray<FTransform> RefCS=Skeleton.GetRefBonePose();
 for(int32 Index=0;Index<RefCS.Num();++Index) if(Skeleton.GetParentIndex(Index)!=INDEX_NONE) RefCS[Index]*=RefCS[ Skeleton.GetParentIndex(Index) ];
 auto Piece=[&](const TCHAR* Name,UStaticMesh* Shape,const FName Bone,const FVector& Offset,const FVector& Scale,UMaterialInterface* Material,const FRotator& Rotation=FRotator::ZeroRotator) {
  const int32 BoneIndex=Skeleton.FindBoneIndex(Bone);
  if(BoneIndex==INDEX_NONE||!Shape) return static_cast<UStaticMeshComponent*>(nullptr);
  auto* Part=NewObject<UStaticMeshComponent>(Character,FName(Name));
  Part->SetStaticMesh(Shape); Part->SetupAttachment(Mesh,Bone);
  const FTransform Desired(Rotation,RefCS[BoneIndex].GetLocation()+Offset,Scale);
  Part->SetRelativeTransform(Desired.GetRelativeTransform(RefCS[BoneIndex]));
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->SetOwnerNoSee(true); Part->SetCastShadow(true); Part->SetCastHiddenShadow(true);
  Part->SetMaterial(0,Material); Character->AddInstanceComponent(Part); Part->RegisterComponent();
  return Part;
 };
 Piece(TEXT("ARC_Helmet"),VanthexArt::LoadSphere(),TEXT("head"),FVector(0,0,8),FVector(.245f,.225f,.27f),Helmet);
 Piece(TEXT("ARC_Visor"),VanthexArt::LoadSphere(),TEXT("head"),FVector(0,10,10),FVector(.20f,.045f,.08f),AccentMaterial);
 Piece(TEXT("ARC_Respirator"),VanthexArt::LoadSphere(),TEXT("head"),FVector(0,8,1),FVector(.14f,.10f,.09f),Dark);
 Piece(TEXT("ARC_Comms"),VanthexArt::LoadSphere(),TEXT("head"),FVector(-12,0,8),FVector(.065f,.11f,.12f),Dark);
 Piece(TEXT("ARC_Antenna"),VanthexArt::LoadCylinder(),TEXT("head"),FVector(-12,0,21),FVector(.016f,.016f,.19f),Dark);
 auto* Chest=Piece(TEXT("ARC_Nameplate"),VanthexArt::LoadCube(),TEXT("spine_03"),FVector(0,11,-5),FVector(.23f,.045f,.12f),Dark);
 if(Chest) {
  auto* Label=NewObject<UTextRenderComponent>(Character,TEXT("ARC_Label"));
  Label->SetupAttachment(Chest); Label->SetUsingAbsoluteScale(true); Label->SetRelativeScale3D(FVector(1));
  Label->SetRelativeLocation(FVector(0,52,0)); Label->SetRelativeRotation(FRotator(0,90,0));
  Label->SetText(FText::FromString(TEXT("ARC"))); Label->SetWorldSize(4.2f);
  Label->SetHorizontalAlignment(EHTA_Center); Label->SetVerticalAlignment(EVRTA_TextCenter);
  Label->SetTextRenderColor(FColor(205,222,235)); Label->SetOwnerNoSee(true);
  Label->SetCollisionEnabled(ECollisionEnabled::NoCollision); Character->AddInstanceComponent(Label); Label->RegisterComponent();
 }
 WorldFirearmRoot=NewObject<USceneComponent>(Character,TEXT("ARC_WorldFirearmPivot"));
 WorldFirearmRoot->SetupAttachment(Mesh);
 WorldFirearmRoot->SetRelativeTransform(UVanthexOperatorAnimInstance::GetRifleTransform(0,0));
 Character->AddInstanceComponent(WorldFirearmRoot); WorldFirearmRoot->RegisterComponent();
 WorldMuzzle=NewObject<UStaticMeshComponent>(Character,TEXT("ARC_WorldMuzzle"));
 WorldMuzzle->SetStaticMesh(VanthexArt::LoadSphere()); WorldMuzzle->SetupAttachment(WorldFirearmRoot);
 WorldMuzzle->SetRelativeLocation(FVector(23,0,1.1f)); WorldMuzzle->SetRelativeScale3D(FVector(.045f,.025f,.025f));
 WorldMuzzle->SetMaterial(0,VanthexArt::MakeEnergy(this,FLinearColor(1,.42f,.08f),3.2f));
 WorldMuzzle->SetCollisionEnabled(ECollisionEnabled::NoCollision); WorldMuzzle->SetOwnerNoSee(true); WorldMuzzle->SetCastShadow(false); WorldMuzzle->SetVisibility(false);
 Character->AddInstanceComponent(WorldMuzzle); WorldMuzzle->RegisterComponent();
 RefreshWeaponVisual();
 SetTeamAppearance(CurrentTeam);
}

void UVanthexOperatorComponent::SetTeamAppearance(EVanthexTeam Team)
{
 CurrentTeam=Team;
 const FLinearColor TeamColor=Team==EVanthexTeam::Bravo?FLinearColor(.9f,.22f,.035f):FLinearColor(.025f,.32f,.85f);
 if(ArmorMaterial) ArmorMaterial->SetVectorParameterValue(TEXT("TeamColor"),TeamColor);
 if(AccentMaterial) { AccentMaterial->SetVectorParameterValue(TEXT("Color"),TeamColor); AccentMaterial->SetVectorParameterValue(TEXT("EmissiveColor"),TeamColor); }
}

bool UVanthexOperatorComponent::IsPresentationReady() const
{
 return Character&&Character->GetMesh()->GetSkeletalMeshAsset()&&Character->GetMesh()->GetAnimInstance()&&ArmorMaterial&&WorldRifle&&WorldRifle->GetStaticMesh();
}

FVector UVanthexOperatorComponent::GetMuzzleLocation() const
{
 return WorldMuzzle?WorldMuzzle->GetComponentLocation():GetOwner()->GetActorLocation();
}

void UVanthexOperatorComponent::PresentShot() { MuzzleSeconds=.06f; }

void UVanthexOperatorComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
 if(!Character||!WorldFirearmRoot) return;
 RefreshWeaponVisual();
 const bool Dead=Character->GetHealthComponent()->IsDead();
 if(FirstPersonBody) {
  const bool OwnView=Character->IsPlayerControlled()&&Character->IsLocallyControlled()&&!Dead&&!Character->IsHidden();
  FirstPersonBody->SetVisibility(OwnView);
  FirstPersonBody->SetComponentTickEnabled(OwnView);
  if(FirstPersonArms){FirstPersonArms->SetVisibility(OwnView);FirstPersonArms->SetComponentTickEnabled(OwnView);}
 }
 const auto* Equipment=Character->GetEquipment();
 const auto Presentation=Equipment?Equipment->GetPresentation():FVanthexEquipmentPresentation{};
 const bool ShowRifle=!Dead&&!Character->IsHidden()&&Presentation.VisibleSlot!=EVanthexWeaponSlot::Knife;
 if(auto* Animation=Cast<UVanthexOperatorAnimInstance>(Character->GetMesh()->GetAnimInstance()))
  WorldFirearmRoot->SetRelativeTransform(UVanthexOperatorAnimInstance::GetRifleTransform(Animation->GetAimPitch(),Animation->GetCrouchAlpha(),Presentation.Lower,VisualWeaponId));
 WorldFirearmRoot->SetVisibility(ShowRifle,true);
 for(const auto& Part:WorldFirearmParts) if(Part) Part->SetCastHiddenShadow(ShowRifle);
 if(const auto* Weapon=Character->GetWeapon()) VanthexWeaponPresentation::Animate(WorldFirearmParts,Weapon->GetWeaponId(),Weapon->GetReloadProgress(),FMath::Clamp(MuzzleSeconds/.06f,0.f,1.f));
 MuzzleSeconds=FMath::Max(0.f,MuzzleSeconds-DeltaTime);
 if(WorldMuzzle) WorldMuzzle->SetVisibility(ShowRifle&&MuzzleSeconds>0);
}
void UVanthexOperatorComponent::RefreshWeaponVisual()
{
 if(!Character||!WorldFirearmRoot) return;
 const FName Id=Character->GetWeapon()?Character->GetWeapon()->GetWeaponId():FName(TEXT("Pistol"));
 if(Id==TEXT("Knife")) return;
 if(Id==VisualWeaponId&&WorldRifle) return;
 VisualWeaponId=Id; FVector Muzzle;
 VanthexWeapons::BuildVisual(Character,WorldFirearmRoot,Id,false,WorldFirearmParts,Muzzle,AccentMaterial);
 WorldRifle=WorldFirearmParts.IsEmpty()?nullptr:WorldFirearmParts[0].Get();
 if(WorldRifle) WorldRifle->ComponentTags.AddUnique(TEXT("OperatorRifle"));
 if(WorldMuzzle) WorldMuzzle->SetRelativeLocation(Muzzle);
}
