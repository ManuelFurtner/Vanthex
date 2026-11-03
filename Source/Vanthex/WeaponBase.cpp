#include "WeaponBase.h"
#include "VanthexWeaponAudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWeaponBase::AWeaponBase()
{
 PrimaryActorTick.bCanEverTick=false; bReplicates=true; bNetUseOwnerRelevancy=true;
 WeaponRoot=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot")); SetRootComponent(WeaponRoot);
}
const FVanthexWeaponProfile& AWeaponBase::GetProfile() const
{
 const auto* Profile=VanthexWeapons::Find(ActiveWeaponId);
 return Profile?*Profile:*VanthexWeapons::Find(TEXT("Pistol"));
}
void AWeaponBase::ApplyProfileSettings()
{
 const auto& P=GetProfile(); MagazineSize=P.MagazineSize; FireInterval=P.FireInterval;
 ReloadDuration=P.ReloadDuration; BodyDamage=P.BodyDamage; HeadshotMultiplier=P.HeadshotMultiplier;
 OnWeaponProfileChanged();
}
void AWeaponBase::OnRep_ActiveWeaponId() { ApplyProfileSettings(); }
void AWeaponBase::HolsterForKnife()
{
 if(!HasAuthority()) return;
 StopFire(); CancelReload(); ActiveWeaponId=TEXT("Knife"); ForceNetUpdate();
}
bool AWeaponBase::EquipProfile(FName Id,int32 Magazine,int32 Reserve)
{
 if(!HasAuthority()||!VanthexWeapons::Find(Id)) return false;
 StopFire(); CancelReload(); ActiveWeaponId=Id; ApplyProfileSettings();
 SetMagazineAmmo(Magazine<0?MagazineSize:Magazine);
 SetReserveAmmo(Reserve<0?GetProfile().ReserveAmmo:Reserve);
 ForceNetUpdate(); return true;
}
void AWeaponBase::BeginPlay()
{
 Super::BeginPlay(); ApplyProfileSettings();
 if(HasAuthority()) { SetMagazineAmmo(MagazineSize); SetReserveAmmo(GetProfile().ReserveAmmo); }
}
void AWeaponBase::EndPlay(const EEndPlayReason::Type Reason)
{
 CancelReload(); Super::EndPlay(Reason);
}
void AWeaponBase::SetMagazineAmmo(int32 Value) { if(HasAuthority()) AmmoInMagazine=FMath::Clamp(Value,0,MagazineSize); }
void AWeaponBase::SetReserveAmmo(int32 Value) { if(HasAuthority()) ReserveAmmo=FMath::Clamp(Value,0,GetProfile().ReserveAmmo); }
void AWeaponBase::ResetForRound()
{
 if(!HasAuthority()) return;
 StopFire(); CancelReload(); SetMagazineAmmo(MagazineSize); SetReserveAmmo(GetProfile().ReserveAmmo);
}
void AWeaponBase::StartFire() { bTriggerHeld=true; }
void AWeaponBase::StopFire() { bTriggerHeld=false; }
void AWeaponBase::StartReload()
{
 if(!HasAuthority()||!GetWorld()||bReloading||AmmoInMagazine>=MagazineSize||ReserveAmmo<=0) return;
 bReloading=true; ReloadActionId=NextActionId();
 UVanthexWeaponAudioComponent::DispatchEvent(this,ActiveWeaponId,EVanthexWeaponAudioEvent::ReloadStart,ReloadActionId);
 const uint32 Action=ReloadActionId;
 auto Mark=[this,Action](EVanthexWeaponAudioEvent Event) {
  if(bReloading&&Action==ReloadActionId) UVanthexWeaponAudioComponent::DispatchEvent(this,ActiveWeaponId,Event,Action);
 };
 auto& Timers=GetWorld()->GetTimerManager(); const auto& P=GetProfile();
 Timers.SetTimer(MagazineOutTimer,[Mark]{ Mark(EVanthexWeaponAudioEvent::ReloadMagazineOut); },ReloadDuration*P.MagazineOut,false);
 Timers.SetTimer(MagazineInTimer,[Mark]{ Mark(EVanthexWeaponAudioEvent::ReloadMagazineIn); },ReloadDuration*P.MagazineIn,false);
 Timers.SetTimer(BoltTimer,[Mark]{ Mark(EVanthexWeaponAudioEvent::ReloadBolt); },ReloadDuration*P.Bolt,false);
 Timers.SetTimer(ReloadTimer,this,&AWeaponBase::FinishReload,ReloadDuration,false);
}
void AWeaponBase::FinishReload()
{
 if(!HasAuthority()||!bReloading) return;
 const int32 Transfer=FMath::Min(MagazineSize-AmmoInMagazine,ReserveAmmo);
 SetMagazineAmmo(AmmoInMagazine+Transfer); SetReserveAmmo(ReserveAmmo-Transfer); bReloading=false;
 UVanthexWeaponAudioComponent::DispatchEvent(this,ActiveWeaponId,EVanthexWeaponAudioEvent::ReloadEnd,ReloadActionId);
}
void AWeaponBase::CancelReload()
{
 if(GetWorld()) { auto& T=GetWorld()->GetTimerManager(); T.ClearTimer(ReloadTimer); T.ClearTimer(MagazineOutTimer); T.ClearTimer(MagazineInTimer); T.ClearTimer(BoltTimer); }
 if(HasAuthority()&&bReloading) UVanthexWeaponAudioComponent::DispatchEvent(this,ActiveWeaponId,EVanthexWeaponAudioEvent::ReloadCancel,ReloadActionId);
 bReloading=false;
}
void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(AWeaponBase,ActiveWeaponId);
 DOREPLIFETIME_CONDITION(AWeaponBase,AmmoInMagazine,COND_OwnerOnly);
 DOREPLIFETIME_CONDITION(AWeaponBase,ReserveAmmo,COND_OwnerOnly);
 DOREPLIFETIME_CONDITION(AWeaponBase,bReloading,COND_OwnerOnly);
}
