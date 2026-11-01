#include "VanthexTestArena.h"
#include "VanthexShiftDevice.h"
#include "VanthexCharacter.h"
#include "HealthComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void AVanthexTestArena::ValidateEnergySystems()
{
 static int32 Stage=0; static bool Passed=true; static TArray<FString> Lines;
 auto Check=[&](bool OK,const TCHAR* Name) { Passed &= OK; Lines.Add(FString(OK?TEXT("PASS "):TEXT("FAIL "))+Name); };
 AVanthexShiftDevice* Terminal=nullptr; AVanthexShiftDevice* Lift=nullptr;
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) {
  if(It->Circuit==1 && It->Kind==3) Terminal=*It;
  if(It->Circuit==1 && It->Kind==2) Lift=*It;
 }
 auto* PC=GetWorld()->GetFirstPlayerController();
 auto* User=PC?Cast<AVanthexCharacter>(PC->GetPawn()):nullptr;
 if(!Terminal || !Lift || !User) { Check(false,TEXT("test actors exist")); Stage=99; }
 if(Stage==0) {
  const FVector Near=Terminal->Home+FVector(0,-180,20);
  User->SetActorLocation(Near);
  Check(Terminal->Activate(User,250),TEXT("nearby player starts four-second hack"));
  Terminal->Tick(2.f); Check(Terminal->bPowered && Terminal->HackProgress>0.4f,TEXT("hack is not instant"));
  User->SetActorLocation(Near+FVector(0,-500,0)); Terminal->Tick(2.f);
  Check(Terminal->bPowered && Terminal->HackProgress==0,TEXT("leaving range cancels hack"));
  Check(!Terminal->Activate(User,250),TEXT("out-of-range activation rejected"));
  Terminal->ResetDevice(); User->SetActorLocation(Near);
  Check(Terminal->Activate(User,250),TEXT("hack restart")); Terminal->Tick(4.1f);
  Check(!Terminal->bPowered && !Lift->bPowered,TEXT("completed hack disables linked lift"));
  User->SetActorLocation(Lift->Home+FVector(0,0,106));
  Check(!Lift->Activate(User,250),TEXT("offline lift rejects travel"));
  Terminal->ResetDevice(); Terminal->SetCircuitPower(false); User->SetActorLocation(Near);
  Check(Terminal->Activate(User,250),TEXT("offline terminal supports restoration")); Terminal->Tick(4.1f);
  Check(Terminal->bPowered && Lift->bPowered,TEXT("second hack restores circuit"));
  Terminal->ResetDevice(); Check(Terminal->Activate(User,250),TEXT("damage cancellation setup"));
  FVanthexDamageRequest Damage; Damage.Damage=5.f;
  User->GetHealthComponent()->ApplyDamage(Damage); Terminal->Tick(0.5f);
  Check(Terminal->HackProgress==0 && Terminal->bPowered,TEXT("damage cancels hack"));
  Lift->ResetDevice(); User->SetActorLocation(Lift->Home+FVector(0,0,106));
  // Allow normal engine movement to settle the pawn onto the lift before calling it.
 } else if(Stage==1) {
  PC->SetControlRotation(FRotator::ZeroRotator);
  User->ServerUseDevice(false);
  Check(Lift->bActive,TEXT("F interaction operates occupied lift while looking ahead"));
 } else if(Stage==2) {
  Check(FMath::Abs(Lift->GetActorLocation().Z-(Lift->Home.Z+600))<5,TEXT("lift reaches upper floor"));
  Check(User->GetActorLocation().Z>Lift->Home.Z+650,TEXT("lift carries player to upper floor"));
  User->SetActorLocation(Lift->Home+FVector(0,-180,100));
  Check(Lift->Activate(User,250),TEXT("lower landing calls distant platform"));
 } else if(Stage==3) {
  Check(FMath::Abs(Lift->GetActorLocation().Z-Lift->Home.Z)<5,TEXT("called lift returns to lower landing"));
  Terminal->SetCircuitPower(false); Terminal->ResetDevice(); Lift->ResetDevice();
  Check(Terminal->bPowered && Lift->bPowered,TEXT("round reset restores power"));
 }
 if(Stage>=3) {
  Lines.Insert(Passed?TEXT("OVERALL PASS"):TEXT("OVERALL FAIL"),0);
  FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n")),*(FPaths::ProjectSavedDir()/TEXT("VanthexEnergyTest.txt")));
  FPlatformMisc::RequestExitWithStatus(false,Passed?0:1); return;
 }
 ++Stage; FTimerHandle Handle;
 GetWorld()->GetTimerManager().SetTimer(Handle,this,&AVanthexTestArena::ValidateEnergySystems,Stage==1?1.f:7.f,false);
}
