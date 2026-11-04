#include "VanthexTacticalHUD.h"
#include "VanthexHUD.h"
#include "VanthexCharacter.h"
#include "VanthexPlayerController.h"
#include "VanthexPlayerState.h"
#include "VanthexGameState.h"
#include "VanthexMapLayout.h"
#include "VanthexExtractionZone.h"
#include "HealthComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void AVanthexHUD::DrawTacticalHUD()
{
 auto* PC=Cast<AVanthexPlayerController>(GetOwningPlayerController());
 const auto* Pawn=Cast<AVanthexCharacter>(GetOwningPawn());
 const auto* Own=PC?PC->GetPlayerState<AVanthexPlayerState>():nullptr;
 const auto* Match=GetWorld()->GetGameState<AVanthexGameState>();
 if(!Canvas||!PC||!Match) return;
 const float S=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f),W=Canvas->SizeX/S,H=Canvas->SizeY/S;
 const FLinearColor Ink(.012f,.021f,.03f,.95f),Edge(.18f,.29f,.35f),Muted(.52f,.65f,.72f),White(.94f,.97f,1),Cyan(.12f,.87f,.96f),Amber(1,.64f,.24f);
 UFont* Font=DisplayFont?DisplayFont.Get():GEngine->GetMediumFont();
 auto Rect=[&](float X,float Y,float Width,float Height,FLinearColor Color){DrawRect(Color,X*S,Y*S,Width*S,Height*S);};
 auto Text=[&](const FString& Value,float X,float Y,float Size,FLinearColor Color){DrawText(Value,Color,X*S,Y*S,Font,Size*S/FMath::Max(1,Font->GetMaxCharHeight()));};
 auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width=1.f){DrawLine(A.X*S,A.Y*S,B.X*S,B.Y*S,Color,Width*S);};
 if(Pawn&&VanthexMap::IsFacility(GetWorld())) {
  const int32 Floor=VanthexTacticalHUD::Floor(Pawn->GetActorLocation().Z);
  // 70% of the previous panel dimensions; keep labels legible at the smaller size.
  Rect(28,82,134.4f,137.2f,Ink); Rect(28,82,134.4f,2,Cyan);
  Text(TEXT("KARTE"),36.4f,88.3f,11,White);
  Text(Floor<0?TEXT("UG"):Floor>0?TEXT("OG"):TEXT("EG"),135.8f,88.3f,11,Cyan);
  const FVector2D Extent=VanthexMap::Extent(GetWorld());
  const float Scale=FMath::Min(117.6f/(Extent.X*2),95.2f/(Extent.Y*2));
  auto Project=[&](FVector P){return FVector2D(95.2f+P.X*Scale,153.4f-P.Y*Scale);};
  for(const auto& Room:VanthexMap::Rooms(GetWorld())) if(FMath::IsNearlyEqual(Room.Center.Z,Floor*600.f,5.f)) {
   const auto P=Project(Room.Center); const FVector2D Size(Room.Size.X*Scale,Room.Size.Y*Scale);
   Rect(P.X-Size.X*.5f,P.Y-Size.Y*.5f,Size.X,Size.Y,FLinearColor(.13f,.22f,.28f));
  }
  for(const auto& Link:VanthexMap::Connections(GetWorld())) for(int32 I=1;I<Link.Points.Num();++I)
   if(FMath::IsNearlyEqual(Link.Points[I].Z,Floor*600.f,5.f)) Line(Project(Link.Points[I-1]),Project(Link.Points[I]),FLinearColor(.20f,.33f,.40f),FMath::Max(2.f,Link.Width*Scale));
  for(const auto& Stair:VanthexMap::VerticalLinks(GetWorld())) {
   const bool Bottom=FMath::IsNearlyEqual(Stair.Bottom.Z,Floor*600.f,5.f),Top=FMath::IsNearlyEqual(Stair.Top.Z,Floor*600.f,5.f);
   if(Bottom||Top) { const auto P=Project(Bottom?Stair.Bottom:Stair.Top); Text(Bottom?TEXT("+"):TEXT("-"),P.X-3,P.Y-7,13,Amber); }
  }
  // Objective locations are static. Never display the Core carrier or enemy positions.
  for(TActorIterator<AVanthexExtractionZone> It(GetWorld());It;++It) if(FMath::Abs(It->GetActorLocation().Z-Floor*600.f)<220.f) {
   const auto P=Project(It->GetActorLocation()); Rect(P.X-6,P.Y-6,12,12,FLinearColor(.08f,.34f,.24f));
   Text(It->GetId()==EVanthexExtractionId::A?TEXT("A"):TEXT("B"),P.X-4,P.Y-8,12,FLinearColor(.4f,1,.65f));
  }
  for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) {
   const auto* State=It->GetPlayerState<AVanthexPlayerState>();
   if(!State||!Own||!VanthexTacticalHUD::ShowTeammate(Own->GetTeam(),State->GetTeam(),State->IsAlive(),*It==Pawn)) continue;
   if(VanthexTacticalHUD::Floor(It->GetActorLocation().Z)!=Floor) continue;
   const auto P=Project(It->GetActorLocation()); Rect(P.X-2,P.Y-2,4,4,Cyan);
  }
  const auto P=Project(Pawn->GetActorLocation());
  const FVector D=PC->GetControlRotation().Vector(); const FVector2D Heading(D.X,-D.Y),Side(-Heading.Y,Heading.X);
  Line(P+Heading*7,P-Heading*4+Side*4,White,2); Line(P-Heading*4+Side*4,P-Heading*4-Side*4,White,2); Line(P-Heading*4-Side*4,P+Heading*7,White,2);
  Text(TEXT("N"),149.8f,103,9,Muted);
  Text(TEXT("DU / TEAM"),36.4f,207.3f,10,Muted);
 }
 Text(TEXT("TAB  TEAMSTATUS"),W-160,51,11,Muted);
 if(!PC->IsScoreboardOpen()) return;
 Rect(0,0,W,H,FLinearColor(.002f,.006f,.012f,.68f));
 const float X=(W-820)*.5f,Y=(H-430)*.5f;
 Rect(X,Y,820,430,Ink); Rect(X,Y,820,3,Cyan);
 Text(TEXT("VANTHEX / TEAMSTATUS"),X+24,Y+19,23,White);
 Text(FString::Printf(TEXT("RUNDE %02d     ALPHA %d : %d BRAVO"),Match->GetRoundIndex(),Match->GetAlphaScore(),Match->GetBravoScore()),X+24,Y+53,12,Muted);
 for(int32 TeamIndex=0;TeamIndex<2;++TeamIndex) {
  const auto Team=TeamIndex==0?EVanthexTeam::Alpha:EVanthexTeam::Bravo; const auto Color=TeamIndex==0?Cyan:Amber;
  const float Top=Y+86+TeamIndex*154;
  Rect(X+16,Top,788,27,FLinearColor(.035f,.065f,.085f));
  Text(TeamIndex==0?TEXT("ALPHA"):TEXT("BRAVO"),X+28,Top+5,13,Color);
  Text(TEXT("K"),X+480,Top+5,12,Muted); Text(TEXT("A"),X+535,Top+5,12,Muted); Text(TEXT("D"),X+590,Top+5,12,Muted);
  Text(TEXT("PING"),X+652,Top+5,12,Muted); Text(TEXT("STATUS"),X+714,Top+5,12,Muted);
  TArray<const AVanthexPlayerState*> Players;
  for(const auto& Entry:Match->PlayerArray) if(const auto* State=Cast<AVanthexPlayerState>(Entry);State&&State->GetTeam()==Team) Players.Add(State);
  Players.Sort([](const AVanthexPlayerState& A,const AVanthexPlayerState& B){return A.GetSpawnSlot()<B.GetSpawnSlot();});
  for(int32 I=0;I<Players.Num()&&I<3;++I) {
   const auto* State=Players[I]; const float Row=Top+31+I*36;
   if(State==Own) Rect(X+16,Row-1,788,33,FLinearColor(.05f,.14f,.17f));
   const auto Tint=State->IsAlive()?White:Muted;
   Text(FString::Printf(TEXT("%02d"),State->GetSpawnSlot()+1),X+28,Row+6,12,Color);
   FString Name=State->GetPlayerName(); if(Name.Len()>32) Name=Name.Left(29)+TEXT("...");
   Text(Name+(State==Own?TEXT("  / DU"):TEXT("")),X+62,Row+5,14,Tint);
   Text(FString::FromInt(State->GetKills()),X+480,Row+5,14,Tint); Text(FString::FromInt(State->GetAssists()),X+535,Row+5,14,Tint); Text(FString::FromInt(State->GetDeaths()),X+590,Row+5,14,Tint);
   Text(State->IsABot()?TEXT("BOT"):FString::Printf(TEXT("%d"),FMath::RoundToInt(State->GetPingInMilliseconds())),X+652,Row+6,12,Muted);
   Text(State->IsAlive()?TEXT("AKTIV"):TEXT("TOT"),X+714,Row+6,12,State->IsAlive()?Color:Muted);
  }
 }
 Text(TEXT("K KILLS    A ASSISTS    D TODE"),X+24,Y+398,11,Muted);
 Text(TEXT("TAB LOSLASSEN / SCHLIESSEN"),X+588,Y+398,10,Muted);
}
