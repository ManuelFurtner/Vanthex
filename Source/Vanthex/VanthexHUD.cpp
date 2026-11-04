#include "VanthexHUD.h"
#include "VanthexEquipmentComponent.h"
#include "VanthexGadgetComponent.h"
#include "VanthexGadgetActor.h"
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
#include "VanthexWeaponPresentation.h"
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

	const APlayerController* PlayerController = GetOwningPlayerController();
	const AVanthexGameState* Match = GetWorld() ? GetWorld()->GetGameState<AVanthexGameState>() : nullptr;
	const AVanthexPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AVanthexPlayerState>() : nullptr;
	const AVanthexCharacter* Character = Cast<AVanthexCharacter>(GetOwningPawn());
	const UHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
	const AHitscanWeapon* Weapon = Character ? Character->GetWeapon() : nullptr;
 const float AimBlend=Character?Character->GetAimBlend():0.f;
 if(AimBlend<.08f) {
  DrawLine(CenterX-8,CenterY,CenterX+8,CenterY,FLinearColor::White,1.5f);
  DrawLine(CenterX,CenterY-8,CenterX,CenterY+8,FLinearColor::White,1.5f);
 } else if(Weapon) {
  const auto Sight=VanthexWeaponPresentation::Aim(Weapon->GetWeaponId());
  if(Sight.Sight==EVanthexSight::Reflex&&AimBlend>.8f) {
   // Collimated dot on the clear, physically aligned 3D reflex lens.
   const float Alpha=FMath::SmoothStep(.8f,1.f,AimBlend);
   DrawRect(FLinearColor(.05f,.01f,0,.55f*Alpha),CenterX-2,CenterY-2,4,4);
   DrawRect(FLinearColor(1.f,.12f,.025f,Alpha),CenterX-1,CenterY-1,2,2);
  } else if(Sight.Sight==EVanthexSight::Scope&&AimBlend>.72f) {
   // One camera projection, with a clear circular ocular and engraved reticle.
   const float Alpha=FMath::SmoothStep(.72f,.94f,AimBlend),Radius=Canvas->SizeY*.39f;
   const FLinearColor Black(.004f,.006f,.008f,Alpha);
   for(float Y=0;Y<Canvas->SizeY;Y+=3.f) {
    const float DY=Y+1.5f-CenterY;
    const float Half=FMath::Abs(DY)<Radius?FMath::Sqrt(FMath::Max(0.f,Radius*Radius-DY*DY)):0.f;
    DrawRect(Black,0,Y,CenterX-Half,3.f);
    DrawRect(Black,CenterX+Half,Y,Canvas->SizeX-CenterX-Half,3.f);
   }
   for(int32 I=0;I<96;++I) {
    const float A=UE_TWO_PI*I/96.f,B=UE_TWO_PI*(I+1)/96.f;
    DrawLine(CenterX+FMath::Cos(A)*Radius,CenterY+FMath::Sin(A)*Radius,
     CenterX+FMath::Cos(B)*Radius,CenterY+FMath::Sin(B)*Radius,FLinearColor(.035f,.043f,.048f,Alpha),10.f);
   }
   const FLinearColor Reticle(.015f,.02f,.022f,Alpha);
   DrawLine(CenterX-Radius+6,CenterY,CenterX-4,CenterY,Reticle,1.f);
   DrawLine(CenterX+4,CenterY,CenterX+Radius-6,CenterY,Reticle,1.f);
   DrawLine(CenterX,CenterY-Radius+6,CenterX,CenterY-4,Reticle,1.f);
   DrawLine(CenterX,CenterY+4,CenterX,CenterY+Radius-6,Reticle,1.f);
   for(int32 I=-3;I<=3;++I) if(I) {
    DrawLine(CenterX+I*Radius*.15f,CenterY-4,CenterX+I*Radius*.15f,CenterY+4,Reticle,1.f);
    DrawLine(CenterX-4,CenterY+I*Radius*.15f,CenterX+4,CenterY+I*Radius*.15f,Reticle,1.f);
   }
   if(Weapon->GetWeaponId()==TEXT("AUG")) for(int32 I=0;I<32;++I) {
    const float A=UE_TWO_PI*I/32.f,B=UE_TWO_PI*(I+1)/32.f;
    DrawLine(CenterX+FMath::Cos(A)*12,CenterY+FMath::Sin(A)*12,CenterX+FMath::Cos(B)*12,CenterY+FMath::Sin(B)*12,Reticle,1.5f);
   }
   DrawRect(FLinearColor(.75f,.045f,.015f,Alpha),CenterX-1,CenterY-1,2,2);
  }
 }

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
		Weapon ? *FString::Printf(TEXT("%d / %d"), Weapon->GetAmmoInMagazine(), Weapon->GetReserveAmmo()) : TEXT("--"));
	if (PlayerState) Status += FString::Printf(TEXT("\n%d CR  |  ARMOR %.0f\nB: Ausruestung"), PlayerState->GetEconomy().Credits, PlayerState->GetArmorPoints());
 if(PlayerState){const auto& Slots=PlayerState->GetEconomy().Gadgets;Status+=FString::Printf(TEXT("\n4 %s / 5 %s"),Slots.IsValidIndex(0)?*Slots[0].ToString():TEXT("--"),Slots.IsValidIndex(1)?*Slots[1].ToString():TEXT("--"));}
 if(Character&&Character->bIsCrouched)Status+=TEXT("\nGEDUCKT - Strg loslassen zum Aufstehen");
 if(Character&&Character->GetGadgets()->IsJammed())Status+=TEXT("\nEMP - Elektronik gestoert");
	if (Match && Match->GetPhase() == EVanthexRoundPhase::BuyPhase) Status += TEXT("\nKAUFPHASE - am Spawn vorbereiten");

	if (Weapon && Weapon->IsReloading())
	{
		Status += TEXT("\nRELOADING");
	}
	if (Character && Character->GetEquipment())
	{
		const auto* Equipment = Character->GetEquipment();
		Status += Equipment->GetSelectedSlot() == EVanthexWeaponSlot::Knife ? TEXT("\nKNIFE - LMB stab") : FString::Printf(TEXT("\n%s"), Weapon ? *Weapon->GetWeaponId().ToString() : TEXT("--"));
		Status += TEXT("\n1 PRIMARY / 2 PISTOL / 3 KNIFE");
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
 Status += TEXT("\nWASD  Mouse  Shift sprint  Ctrl crouch  Space jump\nLMB fire  RMB hold: sights  R reload\nE pickup  G drop  Q throw  F5 match restart");

 if(Character) {
  for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) {
   if(It->Kind==3) Status+=FString::Printf(TEXT("\nGRID %d: %s"),It->Circuit+1,It->bPowered?TEXT("ONLINE"):TEXT("OFFLINE"));
   if(FVector::Dist(Character->GetActorLocation(),It->InteractionPoint(Character->GetActorLocation()))<300) {
    FString Prompt=It->Kind==3 ? FString::Printf(TEXT("F: %s power | Stay nearby 4 seconds | Hack %.0f%%"),It->bPowered?TEXT("DISABLE"):TEXT("RESTORE"),It->HackProgress*100) : It->Kind==2 ? (It->bPowered?TEXT("F: Call / operate lift"):TEXT("LIFT OFFLINE - restore sector power")) : TEXT("F: Interact");
    if(It->IsEMPDisabled())Prompt=TEXT("EMP - Geraet voruebergehend gestoert");
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
 if(Character&&!bDead){
  const auto* Gadgets=Character->GetGadgets();
  if(!Gadgets->IsJammed())for(const auto& Mark:Gadgets->GetMarks())if(Mark.Expires>Gadgets->GetServerTime()){
   FVector2D Screen;
   if(PlayerController->ProjectWorldLocationToScreen(Mark.Position,Screen)&&Screen.X>=0&&Screen.Y>=0&&Screen.X<Canvas->SizeX&&Screen.Y<Canvas->SizeY){
    const FLinearColor Tint(.1f,.85f,1.f,.8f);
    DrawLine(Screen.X-9,Screen.Y,Screen.X,Screen.Y-9,Tint,2);DrawLine(Screen.X,Screen.Y-9,Screen.X+9,Screen.Y,Tint,2);
    DrawLine(Screen.X+9,Screen.Y,Screen.X,Screen.Y+9,Tint,2);DrawLine(Screen.X,Screen.Y+9,Screen.X-9,Screen.Y,Tint,2);
    DrawText(TEXT("SENSOR"),Tint,Screen.X+12,Screen.Y-6,nullptr,.8f);
   }
  }
  const FVector Eye=Character->GetPawnViewLocation();
  for(TActorIterator<AVanthexGadgetActor> It(GetWorld());It;++It)if(It->IsSmokeActive()){
   const FVector Center=It->GetActorLocation()+FVector(0,0,140);
   if(FVector::DistSquared(Eye,Center)>=FMath::Square(It->GetEffectRadius()))continue;
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(SmokeInterior),false,Character);Q.AddIgnoredActor(*It);
   if(!GetWorld()->LineTraceSingleByChannel(H,Eye,Center,ECC_Visibility,Q)){DrawRect(FLinearColor(.19f,.22f,.24f,.985f),0,0,Canvas->SizeX,Canvas->SizeY);break;}
  }
  const float Flash=Gadgets->GetFlashOpacity();
  if(Flash>0.f)DrawRect(FLinearColor(.94f,.96f,1.f,Flash),0,0,Canvas->SizeX,Canvas->SizeY);
 }
}
