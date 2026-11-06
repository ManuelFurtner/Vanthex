#include "VanthexGameMode.h"

#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Vanthex.h"
#include "VanthexCharacter.h"
#include "VanthexDamage.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
FString PlaytestCell(const FVector& Location)
{
	constexpr double CellSize = 400.0;
	return FString::Printf(TEXT("%d:%d:%d"), FMath::FloorToInt(Location.X / CellSize),
		FMath::FloorToInt(Location.Y / CellSize), FMath::FloorToInt(Location.Z / CellSize));
}
}

void AVanthexGameMode::ResetPlaytestReport()
{
#if !UE_BUILD_SHIPPING
	bPlaytestReport = FParse::Param(FCommandLine::Get(), TEXT("VanthexPlaytestReport"));
#endif
	if (!bPlaytestReport) return;

	PlaytestStartedAt = GetWorld()->GetTimeSeconds();
	NextPlaytestSampleAt = PlaytestStartedAt;
	PeakPlayerSlots = 0;
	PlaytestSpawnTimes.Reset();
	PlaytestMetrics.Reset();
}

void AVanthexGameMode::RecordPlaytestSpawn(AVanthexCharacter* Pawn)
{
	if (bPlaytestReport)
	{
		PlaytestSpawnTimes.Add(Pawn, GetWorld()->GetTimeSeconds());
	}
}

void AVanthexGameMode::RecordPlaytestDamage(AVanthexCharacter* Victim, const FVanthexDamageRequest& Request,
	float AppliedHP, bool bKilled, bool bEnemy)
{
	if (!bPlaytestReport) return;

	if (bEnemy)
	{
		const FString Weapon = !Request.WeaponId.IsNone() ? Request.WeaponId.ToString()
			: Request.DamageKind == EVanthexDamageKind::Melee ? TEXT("Knife")
			: Request.DamageKind == EVanthexDamageKind::Explosion ? TEXT("Explosion") : TEXT("Unknown");
		PlaytestMetrics.FindOrAdd(TEXT("damage_hp,") + Weapon) += AppliedHP;
		if (bKilled) ++PlaytestMetrics.FindOrAdd(TEXT("kills,") + Weapon);
	}

	if (bKilled)
	{
		const FString Cell = PlaytestCell(Victim->GetActorLocation());
		++PlaytestMetrics.FindOrAdd(TEXT("deaths,") + Cell);
		const double* SpawnTime = PlaytestSpawnTimes.Find(Victim);
		if (SpawnTime && GetWorld()->GetTimeSeconds() - *SpawnTime <= 5.0)
		{
			++PlaytestMetrics.FindOrAdd(TEXT("deaths_within_5s_of_spawn,") + Cell);
		}
	}
}

void AVanthexGameMode::SamplePlaytestActivity()
{
	if (!bPlaytestReport) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextPlaytestSampleAt) return;
	NextPlaytestSampleAt = Now + 1.0;

	int32 PlayerSlots = 0;
	for (const AVanthexCharacter* Pawn : Combatants)
	{
		if (!IsValid(Pawn)) continue;
		if (Pawn->IsPlayerControlled()) ++PlayerSlots;
		if (Pawn->GetHealthComponent()->IsDead()) continue;

		++PlaytestMetrics.FindOrAdd(TEXT("position_samples,") + PlaytestCell(Pawn->GetActorLocation()));
		if (const AWeaponBase* Weapon = Pawn->GetWeapon())
		{
			++PlaytestMetrics.FindOrAdd(TEXT("equipped_samples,") + Weapon->GetWeaponId().ToString());
		}
	}
	PeakPlayerSlots = FMath::Max(PeakPlayerSlots, PlayerSlots);
}

void AVanthexGameMode::SavePlaytestReport() const
{
	if (!bPlaytestReport) return;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Playtests"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString File = FPaths::Combine(Directory, FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".csv"));
	FString Csv = FString::Printf(TEXT("metric,key,value\nmap,%s,1\nmode,%s,1\nscore,alpha,%d\nscore,bravo,%d\n")
		TEXT("match,duration_seconds,%.2f\nmatch,peak_player_controlled_slots,%d\n"),
		*GetWorld()->GetMapName(), IsCTF() ? TEXT("CTF") : TEXT("Core"), AlphaScore, BravoScore,
		GetWorld()->GetTimeSeconds() - PlaytestStartedAt, PeakPlayerSlots);

	TArray<FString> Keys;
	PlaytestMetrics.GetKeys(Keys);
	Keys.Sort();
	for (const FString& Key : Keys)
	{
		Csv += FString::Printf(TEXT("%s,%.2f\n"), *Key, PlaytestMetrics.FindChecked(Key));
	}
	if (FFileHelper::SaveStringToFile(Csv, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogVanthex, Display, TEXT("Playtest report saved to %s"), *File);
	}
	else
	{
		UE_LOG(LogVanthex, Warning, TEXT("Could not write playtest report to %s"), *File);
	}
}
