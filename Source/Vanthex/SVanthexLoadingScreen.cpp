#include "SVanthexLoadingScreen.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor Cyan(.12f,.85f,.8f), Amber(1.f,.53f,.19f), White(.88f,.94f,.96f), Muted(.4f,.52f,.59f);

class SLoadingCore final : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SLoadingCore) {} SLATE_END_ARGS()
 void Construct(const FArguments&) { Started=FPlatformTime::Seconds(); ForceVolatile(true); }
 virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(600,310); }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
  int32 Layer,const FWidgetStyle&,bool) const override
 {
  // No world, assets, timers or mutable game-thread data are read here.
  const float T=static_cast<float>(FMath::Fmod(FPlatformTime::Seconds()-Started,120.0));
  const FVector2f Center(300,145);
  auto Line=[&](const TArray<FVector2f>& Points,FLinearColor Color,float Width=1.f)
  { FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width); };
  auto Polar=[&](float Radius,float Angle) { return Center+FVector2f(FMath::Cos(Angle),FMath::Sin(Angle))*Radius; };
  auto Hex=[&](float Radius,float Angle,FLinearColor Color,float Width)
  {
   TArray<FVector2f> Points;
   for(int32 I=0;I<=6;++I) Points.Add(Polar(Radius,Angle+I*UE_PI/3));
   Line(Points,Color,Width);
  };
  // Quiet tactical grid and fixed alignment marks frame the moving core.
  for(int32 X=60;X<=540;X+=40) Line({FVector2f(X,20),FVector2f(X,270)},FLinearColor(.06f,.12f,.15f,.35f));
  for(int32 Y=30;Y<=270;Y+=40) Line({FVector2f(60,Y),FVector2f(540,Y)},FLinearColor(.06f,.12f,.15f,.35f));
  Line({FVector2f(45,85),FVector2f(45,30),FVector2f(100,30)},Cyan.CopyWithNewOpacity(.35f));
  Line({FVector2f(500,260),FVector2f(555,260),FVector2f(555,205)},Amber.CopyWithNewOpacity(.35f));
  ++Layer;
  const float Pulse=.5f+.5f*FMath::Sin(T*2.f);
  Hex(61+Pulse*4,UE_PI/6,Cyan.CopyWithNewOpacity(.08f),18);
  Hex(57,UE_PI/6,Cyan.CopyWithNewOpacity(.7f+Pulse*.3f),2);
  Hex(93,T*.24f,Cyan.CopyWithNewOpacity(.42f),1.5f);
  Hex(116,-T*.16f,Amber.CopyWithNewOpacity(.3f),1);
  // A stable V-shaped energy mark, with orbiting packets carrying the motion.
  Line({Center+FVector2f(-25,-18),Center+FVector2f(0,29),Center+FVector2f(25,-18)},Cyan.CopyWithNewOpacity(.12f),14);
  Line({Center+FVector2f(-25,-18),Center+FVector2f(0,29),Center+FVector2f(25,-18)},White,3);
  for(int32 I=0;I<3;++I)
  {
   const float A=T*.8f+I*2*UE_PI/3;
   TArray<FVector2f> Arc;
   for(int32 J=0;J<=14;++J) Arc.Add(Polar(132,A+J*.018f));
   Line(Arc,I==1?Amber:Cyan,2);
  }
  // Indeterminate scanner: deliberately not a fabricated loading percentage.
  Line({FVector2f(150,302),FVector2f(450,302)},FLinearColor(.06f,.15f,.18f),2);
  const float Scan=.5f-.5f*FMath::Cos(T*2.f);
  const float X=150+240*Scan;
  Line({FVector2f(X,302),FVector2f(X+60,302)},Cyan,3);
  return Layer;
 }
private:
 double Started=0;
};

TSharedRef<STextBlock> Label(const TCHAR* Value,int32 Size,FLinearColor Color)
{
 return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size))
  .ColorAndOpacity(Color).Justification(ETextJustify::Center);
}
}

void SVanthexLoadingScreen::Construct(const FArguments&)
{
 ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
  .BorderBackgroundColor(FLinearColor(.006f,.012f,.019f,1)).Padding(24)
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
   [SNew(SBox).WidthOverride(1000).HeightOverride(690)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,16,0,24)[Label(TEXT("T A C T I C A L   C O R E   O P E R A T I O N S"),12,Muted)]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(600).HeightOverride(310)[SNew(SLoadingCore)]]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,22,0,6)[Label(TEXT("V A N T H E X"),64,White)]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,36)[Label(TEXT("SECURE THE CORE. CONTROL THE ARENA."),13,Cyan)]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("SPIELWELT WIRD GELADEN"),12,White)]
     +SVerticalBox::Slot().FillHeight(1)[SNew(SBox)]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,20)[Label(TEXT("3 VS 3     /     EIN CORE     /     DEIN TEAM"),11,Muted)]
    ]]]];
}
