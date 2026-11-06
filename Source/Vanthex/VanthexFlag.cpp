#include "VanthexFlag.h"
#include "VanthexArt.h"
#include "VanthexMapLayout.h"
#include "VanthexCharacter.h"
#include "VanthexGameMode.h"
#include "VanthexPlayerState.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
AVanthexFlag::AVanthexFlag()
{
 bReplicates=true; bAlwaysRelevant=true; SetReplicateMovement(true); PrimaryActorTick.bCanEverTick=true;
 auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
 Pole=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));Pole->SetupAttachment(Root);
 Banner=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flag"));Banner->SetupAttachment(Root);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 for(auto* Part:{Pole.Get(),Banner.Get()}) {Part->SetStaticMesh(Cube.Object);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
 Pole->SetRelativeLocation(FVector(0,0,110));Pole->SetRelativeScale3D(FVector(.06f,.06f,2.2f));
 Banner->SetRelativeLocation(FVector(50,0,175));Banner->SetRelativeScale3D(FVector(1.f,.035f,.65f));
}
void AVanthexFlag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AVanthexFlag,Team);DOREPLIFETIME(AVanthexFlag,Carrier);DOREPLIFETIME(AVanthexFlag,bHome);
}
FVector AVanthexFlag::Base() const {return VanthexMap::FlagBase(Team==EVanthexTeam::Bravo);}
void AVanthexFlag::Initialize(EVanthexTeam NewTeam) {if(!HasAuthority())return;Team=NewTeam;UpdateAppearance();ReturnHome();}
void AVanthexFlag::UpdateAppearance()
{
 const FLinearColor Color=Team==EVanthexTeam::Bravo?FLinearColor(1,.25f,.04f):FLinearColor(.03f,.65f,1);
 Banner->SetMaterial(0,VanthexArt::MakeSurface(this,Color,.7f,0,Color,.3f));
 Pole->SetMaterial(0,VanthexArt::MakeSurface(this,FLinearColor(.3f,.34f,.38f),.3f,.8f));
}
AVanthexFlag* AVanthexFlag::Find(UWorld* World,EVanthexTeam Wanted) {for(TActorIterator<AVanthexFlag> It(World);It;++It)if(It->Team==Wanted)return *It;return nullptr;}
void AVanthexFlag::ReturnHome() {if(!HasAuthority())return;Carrier=nullptr;bHome=true;ReturnAt=0;SetActorLocation(Base());ForceNetUpdate();}
void AVanthexFlag::Drop()
{
 if(!HasAuthority()||!Carrier)return;
 FVector Point=Carrier->GetActorLocation();
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(FlagDrop),false,Carrier);Q.AddIgnoredActor(this);
 if(GetWorld()->LineTraceSingleByChannel(Hit,Point,Point-FVector(0,0,500),ECC_WorldStatic,Q))Point=Hit.ImpactPoint;
 Carrier=nullptr;bHome=false;ReturnAt=GetWorld()->GetTimeSeconds()+30;SetActorLocation(Point);ForceNetUpdate();
}
bool AVanthexFlag::TryTouch(AVanthexCharacter* Pawn)
{
 auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();
 if(!HasAuthority()||!Mode||!Mode->IsRoundActive()||!Pawn||Carrier||Pawn->GetHealthComponent()->IsDead())return false;
 auto* State=Pawn->GetPlayerState<AVanthexPlayerState>();
 if(!State||State->GetTeam()==EVanthexTeam::None||FVector::Dist2D(Pawn->GetActorLocation(),GetActorLocation())>140||FMath::Abs(Pawn->GetActorLocation().Z-GetActorLocation().Z)>160)return false;
 FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(FlagPickup),false,Pawn);Q.AddIgnoredActor(this);
 if(GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetActorLocation(),GetActorLocation()+FVector(0,0,100),ECC_Visibility,Q))return false;
 if(State->GetTeam()==Team)
 {
  if(bHome) return false;
  ReturnHome();
  State->RecordFlagReturn();
  return true;
 }
 Carrier=Pawn;bHome=false;ReturnAt=0;ForceNetUpdate();return true;
}
void AVanthexFlag::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 Banner->SetRelativeRotation(FRotator(0,FMath::Sin(GetWorld()->GetTimeSeconds()*3)*7,0));
 if(!HasAuthority())return;
 auto* Mode=GetWorld()->GetAuthGameMode<AVanthexGameMode>();if(!Mode||!Mode->IsRoundActive())return;
 if(Carrier) {
  if(Carrier->GetHealthComponent()->IsDead()) {Drop();return;}
  SetActorLocation(Carrier->GetActorLocation()+FVector(0,0,35));
  const auto* State=Carrier->GetPlayerState<AVanthexPlayerState>();
  auto* Own=State?Find(GetWorld(),State->GetTeam()):nullptr;
  if(Own&&Own->IsHome()&&FVector::Dist2D(Carrier->GetActorLocation(),Own->Base())<180&&FMath::Abs(Carrier->GetActorLocation().Z-Own->Base().Z)<160) Mode->CaptureFlag(State->GetTeam());
 } else {
  if(!bHome&&ReturnAt<=0)ReturnAt=GetWorld()->GetTimeSeconds()+30;
  if(!bHome&&ReturnAt>0&&GetWorld()->GetTimeSeconds()>=ReturnAt)ReturnHome();
  for(TActorIterator<AVanthexCharacter> It(GetWorld());It;++It)if(TryTouch(*It))break;
 }
}
