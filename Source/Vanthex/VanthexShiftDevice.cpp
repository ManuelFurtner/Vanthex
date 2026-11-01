#include "VanthexShiftDevice.h"
#include "VanthexCharacter.h"
#include "HealthComponent.h"
#include "VanthexGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/PointLight.h"
#include "VanthexArt.h"
AVanthexShiftDevice::AVanthexShiftDevice()
{
 bReplicates = true; bAlwaysRelevant = true; SetReplicateMovement(true);
 PrimaryActorTick.bCanEverTick = true;
 Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Device")); SetRootComponent(Mesh);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Mesh->SetStaticMesh(Cube.Object); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 ScreenFront=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenFront"));
 ScreenBack=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenBack"));
 for(auto* Panel:{ScreenFront.Get(),ScreenBack.Get()}) {
  Panel->SetupAttachment(Mesh); Panel->SetStaticMesh(Cube.Object); Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Panel->SetRelativeScale3D(FVector(0.8f,0.05f,0.4f)); Panel->SetVisibility(false);
 }
 ScreenFront->SetRelativeLocation(FVector(0,-55,15)); ScreenBack->SetRelativeLocation(FVector(0,55,15));

 Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight")); Light->SetupAttachment(Mesh);
 Light->SetRelativeLocation(FVector(0,0,150)); Light->SetIntensity(5000); Light->SetAttenuationRadius(1000);
}
void AVanthexShiftDevice::Configure(uint8 InKind) { Kind=InKind; ScreenFront->SetVisibility(Kind==3); ScreenBack->SetVisibility(Kind==3); Mesh->SetWorldScale3D(Kind==0 ? FVector(DoorWidth/100.f,0.3f,2.8f) : Kind==1 ? FVector(0.4f,0.4f,1) : Kind==3 ? FVector(1.2f,0.6f,1.6f) : FVector(DoorWidth/100.f,DoorWidth/100.f,0.16f)); }
FVector AVanthexShiftDevice::InteractionPoint(const FVector& UserLocation) const
{
 if(Kind!=2) return GetActorLocation();
 // Both landings can call the platform, including while it is at the other floor.
 return Home+FVector(0,0,UserLocation.Z>Home.Z+LiftTravel*0.5f ? LiftTravel+65.f : 65.f);
}
void AVanthexShiftDevice::SetCircuitPower(bool bOn)
{
 if(!HasAuthority()) return;
 for(TActorIterator<AVanthexShiftDevice> It(GetWorld());It;++It) if(It->Circuit==Circuit) { It->bPowered=bOn; It->ForceNetUpdate(); }
}
// Capture the spawn transform before GameMode starts its first round and resets devices.
void AVanthexShiftDevice::PostInitializeComponents() { Super::PostInitializeComponents(); Home=GetActorLocation(); }
void AVanthexShiftDevice::BeginPlay() {
 Super::BeginPlay(); OnRep_Kind();
 Mesh->SetMaterial(0,VanthexArt::MakeSurface(this,FLinearColor(0.10f,0.15f,0.19f),0.55f,0.6f));
 auto* Screen=VanthexArt::MakeEnergy(this,FLinearColor(0.15f,0.8f,1.f),1.f); ScreenFront->SetMaterial(0,Screen); ScreenBack->SetMaterial(0,Screen);
}

void AVanthexShiftDevice::OnRep_Kind() { Configure(Kind); }
bool AVanthexShiftDevice::Activate(AVanthexCharacter* User, float Range)
{
 auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if (!HasAuthority() || !User || User->GetHealthComponent()->IsDead() || !Mode || !Mode->IsRoundActive() || GetWorld()->GetTimeSeconds()-LastUse<0.75f || FVector::Dist(User->GetActorLocation(),InteractionPoint(User->GetActorLocation()))>Range || (!bPowered && Kind!=3)) return false;
 FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(ShiftUse),false,User); Params.AddIgnoredActor(this);
 if (GetWorld()->LineTraceSingleByChannel(Hit,User->GetPawnViewLocation(),InteractionPoint(User->GetActorLocation()),ECC_Visibility,Params)) return false;
 if(Kind==3) {
  if(Hacker.IsValid()) return false;
  Hacker=User; HackRange=Range; HackHealth=User->GetHealthComponent()->GetHealth(); HackProgress=0.f;
  LastUse=GetWorld()->GetTimeSeconds(); ForceNetUpdate(); return true;
 }
 bActive=!bActive; LastUse=GetWorld()->GetTimeSeconds(); ForceNetUpdate(); return true;
}
void AVanthexShiftDevice::ResetDevice() { if (HasAuthority()) { bActive=false; bPowered=true; Hacker.Reset(); HackProgress=0; SetActorLocation(Home); LastUse=-10; ForceNetUpdate(); } }
void AVanthexShiftDevice::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 Light->SetVisibility(Kind==1 ? bPowered && !bActive : true);
 Light->SetLightColor(!bPowered ? FLinearColor::Red : HackProgress>0 ? FLinearColor::Yellow : bActive ? FLinearColor::Green : FLinearColor(0.2f,0.5f,1));
 if(Kind==3) {
  const FName Tag(*FString::Printf(TEXT("Circuit%d"),Circuit));
  for(TActorIterator<APointLight> It(GetWorld());It;++It) if(It->Tags.Contains(Tag)) It->GetLightComponent()->SetVisibility(bPowered);
  if(HasAuthority() && Hacker.IsValid()) {
   auto* User=Hacker.Get(); auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
   FHitResult Hit; FCollisionQueryParams Params; Params.AddIgnoredActor(User); Params.AddIgnoredActor(this);
   if(!Mode || !Mode->IsRoundActive() || User->GetHealthComponent()->IsDead() || User->GetHealthComponent()->GetHealth()<HackHealth || FVector::Dist(User->GetActorLocation(),GetActorLocation())>HackRange || GetWorld()->LineTraceSingleByChannel(Hit,User->GetPawnViewLocation(),GetActorLocation(),ECC_Visibility,Params)) { Hacker.Reset(); HackProgress=0; }
   else { HackProgress+=DeltaSeconds/4.f; if(HackProgress>=1.f) { SetCircuitPower(!bPowered); Hacker.Reset(); HackProgress=0; LastUse=GetWorld()->GetTimeSeconds(); } }
  } else if(HasAuthority()) HackProgress=0;
 }
 if (HasAuthority() && bPowered && (Kind==0 || Kind==2)) {
  // Riders are moved by CharacterMovement's movement-base support. Sweeping the
  // platform against its riders would incorrectly stop it at their feet.
  if(Kind==2) for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It) Mesh->IgnoreActorWhenMoving(*It,true);
  const FVector Target=Home+FVector(0,0,bActive ? (Kind==0 ? 300.f : LiftTravel):0.f);
  // Sweep prevents a closing device from crushing or trapping a player.
  SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(),Target,DeltaSeconds,120.f),true);
 }
}
void AVanthexShiftDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AVanthexShiftDevice,Kind); DOREPLIFETIME(AVanthexShiftDevice,bActive); DOREPLIFETIME(AVanthexShiftDevice,DoorWidth); DOREPLIFETIME(AVanthexShiftDevice,LiftTravel); DOREPLIFETIME(AVanthexShiftDevice,Circuit); DOREPLIFETIME(AVanthexShiftDevice,bPowered); DOREPLIFETIME(AVanthexShiftDevice,HackProgress); DOREPLIFETIME(AVanthexShiftDevice,Home); }

