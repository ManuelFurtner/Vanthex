#include "VanthexHUD.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexShiftDevice.h"
#include "VanthexMapLayout.h"
#include "EngineUtils.h"

#include "VanthexCharacter.h"
#include "VanthexPlayerController.h"
#include "VanthexGameState.h"
#include "VanthexPlayerState.h"
#include "VanthexTypes.h"
#include "HealthComponent.h"
#include "HitscanWeapon.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

namespace
{
const TCHAR* TeamLabel(EVanthexTeam Team)
{
	switch (Team)
	{
	case EVanthexTeam::Alpha: return TEXT("ALPHA");
	case EVanthexTeam::Bravo: return TEXT("BRAVO");
	default: return TEXT("NONE");
	}
}
}

void AVanthexHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const auto* SessionController=Cast<AVanthexPlayerController>(GetOwningPlayerController());
 if(SessionController && SessionController->IsSessionMenuOpen()) return;
 const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.5f;
	DrawLine(CenterX - 8.f, CenterY, CenterX + 8.f, CenterY, FLinearColor::White, 1.5f);
	DrawLine(CenterX, CenterY - 8.f, CenterX, CenterY + 8.f, FLinearColor::White, 1.5f);

	const APlayerController* PlayerController = GetOwningPlayerController();
	const AVanthexGameState* Match = GetWorld() ? GetWorld()->GetGameState<AVanthexGameState>() : nullptr;
	const AVanthexPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AVanthexPlayerState>() : nullptr;
	const AVanthexCharacter* Character = Cast<AVanthexCharacter>(GetOwningPawn());
	const UHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
	const AHitscanWeapon* Weapon = Character ? Character->GetWeapon() : nullptr;

	const int32 AlphaScore = Match ? Match->GetAlphaScore() : 0;
	const int32 BravoScore = Match ? Match->GetBravoScore() : 0;
	const int32 RoundIndex = Match ? Match->GetRoundIndex() : 0;
	const int32 Seconds = Match ? FMath::Max(0, FMath::CeilToInt(Match->GetTimeRemaining())) : 0;
	const EVanthexTeam Team = PlayerState ? PlayerState->GetTeam() : EVanthexTeam::None;
	const bool bDead = Health && Health->IsDead();
 if(Health && Health->GetHealth()<PreviousHealth) DamageFlashUntil=GetWorld()->GetTimeSeconds()+0.3f;
 if(Health) PreviousHealth=Health->GetHealth();
 if(GetWorld()->GetTimeSeconds()<DamageFlashUntil) { DrawRect(FLinearColor(0.8f,0,0,0.35f),0,0,Canvas->SizeX,15); DrawRect(FLinearColor(0.8f,0,0,0.35f),0,Canvas->SizeY-15,Canvas->SizeX,15); }
 if(SessionController && GetWorld()->GetTimeSeconds()<SessionController->HitMarkerUntil) {
  const auto Color=SessionController->bHitMarkerHead ? FLinearColor::Yellow : FLinearColor::White;
  for(float X:{-1.f,1.f}) for(float Y:{-1.f,1.f}) DrawLine(CenterX+X*10,CenterY+Y*10,CenterX+X*17,CenterY+Y*17,Color,2);
 }


	FString Status = FString::Printf(
		TEXT("ALPHA %d - %d BRAVO\nROUND %d\nTIME %d:%02d\nTEAM %s\nHP %s\nAMMO %s"),
		AlphaScore,
		BravoScore,
		RoundIndex,
		Seconds / 60,
		Seconds % 60,
		TeamLabel(Team),
		Health ? *FString::Printf(TEXT("%.0f"), Health->GetHealth()) : TEXT("--"),
		Weapon ? *FString::Printf(TEXT("%d/%d"), Weapon->GetAmmoInMagazine(), Weapon->GetMagazineSize()) : TEXT("--"));

	if (Weapon && Weapon->IsReloading())
	{
		Status += TEXT("\nRELOADING");
	}
	if (Character && Character->GetEquipment())
	{
		const auto* Equipment = Character->GetEquipment();
		Status += Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife
			? TEXT("\nKNIFE - LMB stab\n1 RIFLE / 3 KNIFE") : TEXT("\nRIFLE\n1 RIFLE / 3 KNIFE");
		if (Equipment->IsSwitching()) Status += TEXT("\nSWITCHING");
	}
	if (bDead)
	{
		Status += TEXT("\nELIMINATED - watching team until next round");
	}

	const TCHAR* CoreLabel = TEXT("CORE: CENTER");
	if (Match)
	{
		switch (Match->GetCoreReadout())
		{
		case EVanthexCoreReadout::Alpha: CoreLabel = TEXT("CORE: ALPHA"); break;
		case EVanthexCoreReadout::Bravo: CoreLabel = TEXT("CORE: BRAVO"); break;
		case EVanthexCoreReadout::Dropped: CoreLabel = TEXT("CORE: DROPPED"); break;
		case EVanthexCoreReadout::Thrown: CoreLabel = TEXT("CORE: THROWN"); break;
		case EVanthexCoreReadout::ExtractingA: CoreLabel = TEXT("EXTRACTING A"); break;
		case EVanthexCoreReadout::ExtractingB: CoreLabel = TEXT("EXTRACTING B"); break;
		case EVanthexCoreReadout::Extracted: CoreLabel = TEXT("CORE: EXTRACTED"); break;
		case EVanthexCoreReadout::Center:
		default: CoreLabel = TEXT("CORE: CENTER"); break;
		}
	}
	Status += FString::Printf(TEXT("\nOPERATOR: ARC\n%s"), CoreLabel);
	if (Match && (Match->GetCoreReadout() == EVanthexCoreReadout::ExtractingA || Match->GetCoreReadout() == EVanthexCoreReadout::ExtractingB))
	{
		Status += FString::Printf(TEXT("\n%d"), FMath::Max(1, FMath::CeilToInt(Match->GetExtractionRemaining())));
	}
	if (Character && Character->IsCarryingCore())
	{
		Status += TEXT("\nYOU HAVE THE CORE");
		Status += FString::Printf(TEXT("\nCORE CARRIED\n%s"), TeamLabel(Team));
	}

	if (!VanthexMap::IsGridV3(GetWorld())) Status += FString::Printf(TEXT("\nF: SHIFT  C: ARC remote hack (%.0fs)"), Character && Match ? FMath::Max(0.f, Character->AbilityReadyTime-Match->GetServerWorldTimeSeconds()) : 0.f);
 Status += TEXT("\nEsc: Session menu");
 Status += TEXT("\nWASD  Mouse  Shift sprint  Ctrl crouch  Space jump\nLMB fire  R reload  E pickup  G drop  Q throw  F5 match restart");

 if(Character) {
  for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) {
   if(It->Kind==3) Status+=FString::Printf(TEXT("\nGRID %d: %s"),It->Circuit+1,It->bPowered?TEXT("ONLINE"):TEXT("OFFLINE"));
   if(FVector::Dist(Character->GetActorLocation(),It->InteractionPoint(Character->GetActorLocation()))<300) {
    FString Prompt=It->Kind==3 ? FString::Printf(TEXT("F: %s power | Stay nearby 4 seconds | Hack %.0f%%"),It->bPowered?TEXT("DISABLE"):TEXT("RESTORE"),It->HackProgress*100) : It->Kind==2 ? (It->bPowered?TEXT("F: Call / operate lift"):TEXT("LIFT OFFLINE - restore sector power")) : TEXT("F: Interact");
    DrawText(Prompt,FLinearColor::Yellow,CenterX-250,CenterY+65,nullptr,1.15f);
   }
  }
 }
	DrawText(Status, bDead ? FLinearColor::Red : FLinearColor::White, 40.f, 40.f, nullptr, 1.2f);

	if (!Match)
	{
		return;
	}

	FString Banner;
	FLinearColor BannerColor = FLinearColor::Yellow;
	if (Match->GetPhase() == EVanthexRoundPhase::MatchEnded)
	{
		Banner = FString::Printf(TEXT("%s WINS MATCH"), TeamLabel(Match->GetMatchWinner()));
		BannerColor = FLinearColor(1.f, 0.85f, 0.2f);
	}
	else if (Match->GetPhase() == EVanthexRoundPhase::RoundEnding || Match->GetPhase() == EVanthexRoundPhase::Intermission)
	{
		if (Match->GetRoundWinner() == EVanthexTeam::None)
		{
			Banner = TEXT("ROUND DRAW");
		}
		else
		{
			Banner = FString::Printf(TEXT("%s WINS ROUND"), TeamLabel(Match->GetRoundWinner()));
		}
	}

	if (!Banner.IsEmpty())
	{
		DrawText(Banner, BannerColor, CenterX - 140.f, CenterY - 80.f, nullptr, 1.6f);
	}
}
