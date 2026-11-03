#include "VanthexOperatorComponent.h"
#include "VanthexOperatorAnimInstance.h"
#include "VanthexCharacter.h"
#include "VanthexArt.h"
#include "HealthComponent.h"
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
 Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Mesh->SetOwnerNoSee(true);
 Mesh->SetCastHiddenShadow(true);
 Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Mesh->SetAnimInstanceClass(UVanthexOperatorAnimInstance::StaticClass());
 Mesh->SetVisibility(true);
 Mesh->ComponentTags.AddUnique(TEXT("VanthexOperator"));
 // The existing tagged proxies remain authoritative for body/head hit detection.
 TInlineComponentArray<UStaticMeshComponent*> OldParts;
 Character->GetComponents(OldParts);
 for(auto* Part:OldParts) if(Part->ComponentHasTag(TEXT("Body"))||Part->ComponentHasTag(TEXT("Head"))) {
  Part->SetVisibility(false); Part->SetCastShadow(false);
 }

 auto* ArmorBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Vanthex/Operators/ARC/M_ARC_Armor.M_ARC_Armor"));
 ArmorMaterial=UMaterialInstanceDynamic::Create(ArmorBase?ArmorBase:VanthexArt::GetSurfaceMaterial(),this);
 if(ArmorMaterial) Mesh->SetMaterial(0,ArmorMaterial);
 auto* Dark=VanthexArt::MakeSurface(this,FLinearColor(.025f,.035f,.045f),.36f,.65f,FLinearColor::Black,0,EVanthexSurface::DarkMetal);
 auto* Helmet=VanthexArt::MakeSurface(this,FLinearColor(.12f,.16f,.21f),.6f,.25f,FLinearColor::Black,0,EVanthexSurface::PaintedSteel);
 AccentMaterial=VanthexArt::MakeEnergy(this,FLinearColor(.05f,.5f,1.f),.8f);
 // Replace the template chest logo; ARC receives its own physical nameplate.
 if(Mesh->GetNumMaterials()>1) Mesh->SetMaterial(1,Dark);

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
  Part->SetOwnerNoSee(true); Part->SetCastHiddenShadow(true);
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
 WorldRifle=NewObject<UStaticMeshComponent>(Character,TEXT("ARC_WorldRifle"));
 WorldRifle->SetStaticMesh(VanthexArt::LoadKit(TEXT("SM_Rifle")));
 WorldRifle->SetupAttachment(Mesh);
 WorldRifle->SetRelativeTransform(UVanthexOperatorAnimInstance::GetRifleTransform(0,0));
 WorldRifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 WorldRifle->SetOwnerNoSee(true); WorldRifle->SetCastHiddenShadow(true);
 WorldRifle->SetMaterial(0,Dark); WorldRifle->SetMaterial(1,AccentMaterial);
 WorldRifle->ComponentTags.Add(TEXT("OperatorRifle")); Character->AddInstanceComponent(WorldRifle); WorldRifle->RegisterComponent();
 WorldMuzzle=NewObject<UStaticMeshComponent>(Character,TEXT("ARC_WorldMuzzle"));
 WorldMuzzle->SetStaticMesh(VanthexArt::LoadSphere()); WorldMuzzle->SetupAttachment(WorldRifle);
 WorldMuzzle->SetRelativeLocation(FVector(23,0,1.1f)); WorldMuzzle->SetRelativeScale3D(FVector(.045f,.025f,.025f));
 WorldMuzzle->SetMaterial(0,VanthexArt::MakeEnergy(this,FLinearColor(1,.42f,.08f),3.2f));
 WorldMuzzle->SetCollisionEnabled(ECollisionEnabled::NoCollision); WorldMuzzle->SetOwnerNoSee(true); WorldMuzzle->SetCastShadow(false); WorldMuzzle->SetVisibility(false);
 Character->AddInstanceComponent(WorldMuzzle); WorldMuzzle->RegisterComponent();
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
 if(!Character||!WorldRifle) return;
 const bool Dead=Character->GetHealthComponent()->IsDead();
 if(auto* Animation=Cast<UVanthexOperatorAnimInstance>(Character->GetMesh()->GetAnimInstance()))
  WorldRifle->SetRelativeTransform(UVanthexOperatorAnimInstance::GetRifleTransform(Animation->GetAimPitch(),Animation->GetCrouchAlpha()));
 WorldRifle->SetVisibility(!Dead);
 MuzzleSeconds=FMath::Max(0.f,MuzzleSeconds-DeltaTime);
 if(WorldMuzzle) WorldMuzzle->SetVisibility(!Dead&&MuzzleSeconds>0);
}
