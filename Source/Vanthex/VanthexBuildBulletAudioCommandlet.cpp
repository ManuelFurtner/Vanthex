#include "VanthexBuildBulletAudioCommandlet.h"

#include "Vanthex.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#include "VanthexBulletAudioProfile.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "GameplayTagContainer.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UVanthexBuildBulletAudioCommandlet::UVanthexBuildBulletAudioCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace VanthexBulletAudioAssets
{
constexpr const TCHAR* AssetRoot = TEXT("/Game/Vanthex/Audio/Bullet/");

bool Save(UObject* Asset)
{
	if (!Asset) return false;
	Asset->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
	UE_LOG(LogVanthex, Display, TEXT("BULLET_AUDIO_ASSET %s %s"), bSaved ? TEXT("SAVED") : TEXT("FAILED"), *Asset->GetPathName());
	return bSaved;
}

template <typename T>
T* GetAsset(const FString& Name, bool& bCreated)
{
	const FString PackageName = FString(AssetRoot) + Name;
	const FString ObjectPath = PackageName + TEXT(".") + Name;
	if (T* Existing = LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn))
	{
		bCreated = false;
		return Existing;
	}
	UPackage* Package = CreatePackage(*PackageName);
	T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	bCreated = true;
	return Asset;
}

USoundWave* ImportWave(const FString& Name, bool bReplace)
{
	const FString PackageName = FString(AssetRoot) + Name;
	const FString ObjectPath = PackageName + TEXT(".") + Name;
	USoundWave* Existing = LoadObject<USoundWave>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
	if (Existing && !bReplace) return Existing;
	const FString Source = FPaths::Combine(FPaths::ProjectDir(), TEXT("AudioSource/Bullet"), Name + TEXT(".wav"));
	if (!FPaths::FileExists(Source))
	{
		UE_LOG(LogVanthex, Error, TEXT("BULLET_AUDIO_ASSET source missing: %s; run AudioSource/Bullet/generate_bullet_audio.py"), *Source);
		return nullptr;
	}
	UPackage* Package = Existing ? Existing->GetOutermost() : CreatePackage(*PackageName);
	USoundFactory* Factory = NewObject<USoundFactory>();
	Factory->bAutoCreateCue = false;
	Factory->SuppressImportDialogs();
	bool bCanceled = false;
	USoundWave* Wave = Cast<USoundWave>(Factory->ImportObject(USoundWave::StaticClass(), Package, *Name,
		RF_Public | RF_Standalone | RF_Transactional, Source, nullptr, bCanceled));
	if (!Wave || bCanceled) return nullptr;
	Wave->bLooping = false;
	Wave->Volume = 1.f;
	Wave->Pitch = 1.f;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->LoadingBehavior = ESoundWaveLoadingBehavior::ForceInline;
	Wave->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM);
	Wave->PostEditChange();
	if (!Existing) FAssetRegistryModule::AssetCreated(Wave);
	if (Wave->NumChannels != 1 || Wave->Duration <= 0.f || Wave->Duration > 0.5f)
	{
		UE_LOG(LogVanthex, Error, TEXT("BULLET_AUDIO_ASSET invalid source format %s channels=%d duration=%.3f"), *Name, Wave->NumChannels, Wave->Duration);
		return nullptr;
	}
	return Save(Wave) ? Wave : nullptr;
}

FVanthexBulletSoundSet SoundSet(const TArray<TObjectPtr<USoundBase>>& Variants, float Volume)
{
	FVanthexBulletSoundSet Result;
	Result.Variants = Variants;
	Result.Volume = Volume;
	Result.PitchMin = 0.97f;
	Result.PitchMax = 1.03f;
	return Result;
}
}
#endif

int32 UVanthexBuildBulletAudioCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace VanthexBulletAudioAssets;
	const bool bReplace = FParse::Param(*Params, TEXT("ReplacePrototypeAssets"));
	TMap<FName, TArray<TObjectPtr<USoundBase>>> Families;
	const TPair<const TCHAR*, int32> Definitions[] = {
		{TEXT("Whiz"), 3}, {TEXT("Flyby"), 2}, {TEXT("Crack"), 2},
		{TEXT("Concrete"), 3}, {TEXT("Steel"), 3}, {TEXT("MetalGrate"), 3}
	};
	for (const auto& Definition : Definitions)
	{
		for (int32 Index = 1; Index <= Definition.Value; ++Index)
		{
			const FString Name = FString::Printf(TEXT("S_Bullet_%s_%02d"), Definition.Key, Index);
			USoundWave* Wave = ImportWave(Name, bReplace);
			if (!Wave) return 1;
			Families.FindOrAdd(FName(Definition.Key)).Add(Wave);
		}
	}

	bool bCreated = false;
	USoundAttenuation* Attenuation = GetAsset<USoundAttenuation>(TEXT("SA_BulletSpatial"), bCreated);
	if (bCreated || bReplace)
	{
		auto& Settings = Attenuation->Attenuation;
		Settings.bSpatialize = true;
		Settings.SpatializationAlgorithm = SPATIALIZATION_Default;
		Settings.bAttenuate = true;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
		Settings.FalloffDistance = 300.f;
		Settings.bEnableListenerFocus = false;
		Settings.bEnableOcclusion = false; // Server performs one event visibility test, no audio-rate traces.
		Settings.bEnableReverbSend = false;
		Settings.NonSpatializedRadiusStart = 0.f;
		Settings.NonSpatializedRadiusEnd = 0.f;
		if (!Save(Attenuation)) return 1;
	}
	USoundConcurrency* Concurrency = GetAsset<USoundConcurrency>(TEXT("SC_BulletAudio"), bCreated);
	if (bCreated || bReplace)
	{
		Concurrency->Concurrency.MaxCount = 8;
		Concurrency->Concurrency.bLimitToOwner = true;
		Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopQuietest;
		Concurrency->Concurrency.RetriggerTime = 0.f;
		Concurrency->Concurrency.VoiceStealReleaseTime = 0.01f;
		if (!Save(Concurrency)) return 1;
	}

	UVanthexBulletAudioProfile* Profile = GetAsset<UVanthexBulletAudioProfile>(TEXT("DA_BulletAudio"), bCreated);
	if (bCreated || bReplace)
	{
		Profile->Events.Reset();
		Profile->Impacts.Reset();
		Profile->Attenuation = Attenuation;
		Profile->Concurrency = Concurrency;
		for (const TCHAR* Event : {TEXT("Bullet.NearMiss.Close"), TEXT("Bullet.NearMiss.Medium"), TEXT("Bullet.NearMiss.Far")})
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Event));
			if (!Tag.IsValid()) return 1;
			Profile->Events.Add(Tag, SoundSet(Families.FindChecked(TEXT("Whiz")), 0.45f));
		}
		Profile->Events.Add(FGameplayTag::RequestGameplayTag(TEXT("Bullet.Flyby")), SoundSet(Families.FindChecked(TEXT("Flyby")), 0.30f));
		Profile->Events.Add(FGameplayTag::RequestGameplayTag(TEXT("Bullet.Crack")), SoundSet(Families.FindChecked(TEXT("Crack")), 0.18f));
		Profile->Events.Add(FGameplayTag::RequestGameplayTag(TEXT("Bullet.Impact.Nearby")), SoundSet(Families.FindChecked(TEXT("Concrete")), 0.32f));
		for (const TCHAR* Surface : {TEXT("Concrete"), TEXT("Steel"), TEXT("MetalGrate")})
		{
			Profile->Impacts.Add(FName(Surface), SoundSet(Families.FindChecked(FName(Surface)), 0.32f));
		}
		if (!Save(Profile)) return 1;
	}
	UE_LOG(LogVanthex, Display, TEXT("BULLET_AUDIO_ASSETS PASS 16 mono waves, 6 events, 3 impact surfaces, attenuation and max8 concurrency; profile=%s"), *Profile->GetPathName());
	return 0;
#else
	UE_LOG(LogVanthex, Error, TEXT("Bullet audio asset generation requires the editor target."));
	return 1;
#endif
}
