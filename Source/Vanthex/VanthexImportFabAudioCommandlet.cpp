#include "VanthexImportFabAudioCommandlet.h"
#include "Vanthex.h"
#if WITH_EDITOR
#include "VanthexWeaponAudioProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "Audio.h"
#include "HAL/FileManager.h"

namespace
{
bool SaveImportedAudio(UObject* Object)
{
 Object->SetFlags(RF_Public|RF_Standalone);Object->MarkPackageDirty();
 FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(Object->GetPackage(),Object,*FPackageName::LongPackageNameToFilename(Object->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),A);
}
USoundWave* ConvertFabWave(const FString& Source,const FString& Destination,float Rate,TArray<FString>& Report)
{
 auto* Input=LoadObject<USoundWave>(nullptr,*Source);if(!Input)return nullptr;
 TArray<uint8> Raw;uint32 SampleRate=0;uint16 Channels=0;
 if(!Input->GetImportedSoundWaveData(Raw,SampleRate,Channels)||!SampleRate||!Channels||Raw.Num()<4)return nullptr;
 const int32 Frames=Raw.Num()/(2*Channels);const auto* PCM=reinterpret_cast<const int16*>(Raw.GetData());
 // All routed world sources are mono. Resampling gives separate weapon signatures
 // from the supplied category recordings; it does not claim authentic firearm recordings.
 const float Step=float(SampleRate)/48000.f*Rate;
 const int32 OutputFrames=FMath::Min(int32(Frames/Step),int32(.55f*48000));
 if(OutputFrames<8)return nullptr;
 TArray<float> Values;Values.SetNum(OutputFrames);float Peak=0;
 for(int32 I=0;I<OutputFrames;++I){
  const float Position=I*Step;const int32 N=FMath::Min(int32(Position),Frames-1),Next=FMath::Min(N+1,Frames-1);
  float A=0,B=0;for(int32 C=0;C<Channels;++C){A+=PCM[N*Channels+C]/32768.f;B+=PCM[Next*Channels+C]/32768.f;}
  float V=FMath::Lerp(A,B,Position-N)/Channels;
  const float Fade=FMath::Min(1.f,float(OutputFrames-1-I)/1920.f);V*=Fade;
  Values[I]=V;Peak=FMath::Max(Peak,FMath::Abs(V));
 }
 if(Peak<.00001f)return nullptr;
 TArray<int16> OutPCM;OutPCM.SetNum(OutputFrames);
 for(int32 I=0;I<OutputFrames;++I)OutPCM[I]=int16(FMath::RoundToInt(FMath::Clamp(Values[I]*(.49f/Peak),-.49f,.49f)*32767.f));
 TArray<uint8> Wave;SerializeWaveFile(Wave,reinterpret_cast<const uint8*>(OutPCM.GetData()),OutPCM.Num()*2,1,48000);
 const FString Name=TEXT("S_Fab_")+Destination;
 const FString File=FPaths::ProjectSavedDir()/TEXT("ImportedFabAudio")/(Name+TEXT(".wav"));
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
 if(!FFileHelper::SaveArrayToFile(Wave,*File))return nullptr;
 const FString PackagePath=TEXT("/Game/Vanthex/Imported/WeaponAudio/")+Name;
 auto* Package=CreatePackage(*PackagePath);Package->FullyLoad();
 auto* Factory=NewObject<USoundFactory>();Factory->bAutoCreateCue=false;Factory->SuppressImportDialogs();bool Cancel=false;
 auto* Sound=Cast<USoundWave>(Factory->ImportObject(USoundWave::StaticClass(),Package,*Name,RF_Public|RF_Standalone,File,nullptr,Cancel));
 if(!Sound||Cancel)return nullptr;
 Sound->Volume=1.f;Sound->Pitch=1.f;Sound->bLooping=false;Sound->SoundGroup=SOUNDGROUP_Effects;
 Sound->LoadingBehavior=ESoundWaveLoadingBehavior::ForceInline;Sound->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM);
 Sound->bBypassVolumeScaleForPriority=true;Sound->PostEditChange();FAssetRegistryModule::AssetCreated(Sound);
 if(!SaveImportedAudio(Sound))return nullptr;
 Report.Add(FString::Printf(TEXT("PASS %s -> %s sourceChannels=%u mono48000 frames=%d peak<=0.49 rate=%.3f"),*Source,*PackagePath,Channels,OutputFrames,Rate));return Sound;
}
}
#endif
UVanthexImportFabAudioCommandlet::UVanthexImportFabAudioCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVanthexImportFabAudioCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 auto* Base=LoadObject<UVanthexWeaponAudioProfile>(nullptr,TEXT("/Game/Vanthex/Audio/Weapons/DA_WeaponAudio.DA_WeaponAudio"));if(!Base)return 1;
 const FString Name=TEXT("DA_WeaponAudio_Fab"),Path=TEXT("/Game/Vanthex/Imported/WeaponAudio/")+Name;
 auto* Package=CreatePackage(*Path);Package->FullyLoad();
 if(auto* Existing=FindObject<UObject>(Package,*Name)) {
  Existing->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
  Existing->ClearFlags(RF_Public|RF_Standalone);
 }
 auto* Profile=DuplicateObject<UVanthexWeaponAudioProfile>(Base,Package,*Name);if(!Profile)return 1;
 TArray<FString> Report;int32 Count=0;bool OK=true;
 using E=EVanthexWeaponAudioEvent;
 for(const TCHAR* Weapon:{TEXT("Pistol"),TEXT("P250"),TEXT("M4"),TEXT("AK"),TEXT("AUG"),TEXT("DMR"),TEXT("Shotgun")}){
  const FName Id(Weapon);const bool Pistol=Id==TEXT("Pistol")||Id==TEXT("P250"),Shotgun=Id==TEXT("Shotgun");
  const FString Category=Pistol?TEXT("Handgun"):Shotgun?TEXT("Shotgun"):TEXT("AssaultRifle");
  const FString Prefix=Pistol?TEXT("hgn"):Shotgun?TEXT("sgn"):TEXT("ar");
  const float Rate=Id==TEXT("AK")?.90f:Id==TEXT("DMR")?.84f:Id==TEXT("AUG")?1.06f:Id==TEXT("P250")?.94f:1.f;
  auto* Bank=Profile->Weapons.Find(Id);if(!Bank){OK=false;continue;}
  auto Set=[&](E Event,const FString& Folder,const TArray<FString>& Suffixes){
   FVanthexWeaponSoundSet Sounds;Sounds.Volume=Event==E::Shot?.72f:.24f;
   for(int32 I=0;I<Suffixes.Num();++I){
    const FString WaveName=TEXT("sfx_wpn_")+Prefix+TEXT("_")+Suffixes[I];
    const FString Source=TEXT("/Game/FreeWeaponSFX/Wav/")+Category+TEXT("/")+Folder+TEXT("/")+WaveName+TEXT(".")+WaveName;
    const FString Out=FString(Weapon)+TEXT("_")+StaticEnum<E>()->GetNameStringByValue(int64(Event))+FString::FromInt(I+1);
    auto* W=ConvertFabWave(Source,Out,Event==E::Shot?Rate:1.f,Report);if(!W){OK=false;continue;}Sounds.Variants.Add(W);++Count;
   }
   if(Sounds.Variants.Num()==Suffixes.Num())Bank->Events.Add(Event,Sounds);
  };
  Set(E::Shot,TEXT("Gunshots"),{TEXT("gunshot_01"),TEXT("gunshot_02"),TEXT("gunshot_03")});
  Set(E::Draw,TEXT("Foley"),{TEXT("foley_equip_01")});Set(E::Holster,TEXT("Foley"),{TEXT("foley_equip_02")});
  if(Shotgun){
   Set(E::ReloadStart,TEXT("Reload"),{TEXT("reload_01_charge")});Set(E::ReloadMagazineOut,TEXT("Reload"),{TEXT("reload_02_pumpBack")});
   Set(E::ReloadMagazineIn,TEXT("Reload"),{TEXT("reload_03_shellDraw")});Set(E::ReloadBolt,TEXT("Reload"),{TEXT("reload_05_pumpForward")});
  }else if(Pistol){
   Set(E::ReloadStart,TEXT("Reload"),{TEXT("reload_01_slideLock")});Set(E::ReloadMagazineOut,TEXT("Reload"),{TEXT("reload_02_magOut")});
   Set(E::ReloadMagazineIn,TEXT("Reload"),{TEXT("reload_04_magIn")});Set(E::ReloadBolt,TEXT("Reload"),{TEXT("reload_05_charge")});
  }else{
   Set(E::ReloadStart,TEXT("Reload"),{TEXT("reload_02_magDraw")});Set(E::ReloadMagazineOut,TEXT("Reload"),{TEXT("reload_01_magOut")});
   Set(E::ReloadMagazineIn,TEXT("Reload"),{TEXT("reload_03_magIn")});Set(E::ReloadBolt,TEXT("Reload"),{TEXT("reload_04_charge")});
  }
 }
 OK&=SaveImportedAudio(Profile);FAssetRegistryModule::AssetCreated(Profile);
 Report.Insert(FString::Printf(TEXT("OVERALL %s\nCONVERTED_WAVES %d\nOptional local profile; retain Fab license restrictions. MP5/MP7/Knife and missing events keep project-owned audio."),OK?TEXT("PASS"):TEXT("FAIL"),Count),0);
 FFileHelper::SaveStringArrayToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("FabAudioImport.txt")));
 return OK?0:1;
#else
 return 1;
#endif
}
