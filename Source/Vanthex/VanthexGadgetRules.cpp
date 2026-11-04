#include "VanthexGadgetRules.h"
const FGadgetProfile* VanthexGadgetRules::Find(FName Id)
{
 static const FGadgetProfile Profiles[]={{TEXT("Flash"),1.6f,4.f,1800.f,0.f},{TEXT("Smoke"),1.8f,18.f,450.f,0.f},
  {TEXT("Frag"),2.f,1.f,650.f,110.f},{TEXT("EMP"),1.5f,8.f,1000.f,0.f},{TEXT("Sensor"),1.f,15.f,1000.f,0.f}};
 for(const auto& P:Profiles) if(P.Id==Id) return &P;
 return nullptr;
}
float VanthexGadgetRules::FlashStrength(float Distance,float FacingDot,float Radius)
{
 if(!FMath::IsFinite(Distance)||!FMath::IsFinite(FacingDot)||!FMath::IsFinite(Radius)||Radius<=0||Distance<0||Distance>=Radius) return 0.f;
 const float Facing=FMath::Lerp(.15f,1.f,FMath::Clamp((FacingDot+.25f)/1.0f,0.f,1.f));
 return FMath::Clamp((1.f-Distance/Radius)*Facing,0.f,1.f);
}
float VanthexGadgetRules::BlastDamage(float Distance,float Radius,float Maximum)
{
 if(!FMath::IsFinite(Distance)||!FMath::IsFinite(Radius)||!FMath::IsFinite(Maximum)||Radius<=0||Distance<0||Maximum<=0) return 0.f;
 return Maximum*FMath::Clamp(1.f-Distance/Radius,0.f,1.f);
}
bool VanthexGadgetRules::SegmentIntersectsSmoke(const FVector& A,const FVector& B,const FVector& Center,float Radius)
{
 if(A.ContainsNaN()||B.ContainsNaN()||Center.ContainsNaN()||!FMath::IsFinite(Radius)||Radius<=0) return false;
 return FMath::PointDistToSegmentSquared(Center,A,B)<=FMath::Square(Radius);
}
