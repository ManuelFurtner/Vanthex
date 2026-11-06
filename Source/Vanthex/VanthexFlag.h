#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanthexTypes.h"
#include "VanthexFlag.generated.h"
class AVanthexCharacter;
class UStaticMeshComponent;
UCLASS()
class AVanthexFlag : public AActor
{
 GENERATED_BODY()
public:
 AVanthexFlag();
 virtual void Tick(float DeltaSeconds) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 void Initialize(EVanthexTeam Team);
 void ReturnHome();
 void Drop();
 bool TryTouch(AVanthexCharacter* Pawn);
 EVanthexTeam GetTeam() const { return Team; }
 AVanthexCharacter* GetCarrier() const { return Carrier; }
 bool IsHome() const { return bHome; }
 FVector Base() const;
 static AVanthexFlag* Find(UWorld* World,EVanthexTeam Team);
private:
 UFUNCTION() void UpdateAppearance();
 UPROPERTY(ReplicatedUsing=UpdateAppearance) EVanthexTeam Team=EVanthexTeam::None;
 UPROPERTY(Replicated) TObjectPtr<AVanthexCharacter> Carrier;
 UPROPERTY(Replicated) bool bHome=true;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Pole;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Banner;
 float ReturnAt=0;
};
