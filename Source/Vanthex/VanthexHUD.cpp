#include "VanthexHUD.h"
#include "VanthexLocalSettings.h"
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
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "VanthexCore.h"
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

AVanthexHUD::AVanthexHUD()
{
 // Fonts are client-only assets and may be stripped from a dedicated server cook.
 if (IsRunningDedicatedServer()) return;
 static ConstructorHelpers::FObjectFinder<UFont> FontAsset(TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
 DisplayFont=FontAsset.Object;
}

void AVanthexHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !bShowHUD)
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
 if(Character&&Health&&!Health->IsDead()){
  const FVector Eye=Character->GetPawnViewLocation();
  for(TActorIterator<AVanthexGadgetActor> It(GetWorld());It;++It)if(It->IsSmokeActive()){
   const FVector Center=It->GetActorLocation()+FVector(0,0,140);
   if(FVector::DistSquared(Eye,Center)>=FMath::Square(It->GetEffectRadius()))continue;
   FHitResult SmokeHit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SmokeInterior),false,Character);Q.AddIgnoredActor(*It);
   if(!GetWorld()->LineTraceSingleByChannel(SmokeHit,Eye,Center,ECC_Visibility,Q)){DrawRect(FLinearColor(.19f,.22f,.24f,.985f),0,0,Canvas->SizeX,Canvas->SizeY);break;}
  }
 }
 if(AimBlend<.08f&&Health&&!Health->IsDead()) {
  const auto Crosshair=UVanthexLocalSettings::Get()->GetCrosshairStyle();
  for(const auto& R:Crosshair.Rects()) DrawRect(Crosshair.Color,CenterX+R.X,CenterY+R.Y,R.Z,R.W);
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


 // Layout uses a 1280 x 720 reference and keeps every panel inside the safe area.
 const float UI=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f);
 const float W=Canvas->SizeX/UI,H=Canvas->SizeY/UI,CX=W*.5f;
 const FLinearColor Ink(.012f,.021f,.03f,.88f),Edge(.22f,.32f,.39f,.5f);
 const FLinearColor White(.94f,.97f,1.f),Muted(.53f,.64f,.70f),Cyan(.12f,.87f,.96f),Amber(1.f,.64f,.24f),Red(1.f,.23f,.26f);
 const FLinearColor Accent=Team==EVanthexTeam::Bravo?Amber:Cyan;
 UFont* Font=DisplayFont?DisplayFont.Get():GEngine->GetMediumFont();
 auto Rect=[&](float X,float Y,float Width,float Height,FLinearColor Color){DrawRect(Color,X*UI,Y*UI,Width*UI,Height*UI);};
 auto Text=[&](const FString& Value,float X,float Y,float Size,FLinearColor Color,bool Center=false){
  const float Scale=Size/FMath::Max(1,Font->GetMaxCharHeight());
  float TW=0,TH=0;GetTextSize(Value,TW,TH,Font,Scale*UI);
  DrawText(Value,Color,X*UI-(Center?TW*.5f:0),Y*UI,Font,Scale*UI);
 };
 auto Panel=[&](float X,float Y,float Width,float Height,FLinearColor Stripe){
  Rect(X,Y,Width,Height,Ink);Rect(X,Y,Width,1,Edge);Rect(X,Y,3,Height,Stripe);
 };
 auto Bar=[&](float X,float Y,float Width,float Height,float Fraction,FLinearColor Color){
  Rect(X,Y,Width,Height,FLinearColor(.14f,.20f,.24f,.8f));
  Rect(X,Y,Width*FMath::Clamp(Fraction,0.f,1.f),Height,Color);
 };
 const bool Buy=Match&&Match->GetPhase()==EVanthexRoundPhase::BuyPhase;
 const auto* Equipment=Character?Character->GetEquipment():nullptr;
 const bool Knife=Equipment&&Equipment->GetSelectedSlot()==EVanthexWeaponSlot::Knife;

 // Scoreboard: fixed widths prevent the clock and scores from jumping as they change.
 Panel(CX-224,24,448,65,Accent);
 Rect(CX-62,24,124,65,FLinearColor(.025f,.045f,.06f,.9f));
 Text(TEXT("ALPHA"),CX-184,34,13,Cyan);
 Text(FString::FromInt(AlphaScore),CX-88,37,31,White,true);
 Text(TEXT("BRAVO"),CX+137,34,13,Amber);
 Text(FString::FromInt(BravoScore),CX+88,37,31,White,true);
 Text(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60),CX,43,28,Seconds<=15&&!Buy?Red:White,true);
 Text(Buy?TEXT("KAUFPHASE"):FString::Printf(TEXT("RUNDE %02d"),RoundIndex),CX,29,11,Buy?Amber:Muted,true);
 int32 AliveA=0,AliveB=0,CountA=0,CountB=0;
 if(Match)for(const auto& Entry:Match->PlayerArray)if(const auto* P=Cast<AVanthexPlayerState>(Entry)){
  if(P->GetTeam()==EVanthexTeam::Alpha){++CountA;if(P->IsAlive())++AliveA;}
  if(P->GetTeam()==EVanthexTeam::Bravo){++CountB;if(P->IsAlive())++AliveB;}
 }
 for(int32 I=0;I<FMath::Min(CountA,6);++I)Rect(CX-184+I*15,65,10,4,I<AliveA?Cyan:Edge);
 for(int32 I=0;I<FMath::Min(CountB,6);++I)Rect(CX+137+I*15,65,10,4,I<AliveB?Amber:Edge);
 Text(TEXT("V A N T H E X"),28,26,18,White);
 Text(FString::Printf(TEXT("%s  /  ARC"),TeamLabel(Team)),28,51,11,Accent);
 Text(TEXT("ESC  MENUE"),W-111,28,11,Muted);

 FString Objective=TEXT("CORE SICHERN");
 bool Extracting=false;
 if(Match)switch(Match->GetCoreReadout()){
  case EVanthexCoreReadout::Alpha:Objective=TEXT("CORE  /  ALPHA");break;
  case EVanthexCoreReadout::Bravo:Objective=TEXT("CORE  /  BRAVO");break;
  case EVanthexCoreReadout::Dropped:Objective=TEXT("CORE FALLENGELASSEN");break;
  case EVanthexCoreReadout::Thrown:Objective=TEXT("CORE IM FLUG");break;
  case EVanthexCoreReadout::ExtractingA:Objective=TEXT("EXTRAKTION  /  A");Extracting=true;break;
  case EVanthexCoreReadout::ExtractingB:Objective=TEXT("EXTRAKTION  /  B");Extracting=true;break;
  case EVanthexCoreReadout::Extracted:Objective=TEXT("CORE EXTRAHIERT");break;
  default:break;
 }
 if(Character&&Character->IsCarryingCore()&&!Extracting)Objective=TEXT("DU TRAEGST DEN CORE");
 Rect(CX-150,95,300,Extracting?65:28,Ink);
 Text(Objective,CX,102,12,Extracting?Amber:Accent,true);
 if(Extracting){
  Text(FString::Printf(TEXT("%.1f s"),Match->GetExtractionRemaining()),CX,120,18,White,true);
  Bar(CX-130,148,260,4,1.f-Match->GetExtractionRemaining()/FMath::Max(.01f,Match->GetExtractionDuration()),Amber);
 }

 // Vitals: numbers, labels and bars all remain readable without relying on colour alone.
 Panel(28,H-135,236,107,Accent);
 const float HP=Health?Health->GetHealth():0.f;
 const auto HPColor=HP<=30?Red:White;
 Text(TEXT("LEBEN"),44,H-125,11,Muted);
 Text(Health?FString::Printf(TEXT("%03d"),FMath::CeilToInt(HP)):TEXT("---"),44,H-111,38,HPColor);
 Text(bDead?TEXT("GEFALLEN"):HP<=30?TEXT("KRITISCH"):TEXT("ARC"),137,H-95,12,HP<=30?Red:Muted);
 Bar(44,H-66,204,6,Health?HP/FMath::Max(1.f,Health->GetMaxHealth()):0.f,HP<=30?Red:Accent);
 const float Armor=PlayerState?PlayerState->GetArmorPoints():0.f;
 const float MaxArmor=PlayerState?VanthexEconomy::MaximumArmor(PlayerState->GetEconomy().ArmorTier):0.f;
 Text(FString::Printf(TEXT("RUESTUNG  %03d"),FMath::CeilToInt(Armor)),44,H-51,11,Muted);
 Bar(167,H-47,81,3,MaxArmor>0?Armor/MaxArmor:0,Accent);
 if(PlayerState){
  Rect(28,H-169,236,28,Ink);
  Text(FString::Printf(TEXT("%d CR"),PlayerState->GetEconomy().Credits),44,H-163,14,Amber);
  Text(Buy?TEXT("B  AUSRUESTEN"):TEXT("B  AUSRUESTUNG"),140,H-160,10,Buy?White:Muted);
 }

 // Active weapon and ammunition. Knife selection never shows a stale rifle magazine.
 Panel(W-276,H-135,248,107,Accent);
 const FString WeaponName=Knife?TEXT("KAMPFMESSER"):Weapon?Weapon->GetWeaponId().ToString().ToUpper():TEXT("KEINE WAFFE");
 Text(WeaponName,W-260,H-123,16,White);
 Text(Knife?TEXT("NAHKAMPF"):Weapon&&Weapon->IsAutomatic()?TEXT("AUTO"):TEXT("SEMI"),W-92,H-120,10,Muted);
 if(!Knife&&Weapon){
  const int32 Ammo=Weapon->GetAmmoInMagazine();
  Text(FString::Printf(TEXT("%02d"),Ammo),W-260,H-101,40,Ammo<=FMath::Max(1,Weapon->GetMagazineSize()/5)?Amber:White);
  Text(FString::Printf(TEXT("/ %03d"),Weapon->GetReserveAmmo()),W-186,H-81,18,Muted);
  const bool Reload=Weapon->IsReloading();
  Bar(W-260,H-52,216,3,Reload?Weapon->GetReloadProgress():float(Ammo)/FMath::Max(1,Weapon->GetMagazineSize()),Reload?Amber:Accent);
  Text(Reload?TEXT("WIRD NACHGELADEN"):Equipment&&Equipment->IsSwitching()?TEXT("WAFFENWECHSEL"):Ammo==0?TEXT("R  NACHLADEN"):TEXT("R  LADEN   /   RMB  VISIER"),W-260,H-44,9,Reload||Ammo==0?Amber:Muted);
 }else Text(TEXT("LMB  ANGRIFF"),W-260,H-79,15,Muted);

 // Compact inventory; slot names come from the actual owned loadout.
 const EVanthexWeaponSlot WeaponSlots[]={EVanthexWeaponSlot::Rifle,EVanthexWeaponSlot::Pistol,EVanthexWeaponSlot::Knife};
 for(int32 I=0;I<5;++I){
  const float X=CX-217+I*88;
  bool Selected=I<3&&Equipment&&Equipment->GetSelectedSlot()==WeaponSlots[I];
  FString Name=TEXT("LEER");
  if(I<3&&Equipment&&Equipment->HasSlot(WeaponSlots[I]))Name=I==2?TEXT("MESSER"):Equipment->GetSlotWeaponId(WeaponSlots[I]).ToString().ToUpper();
  if(I>=3&&PlayerState&&PlayerState->GetEconomy().Gadgets.IsValidIndex(I-3))Name=PlayerState->GetEconomy().Gadgets[I-3].ToString().ToUpper();
  Rect(X,H-79,82,51,Selected?FLinearColor(.035f,.11f,.14f,.94f):Ink);
  Rect(X,H-30,82,2,Selected?Accent:Edge);
  Text(FString::FromInt(I+1),X+8,H-74,11,Selected?Accent:Muted);
  Text(Name,X+8,H-53,10,Name==TEXT("LEER")?Muted:White);
 }
 FString Condition;
 if(Character&&Character->bIsCrouched)Condition=TEXT("GEDUCKT");
 if(Character&&Character->GetGadgets()->IsJammed())Condition+=Condition.IsEmpty()?TEXT("EMP  /  GESTOERT"):TEXT("   |   EMP  /  GESTOERT");
 if(!Condition.IsEmpty())Text(Condition,CX,H-101,12,Amber,true);
 if(Buy){
  Rect(CX-180,162,360,43,Ink);
  Text(TEXT("AUSRUESTEN UND VORBEREITEN"),CX,169,14,White,true);
  Text(TEXT("B  KAUFMENUE"),CX,188,10,Amber,true);
 }

 // One contextual prompt at a time, including a progress bar for power hacking.
 FString Prompt;float Interaction=-1.f;
 if(Character&&!bDead){
  float Nearest=300.f;
  for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It){
   const float D=FVector::Dist(Character->GetActorLocation(),It->InteractionPoint(Character->GetActorLocation()));
   if(D>=Nearest)continue;Nearest=D;
   Prompt=It->Kind==3?(It->bPowered?TEXT("F  STROM ABSCHALTEN"):TEXT("F  STROM WIEDERHERSTELLEN")):It->Kind==2?(It->bPowered?TEXT("F  FAHRSTUHL BEDIENEN"):TEXT("FAHRSTUHL OHNE STROM")):TEXT("F  INTERAGIEREN");
   Interaction=It->Kind==3?It->HackProgress:-1.f;
   if(It->IsEMPDisabled()){Prompt=TEXT("EMP  /  GERAET GESTOERT");Interaction=-1.f;}
  }
  if(Prompt.IsEmpty()){
   if(Character->IsCarryingCore())Prompt=TEXT("G  ABLEGEN     Q  WERFEN");
   else for(TActorIterator<AVanthexCore> It(GetWorld());It;++It)if(It->GetCoreState()==EVanthexCoreState::AtRest&&FVector::Dist(Character->GetActorLocation(),It->GetActorLocation())<180){Prompt=TEXT("E  CORE AUFNEHMEN");break;}
  }
 }
 if(!Prompt.IsEmpty()){
  Rect(CX-195,H*.5f+64,390,Interaction>=0?44:30,Ink);
  Text(Prompt,CX,H*.5f+72,13,White,true);
  if(Interaction>=0)Bar(CX-175,H*.5f+99,350,3,Interaction,Accent);
 }
 if(bDead){
  Panel(CX-225,H*.5f-101,450,61,Red);
  Text(TEXT("ELIMINIERT"),CX,H*.5f-94,23,White,true);
  Text(TEXT("Du steigst in der naechsten Runde wieder ein."),CX,H*.5f-62,12,Muted,true);
 }

	if (!Match)
	{
		return;
	}

	FString Banner;

	if (Match->GetPhase() == EVanthexRoundPhase::MatchEnded)
	{
		Banner = FString::Printf(TEXT("%s GEWINNT DAS MATCH"), TeamLabel(Match->GetMatchWinner()));

	}
	else if (Match->GetPhase() == EVanthexRoundPhase::RoundEnding || Match->GetPhase() == EVanthexRoundPhase::Intermission)
	{
		if (Match->GetRoundWinner() == EVanthexTeam::None)
		{
			Banner = TEXT("UNENTSCHIEDEN");
		}
		else
		{
			Banner = FString::Printf(TEXT("%s GEWINNT DIE RUNDE"), TeamLabel(Match->GetRoundWinner()));
		}
	}

	if (!Banner.IsEmpty())
	{
		Panel(CX-225,H*.5f-178,450,62,Accent);
  Text(Banner,CX,H*.5f-161,25,White,true);
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
  DrawTacticalHUD();
  const float Flash=Gadgets->GetFlashOpacity();
  if(Flash>0.f)DrawRect(FLinearColor(.94f,.96f,1.f,Flash),0,0,Canvas->SizeX,Canvas->SizeY);
 } else DrawTacticalHUD();
}
