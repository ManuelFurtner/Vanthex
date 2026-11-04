#pragma once
#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

/** Shared geometry for Canvas HUD and Slate preview. Values are screen pixels. */
struct FVanthexCrosshairStyle
{
 float Thickness=1.5f, Length=8.f, Gap=0.f;
 FLinearColor Color=FLinearColor::White;
 static float Safe(float Value,float Min,float Max,float Fallback)
 { return FMath::IsFinite(Value)?FMath::Clamp(Value,Min,Max):Fallback; }
 static FVanthexCrosshairStyle Make(float Thickness,float Length,float Gap,FLinearColor Color)
 {
  FVanthexCrosshairStyle S;
  S.Thickness=Safe(Thickness,1,8,1.5f); S.Length=Safe(Length,2,32,8); S.Gap=Safe(Gap,0,24,0);
  S.Color=FLinearColor(Safe(Color.R,0,1,1),Safe(Color.G,0,1,1),Safe(Color.B,0,1,1),1);
  return S;
 }
 TStaticArray<FVector4f,4> Rects() const
 {
  const float Half=Thickness*.5f;
  TStaticArray<FVector4f,4> R;
  R[0]=FVector4f(-Gap-Length,-Half,Length,Thickness);
  R[1]=FVector4f(Gap,-Half,Length,Thickness);
  R[2]=FVector4f(-Half,-Gap-Length,Thickness,Length);
  R[3]=FVector4f(-Half,Gap,Thickness,Length);
  return R;
 }
};
