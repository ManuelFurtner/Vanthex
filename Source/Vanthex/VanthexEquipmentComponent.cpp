#include "VanthexEquipmentComponent.h"
#include "VanthexWeaponPresentation.h"
#include "VanthexWeaponPickup.h"

#include "VanthexCharacter.h"
#include "VanthexPlayerState.h"
#include "VanthexWeaponProfile.h"
#include "VanthexWeaponAudioComponent.h"
#include "VanthexOperatorAnimInstance.h"
#include "VanthexGameMode.h"
#include "VanthexDamage.h"
#include "VanthexArt.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UVanthexEquipmentComponent::UVanthexEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

AVanthexCharacter* UVanthexEquipmentComponent::Character() const
{
	return Cast<AVanthexCharacter>(GetOwner());
}

float UVanthexEquipmentComponent::ServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;
	const AGameStateBase* GS = World->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

bool UVanthexEquipmentComponent::IsSwitching() const
{
	return ServerTime() < State.SwitchStart + State.SwitchDuration;
}

bool UVanthexEquipmentComponent::IsSwinging() const
{
	return ServerTime() < State.SwingStart + State.SwingDuration;
}

FVanthexEquipmentPresentation UVanthexEquipmentComponent::GetPresentation() const
{
	const float Now = ServerTime();
	const float Progress = FMath::Clamp((Now - State.SwitchStart) / FMath::Max(.1f, State.SwitchDuration), 0.f, 1.f);
	const bool Holstering = Progress < .5f;
	FVanthexEquipmentPresentation Pose;
	Pose.VisibleSlot = Holstering ? State.Previous : State.Selected;
	Pose.Lower = FMath::SmoothStep(0.f, 1.f, Holstering ? Progress * 2.f : (1.f - Progress) * 2.f);
	const float SwingDurationSafe = FMath::Max(.1f, State.SwingDuration);
	const float SwingProgress = FMath::Clamp((Now - State.SwingStart) / SwingDurationSafe, 0.f, 1.f);
	const float Contact = FMath::Clamp(StrikeDelay / SwingDurationSafe, .05f, .9f);
	Pose.Swing = FMath::SmoothStep(0.f, 1.f, SwingProgress < Contact ? SwingProgress / Contact : (1.f - SwingProgress) / (1.f - Contact));
	return Pose;
}

bool UVanthexEquipmentComponent::CanUseRifle() const
{
	return State.Selected != EVanthexWeaponSlot::Knife && !IsSwitching() && HasSlot(State.Selected);
}

bool UVanthexEquipmentComponent::CanAct() const
{
	const auto* OwnerCharacter = Character();
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AVanthexGameMode>() : nullptr;
	return OwnerCharacter && OwnerCharacter->HasAuthority() && OwnerCharacter->GetController()
		&& OwnerCharacter->GetHealthComponent() && !OwnerCharacter->GetHealthComponent()->IsDead()
		&& Mode && Mode->IsRoundActive();
}

void UVanthexEquipmentComponent::SelectRifle() { Select(EVanthexWeaponSlot::Rifle); }
void UVanthexEquipmentComponent::SelectPistol() { Select(EVanthexWeaponSlot::Pistol); }
void UVanthexEquipmentComponent::SelectKnife() { Select(EVanthexWeaponSlot::Knife); }

bool UVanthexEquipmentComponent::CanSelect() const
{
 const auto* C=Character(); const auto* Mode=GetWorld()?GetWorld()->GetAuthGameMode<AVanthexGameMode>():nullptr;
 return C&&C->HasAuthority()&&C->GetController()&&C->GetHealthComponent()&&!C->GetHealthComponent()->IsDead()
  &&Mode&&(Mode->IsBuyPhase()||Mode->IsRoundActive());
}
void UVanthexEquipmentComponent::CycleWeapon(int32 Direction)
{
 if(Direction!=1&&Direction!=-1) return;
 if(!GetOwner()->HasAuthority()) { ServerCycleWeapon(Direction); return; }
 if(!CanSelect()||IsSwitching()) return;
 const EVanthexWeaponSlot Slots[]={EVanthexWeaponSlot::Rifle,EVanthexWeaponSlot::Pistol,EVanthexWeaponSlot::Knife};
 int32 Current=0; for(int32 I=0;I<3;++I) if(Slots[I]==State.Selected) Current=I;
 for(int32 Offset=1;Offset<3;++Offset) {
  const auto Slot=Slots[(Current+Direction*Offset+6)%3];
  if(HasSlot(Slot)) { Select(Slot); return; }
 }
}
void UVanthexEquipmentComponent::ServerCycleWeapon_Implementation(int32 Direction) { CycleWeapon(Direction); }

FName UVanthexEquipmentComponent::GetSlotWeaponId(EVanthexWeaponSlot Slot) const
{
 if(Slot==EVanthexWeaponSlot::Knife) return TEXT("Knife");
 if(Slot!=EVanthexWeaponSlot::Rifle&&Slot!=EVanthexWeaponSlot::Pistol) return NAME_None;
 if(GetOwner()->HasAuthority()) return Slot==EVanthexWeaponSlot::Rifle?PrimaryAmmo.Id:SidearmAmmo.Id;
 const auto* C=Character(); if(!C) return NAME_None;
 if(State.Selected==Slot&&C->GetWeapon()&&C->GetWeapon()->GetWeaponId()==TEXT("LegacyRifle")) return TEXT("LegacyRifle");
 // Only owners receive the private inventory; remote presentation uses the
 // public active weapon ID and never replicates the holstered slot contents.
 if(C->IsLocallyControlled()) if(const auto* PS=C->GetPlayerState<AVanthexPlayerState>())
  return Slot==EVanthexWeaponSlot::Rifle?PS->GetEconomy().PrimaryWeapon:PS->GetEconomy().Sidearm;
 return State.Selected==Slot&&C->GetWeapon()?C->GetWeapon()->GetWeaponId():NAME_None;
}
bool UVanthexEquipmentComponent::HasSlot(EVanthexWeaponSlot Slot) const
{
 return Slot==EVanthexWeaponSlot::Knife || VanthexWeapons::Find(GetSlotWeaponId(Slot))!=nullptr;
}
void UVanthexEquipmentComponent::CaptureAmmo()
{
 auto* C=Character(); auto* W=C?C->GetWeapon():nullptr;
 if(!W||State.Selected==EVanthexWeaponSlot::Knife) return;
 FSlotAmmo& Saved=State.Selected==EVanthexWeaponSlot::Rifle?PrimaryAmmo:SidearmAmmo;
 if(Saved.Id==W->GetWeaponId()) { Saved.Magazine=W->GetAmmoInMagazine(); Saved.Reserve=W->GetReserveAmmo(); }
}
void UVanthexEquipmentComponent::ApplySelectedWeapon()
{
 if(!GetOwner()->HasAuthority()) return;
 auto* C=Character(); if(!C||!C->GetWeapon()) return;
 if(State.Selected==EVanthexWeaponSlot::Knife) { C->GetWeapon()->HolsterForKnife(); return; }
 const FSlotAmmo& Saved=State.Selected==EVanthexWeaponSlot::Rifle?PrimaryAmmo:SidearmAmmo;
 if(!Saved.Id.IsNone()) C->GetWeapon()->EquipProfile(Saved.Id,Saved.Magazine,Saved.Reserve);
}
void UVanthexEquipmentComponent::SyncLoadout(FName PrimaryId,FName SidearmId,bool Refill)
{
 if(!GetOwner()->HasAuthority()) return;
 CaptureAmmo();
 const auto* Primary=VanthexWeapons::Find(PrimaryId);
 const auto* Sidearm=VanthexWeapons::Find(SidearmId);
 if(!Primary||Primary->bSidearm) PrimaryId=NAME_None;
 if(!SidearmId.IsNone()&&(!Sidearm||!Sidearm->bSidearm)) SidearmId=TEXT("Pistol");
 if(Refill) { bDeathWeaponsDropped=false; NextTransferTime=0.f; }
 auto Update=[Refill](FSlotAmmo& Saved,FName Id) {
  const auto* Profile=VanthexWeapons::Find(Id);
  if(Saved.Id!=Id||Refill) { Saved.Id=Id; Saved.Magazine=Profile?Profile->MagazineSize:0; Saved.Reserve=Profile?Profile->ReserveAmmo:0; }
 };
 Update(PrimaryAmmo,PrimaryId); Update(SidearmAmmo,SidearmId);
 if(!HasSlot(State.Selected)) { State=FVanthexEquipmentState(); State.Previous=State.Selected=HasSlot(EVanthexWeaponSlot::Pistol)?EVanthexWeaponSlot::Pistol:EVanthexWeaponSlot::Knife; }
 ApplySelectedWeapon();
}
void UVanthexEquipmentComponent::SelectRoundWeapon()
{
 if(!GetOwner()->HasAuthority()) return;
 CaptureAmmo();
 if(GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(SwitchTimer); GetWorld()->GetTimerManager().ClearTimer(StrikeTimer); }
 if(auto* W=Character()?Character()->GetWeapon():nullptr) { W->StopFire(); W->CancelReload(); }
 State=FVanthexEquipmentState();
 State.Previous=State.Selected=HasSlot(EVanthexWeaponSlot::Rifle)?EVanthexWeaponSlot::Rifle:HasSlot(EVanthexWeaponSlot::Pistol)?EVanthexWeaponSlot::Pistol:EVanthexWeaponSlot::Knife;
 ApplySelectedWeapon(); GetOwner()->ForceNetUpdate(); UpdatePresentation();
 if(CanAct()) UVanthexWeaponAudioComponent::DispatchEvent(GetOwner(),GetSlotWeaponId(State.Selected),EVanthexWeaponAudioEvent::Draw,++AudioActionSequence);
}
void UVanthexEquipmentComponent::CommitSwitch()
{
 if(!CanSelect()) return;
 ApplySelectedWeapon();
 UVanthexWeaponAudioComponent::DispatchEvent(GetOwner(),GetSlotWeaponId(State.Selected),EVanthexWeaponAudioEvent::Draw,SwitchActionId);
}
void UVanthexEquipmentComponent::Select(EVanthexWeaponSlot Slot)
{
 if(!GetOwner()->HasAuthority()) { ServerSelect(Slot); return; }
 if(!HasSlot(Slot)||!CanSelect()||IsSwitching()||IsSwinging()||Slot==State.Selected) return;
 CaptureAmmo();
 if(auto* W=Character()->GetWeapon()) { W->StopFire(); W->CancelReload(); }
 SwitchActionId=++AudioActionSequence;
 UVanthexWeaponAudioComponent::DispatchEvent(GetOwner(),GetSlotWeaponId(State.Selected),EVanthexWeaponAudioEvent::Holster,SwitchActionId);
 State.Previous=State.Selected; State.Selected=Slot; State.SwitchStart=ServerTime();
 State.SwitchDuration=FMath::Max(.1f,SwitchDuration);
 GetWorld()->GetTimerManager().SetTimer(SwitchTimer,this,&UVanthexEquipmentComponent::CommitSwitch,State.SwitchDuration*.5f,false);
 GetOwner()->ForceNetUpdate(); UpdatePresentation();
}

void UVanthexEquipmentComponent::ServerSelect_Implementation(EVanthexWeaponSlot Slot) { Select(Slot); }

void UVanthexEquipmentComponent::PublishCarriedWeapons()
{
 if (auto* PS=Character()->GetPlayerState<AVanthexPlayerState>()) PS->SetCarriedWeapons(PrimaryAmmo.Id,SidearmAmmo.Id);
 GetOwner()->ForceNetUpdate();
}

void UVanthexEquipmentComponent::FinishInventoryTransfer(EVanthexWeaponSlot Slot)
{
 auto* C=Character();
 C->SetAiming(false);
 if (auto* W=C->GetWeapon()) { W->StopFire(); W->CancelReload(); }
 GetWorld()->GetTimerManager().ClearTimer(SwitchTimer);
 GetWorld()->GetTimerManager().ClearTimer(StrikeTimer);
 State=FVanthexEquipmentState();
 State.Previous=State.Selected=Slot;
 State.SwitchStart=ServerTime();
 State.SwitchDuration=FMath::Max(.1f,SwitchDuration);
 NextTransferTime=State.SwitchStart+State.SwitchDuration;
 ApplySelectedWeapon();
 PublishCarriedWeapons();
 UpdatePresentation();
}

bool UVanthexEquipmentComponent::DropSelectedWeapon()
{
 if (!CanSelect() || IsSwitching() || IsSwinging() || ServerTime()<NextTransferTime || State.Selected==EVanthexWeaponSlot::Knife || !HasSlot(State.Selected)) return false;
 CaptureAmmo();
 FSlotAmmo& Saved=State.Selected==EVanthexWeaponSlot::Rifle?PrimaryAmmo:SidearmAmmo;
 FVanthexDroppedWeapon Data; Data.Id=Saved.Id; Data.Magazine=Saved.Magazine; Data.Reserve=Saved.Reserve;
 if (!AVanthexWeaponPickup::SpawnDrop(Character(),Data,true)) return false;
 Saved=FSlotAmmo();
 FinishInventoryTransfer(HasSlot(EVanthexWeaponSlot::Rifle)?EVanthexWeaponSlot::Rifle:HasSlot(EVanthexWeaponSlot::Pistol)?EVanthexWeaponSlot::Pistol:EVanthexWeaponSlot::Knife);
 return true;
}

void UVanthexEquipmentComponent::DropWeaponsOnDeath()
{
 auto* C=Character();
 if (!C || !C->HasAuthority() || !C->GetHealthComponent()->IsDead() || bDeathWeaponsDropped) return;
 bDeathWeaponsDropped=true;
 // Snapshot before death clears the economy and before changing selection.
 CaptureAmmo();
 for (FSlotAmmo* Saved : {&PrimaryAmmo,&SidearmAmmo}) {
  if (!Saved->Id.IsNone()) {
   FVanthexDroppedWeapon Data; Data.Id=Saved->Id; Data.Magazine=Saved->Magazine; Data.Reserve=Saved->Reserve;
   AVanthexWeaponPickup::SpawnDrop(C,Data,false);
   *Saved=FSlotAmmo();
  }
 }
 FinishInventoryTransfer(EVanthexWeaponSlot::Knife);
}

bool UVanthexEquipmentComponent::TakeDroppedWeapon(const FVanthexDroppedWeapon& Data)
{
 const auto* Profile=VanthexWeapons::Find(Data.Id);
 if (!CanSelect() || IsSwitching() || IsSwinging() || ServerTime()<NextTransferTime || !Profile || Data.Id==TEXT("LegacyRifle")
  || Data.Magazine<0 || Data.Magazine>Profile->MagazineSize || Data.Reserve<0 || Data.Reserve>Profile->ReserveAmmo) return false;
 CaptureAmmo();
 FSlotAmmo& Saved=Profile->bSidearm?SidearmAmmo:PrimaryAmmo;
 if (!Saved.Id.IsNone()) {
  FVanthexDroppedWeapon Previous; Previous.Id=Saved.Id; Previous.Magazine=Saved.Magazine; Previous.Reserve=Saved.Reserve;
  if (!AVanthexWeaponPickup::SpawnDrop(Character(),Previous,false)) return false;
 }
 Saved.Id=Data.Id; Saved.Magazine=Data.Magazine; Saved.Reserve=Data.Reserve;
 FinishInventoryTransfer(Profile->bSidearm?EVanthexWeaponSlot::Pistol:EVanthexWeaponSlot::Rifle);
 UVanthexWeaponAudioComponent::DispatchEvent(GetOwner(),Data.Id,EVanthexWeaponAudioEvent::Draw,++AudioActionSequence);
 return true;
}
void UVanthexEquipmentComponent::Attack()
{
	if (!GetOwner()->HasAuthority()) { ServerAttack(); return; }
	if (!CanAct() || IsSwitching() || IsSwinging() || State.Selected != EVanthexWeaponSlot::Knife) return;
	State.SwingStart = ServerTime();
	State.SwingDuration = FMath::Max(0.1f, SwingDuration);
 UVanthexWeaponAudioComponent::DispatchEvent(GetOwner(),TEXT("Knife"),EVanthexWeaponAudioEvent::KnifeSwing,++AudioActionSequence);
	GetWorld()->GetTimerManager().SetTimer(StrikeTimer, this, &UVanthexEquipmentComponent::ResolveKnifeStrike,
		FMath::Clamp(StrikeDelay, 0.01f, State.SwingDuration * 0.9f), false);
	GetOwner()->ForceNetUpdate();
}
void UVanthexEquipmentComponent::ServerAttack_Implementation() { Attack(); }

void UVanthexEquipmentComponent::ResolveKnifeStrike()
{
	if (!CanAct() || IsSwitching() || !IsSwinging() || State.Selected != EVanthexWeaponSlot::Knife) return;
	auto* OwnerCharacter = Character();
	const FVector Start = OwnerCharacter->GetPawnViewLocation();
	const FVector End = Start + OwnerCharacter->GetControlRotation().Vector() * FMath::Max(1.f, KnifeRange);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VanthexKnife), false, OwnerCharacter);
	Params.AddIgnoredActor(OwnerCharacter->GetWeapon());
	FHitResult Hit;
	// First blocking surface wins, including walls and friendly players. No through-wall damage.
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return;
	auto* Target = Hit.GetActor();
	auto* Mode = GetWorld()->GetAuthGameMode<AVanthexGameMode>();
	if (!Target || !Mode->CanDamage(OwnerCharacter, Target)) return;
	if (auto* Health = Target->FindComponentByClass<UHealthComponent>())
	{
		FVanthexDamageRequest Damage;
		Damage.InstigatorActor = OwnerCharacter;
		Damage.InstigatorController = OwnerCharacter->GetController();
		Damage.Damage = KnifeDamage;
  Damage.DamageKind = EVanthexDamageKind::Melee;
		Damage.Hit = Hit;
		Health->ApplyDamage(Damage);
	}
}

void UVanthexEquipmentComponent::ResetEquipment()
{
 if(!GetOwner()->HasAuthority()) return;
 SelectRoundWeapon();
}

void UVanthexEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	if (auto* OwnerCharacter = Character()) AddTickPrerequisiteComponent(OwnerCharacter->GetMesh());
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) BuildKnife();
}

void UVanthexEquipmentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(StrikeTimer); GetWorld()->GetTimerManager().ClearTimer(SwitchTimer); }
	Super::EndPlay(Reason);
}

void UVanthexEquipmentComponent::BuildKnife()
{
	auto* OwnerCharacter = Character();
	if (!OwnerCharacter) return;
	KnifeRoot = NewObject<USceneComponent>(OwnerCharacter, TEXT("KnifePivot"));
	KnifeRoot->SetupAttachment(OwnerCharacter->GetFirstPersonCamera());
	OwnerCharacter->AddInstanceComponent(KnifeRoot);
	KnifeRoot->RegisterComponent();
	WorldKnifeRoot = NewObject<USceneComponent>(OwnerCharacter, TEXT("ARC_WorldKnifePivot"));
	WorldKnifeRoot->SetupAttachment(OwnerCharacter->GetMesh());
	WorldKnifeRoot->ComponentTags.Add(TEXT("OperatorKnife"));
	OwnerCharacter->AddInstanceComponent(WorldKnifeRoot);
	WorldKnifeRoot->RegisterComponent();
 TArray<TObjectPtr<UStaticMeshComponent>> FirstPersonParts; FVector Muzzle;
 if(!KnifeMesh) {
  VanthexWeaponPresentation::Build(OwnerCharacter,KnifeRoot,TEXT("Knife"),true,FirstPersonParts,Muzzle);
  VanthexWeaponPresentation::Build(OwnerCharacter,WorldKnifeRoot,TEXT("Knife"),false,WorldKnifeParts,Muzzle);
 } else {
  for(bool FirstPerson:{true,false}) {
   auto* Part=NewObject<UStaticMeshComponent>(OwnerCharacter);
   Part->SetupAttachment(FirstPerson?KnifeRoot.Get():WorldKnifeRoot.Get()); Part->SetStaticMesh(KnifeMesh);
   Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetOnlyOwnerSee(FirstPerson); Part->SetOwnerNoSee(!FirstPerson);
   Part->SetCastShadow(!FirstPerson); Part->SetCastHiddenShadow(!FirstPerson);
   OwnerCharacter->AddInstanceComponent(Part); Part->RegisterComponent();
   if(!FirstPerson) WorldKnifeParts.Add(Part);
  }
 }
 for(const auto& Part:FirstPersonParts) Part->ComponentTags.Add(TEXT("EquipmentKnifeFirstPerson"));
 for(const auto& Part:WorldKnifeParts) Part->ComponentTags.Add(TEXT("EquipmentKnifeWorld"));
	UpdatePresentation();
}

void UVanthexEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetWorld()->GetNetMode() != NM_DedicatedServer) UpdatePresentation();
}

void UVanthexEquipmentComponent::UpdatePresentation()
{
	auto* OwnerCharacter = Character();
	if (!OwnerCharacter) return;
	const FVanthexEquipmentPresentation Pose = GetPresentation();
	const float Lower = Pose.Lower;
	const bool bAlive = OwnerCharacter->GetHealthComponent() && !OwnerCharacter->GetHealthComponent()->IsDead() && !OwnerCharacter->IsHidden();
	if (auto* Rifle = OwnerCharacter->GetWeapon())
	{
		if (Rifle->GetAttachParentActor() != OwnerCharacter)
			Rifle->AttachToComponent(OwnerCharacter->GetFirstPersonCamera(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Rifle->SetActorHiddenInGame(!bAlive || Pose.VisibleSlot == EVanthexWeaponSlot::Knife);
		Rifle->SetActorRelativeLocation(FVector(30.f - 12.f * Lower, 14.f, -12.f - 55.f * Lower));
		Rifle->SetActorRelativeRotation(FRotator(-35.f * Lower, 0.f, 18.f * Lower));
	}
	if (KnifeRoot)
	{
		KnifeRoot->SetVisibility(bAlive && Pose.VisibleSlot == EVanthexWeaponSlot::Knife, true);
		const float Swing = Pose.Swing;
		KnifeRoot->SetRelativeLocation(FVector(42.f + 20.f * Swing - 10.f * Lower, 18.f - 9.f * Swing, -20.f - 50.f * Lower + 4.f * Swing));
		KnifeRoot->SetRelativeRotation(FRotator(10.f - 35.f * Lower - 12.f * Swing, -8.f - 15.f * Swing, -12.f + 35.f * Lower));
	}
	if (WorldKnifeRoot)
	{
		float Pitch = 0.f, Crouch = 0.f;
		if (const auto* Animation = Cast<UVanthexOperatorAnimInstance>(OwnerCharacter->GetMesh()->GetAnimInstance()))
		{
			Pitch = Animation->GetAimPitch(); Crouch = Animation->GetCrouchAlpha();
		}
		FTransform KnifeTransform = UVanthexOperatorAnimInstance::GetKnifeTransform(Pitch, Crouch, Pose.Lower, Pose.Swing);
		// Authored replacements put the grip at their origin; the fallback grip is at X=-4.
		if (KnifeMesh) KnifeTransform.SetTranslation(KnifeTransform.TransformPosition(FVector(-4,0,0)));
		WorldKnifeRoot->SetRelativeTransform(KnifeTransform);
		const bool ShowWorldKnife = bAlive && Pose.VisibleSlot == EVanthexWeaponSlot::Knife;
		WorldKnifeRoot->SetVisibility(ShowWorldKnife, true);
		for (const auto& Part : WorldKnifeParts) if (Part) Part->SetCastHiddenShadow(ShowWorldKnife);
	}
}

void UVanthexEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVanthexEquipmentComponent, State);
}
