#include "VanthexBuildWeaponAudioCommandlet.h"
#include "Vanthex.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "VanthexWeaponAudioProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr const TCHAR* Root=TEXT("/Game/Vanthex/Audio/Weapons/");
bool Save(UObject* Asset)
{
 if(!Asset) return false;
 Asset->MarkPackageDirty();
 FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
 const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
 return UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Args);
}
template<typename T> T* Asset(const FString& Name,bool& Created)
{
 const FString Package=FString(Root)+Name;
 if(T* Existing=LoadObject<T>(nullptr,*(Package+TEXT(".")+Name),nullptr,LOAD_NoWarn)) { Created=false; return Existing; }
 T* New=NewObject<T>(CreatePackage(*Package),*Name,RF_Public|RF_Standalone|RF_Transactional);
 FAssetRegistryModule::AssetCreated(New); Created=true; return New;
}
USoundWave* Wave(const FString& Name,bool Replace)
{
 const FString PackageName=FString(Root)+Name;
 USoundWave* Existing=LoadObject<USoundWave>(nullptr,*(PackageName+TEXT(".")+Name),nullptr,LOAD_NoWarn);
 if(Existing && !Replace) return Existing;
 const FString File=FPaths::ProjectDir()/TEXT("AudioSource/Weapons")/(Name+TEXT(".wav"));
 if(!FPaths::FileExists(File)) { UE_LOG(LogVanthex,Error,TEXT("WEAPON_AUDIO missing %s; run generate_weapon_audio.py"),*File); return nullptr; }
 auto* Factory=NewObject<USoundFactory>(); Factory->bAutoCreateCue=false; Factory->SuppressImportDialogs(); bool Canceled=false;
 auto* Sound=Cast<USoundWave>(Factory->ImportObject(USoundWave::StaticClass(),Existing?Existing->GetOutermost():CreatePackage(*PackageName),*Name,
  RF_Public|RF_Standalone|RF_Transactional,File,nullptr,Canceled));
 if(!Sound || Canceled) return nullptr;
 Sound->bLooping=false; Sound->Volume=1.f; Sound->Pitch=1.f; Sound->SoundGroup=SOUNDGROUP_Effects;
 Sound->bBypassVolumeScaleForPriority=true; // Semantic danger priority must survive distance/occlusion gain.
 Sound->LoadingBehavior=ESoundWaveLoadingBehavior::ForceInline;
 Sound->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM); Sound->PostEditChange();
 if(!Existing) FAssetRegistryModule::AssetCreated(Sound);
 if(Sound->NumChannels!=1 || Sound->Duration<=0.f || Sound->Duration>.6f || !Save(Sound)) return nullptr;
 return Sound;
}
}
#endif

UVanthexBuildWeaponAudioCommandlet::UVanthexBuildWeaponAudioCommandlet()
{
 IsClient=false; IsEditor=true; IsServer=false; LogToConsole=true;
}

int32 UVanthexBuildWeaponAudioCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 const bool Replace=FParse::Param(*Params,TEXT("ReplacePrototypeAssets")); bool Created=false;
 auto* Profile=Asset<UVanthexWeaponAudioProfile>(TEXT("DA_WeaponAudio"),Created);
 const bool UpdateProfile=Created || Replace;
 auto* Attenuation=Asset<USoundAttenuation>(TEXT("SA_WeaponSpatial"),Created);
 if(Created || Replace) {
  auto& S=Attenuation->Attenuation;
  S.bSpatialize=true; S.SpatializationAlgorithm=SPATIALIZATION_Default;
  S.bAttenuate=false; // Route distance and obstruction are applied once by the server.
  S.bEnableOcclusion=false; S.bEnableListenerFocus=false; S.bEnableReverbSend=false;
  S.NonSpatializedRadiusStart=0.f; S.NonSpatializedRadiusEnd=0.f;
  if(!Save(Attenuation)) return 1;
 }
 auto* Concurrency=Asset<USoundConcurrency>(TEXT("SC_WeaponAudio"),Created);
 if(Created || Replace) {
  Concurrency->Concurrency.MaxCount=24;
  Concurrency->Concurrency.bLimitToOwner=false;
  Concurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew;
  Concurrency->Concurrency.VoiceStealReleaseTime=.005f;
  Concurrency->Concurrency.RetriggerTime=0.f;
  if(!Save(Concurrency)) return 1;
 }
 const TCHAR* Weapons[]={TEXT("Pistol"),TEXT("P250"),TEXT("MP5"),TEXT("MP7"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun"),TEXT("Knife")};
 const EVanthexWeaponAudioEvent Events[]={EVanthexWeaponAudioEvent::Shot,EVanthexWeaponAudioEvent::Mechanic,
  EVanthexWeaponAudioEvent::ReloadStart,EVanthexWeaponAudioEvent::ReloadMagazineOut,EVanthexWeaponAudioEvent::ReloadMagazineIn,
  EVanthexWeaponAudioEvent::ReloadBolt,EVanthexWeaponAudioEvent::ReloadEnd,EVanthexWeaponAudioEvent::DryFire,
  EVanthexWeaponAudioEvent::Draw,EVanthexWeaponAudioEvent::Holster,EVanthexWeaponAudioEvent::KnifeSwing};
 int32 Count=0;
 for(const TCHAR* Weapon:Weapons) {
  FVanthexWeaponSoundBank Bank;
  for(auto Event:Events) {
   const bool Knife=FCString::Strcmp(Weapon,TEXT("Knife"))==0;
   if(Knife && Event!=EVanthexWeaponAudioEvent::Draw && Event!=EVanthexWeaponAudioEvent::Holster && Event!=EVanthexWeaponAudioEvent::KnifeSwing) continue;
   if(!Knife && Event==EVanthexWeaponAudioEvent::KnifeSwing) continue;
   const FString EventName=StaticEnum<EVanthexWeaponAudioEvent>()->GetNameStringByValue(int64(Event));
   FVanthexWeaponSoundSet Set;
   Set.Volume=Event==EVanthexWeaponAudioEvent::Shot?.72f:(Event==EVanthexWeaponAudioEvent::Mechanic?.13f:.24f);
   for(int32 I=1; I<=(Event==EVanthexWeaponAudioEvent::Shot?3:1); ++I) {
    const FString Name=FString::Printf(TEXT("S_WA_%s_%s"),Weapon,*EventName)+(Event==EVanthexWeaponAudioEvent::Shot?FString::Printf(TEXT("_%02d"),I):FString());
    auto* Sound=Wave(Name,Replace); if(!Sound) return 1;
    Set.Variants.Add(Sound); ++Count;
   }
   Bank.Events.Add(Event,Set);
  }
  if(UpdateProfile) Profile->Weapons.Add(FName(Weapon),Bank);
 }
 for(int32 I=int32(EVanthexWeaponAudioEvent::PurchaseAccepted); I<=int32(EVanthexWeaponAudioEvent::RoundStart); ++I) {
  const auto Event=EVanthexWeaponAudioEvent(I);
  const FString EventName=StaticEnum<EVanthexWeaponAudioEvent>()->GetNameStringByValue(I);
  auto* Sound=Wave(TEXT("S_WA_UI_")+EventName,Replace); if(!Sound) return 1;
  FVanthexWeaponSoundSet Set; Set.Volume=.16f; Set.Variants.Add(Sound);
  if(UpdateProfile) Profile->UI.Add(Event,Set); ++Count;
 }
 auto* Tail=Wave(TEXT("S_WA_RoomTail"),Replace); if(!Tail) return 1; ++Count;
 FVanthexWeaponSoundSet Movement; Movement.Volume=.22f;
 for(int32 I=1; I<=3; ++I) {
  auto* Sound=Wave(FString::Printf(TEXT("S_WA_EquipmentMove_%02d"),I),Replace); if(!Sound) return 1;
  Movement.Variants.Add(Sound); ++Count;
 }
 if(UpdateProfile) Profile->Movement=Movement;
 if(UpdateProfile) { Profile->Attenuation=Attenuation; Profile->Concurrency=Concurrency; Profile->RoomTail=Tail; }
 if(!Save(Profile)) return 1;
 UE_LOG(LogVanthex,Display,TEXT("WEAPON_AUDIO_ASSETS PASS waves=%d weapons=%d profile=%s"),Count,Profile->Weapons.Num(),*Profile->GetPathName());
 return 0;
#else
 return 1;
#endif
}
