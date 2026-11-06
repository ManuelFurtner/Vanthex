#pragma once

#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

namespace VanthexMenuStyle
{
inline const FLinearColor Ink(.012f, .019f, .027f, 1.f);
inline const FLinearColor Panel(.024f, .037f, .05f, 1.f);
inline const FLinearColor Cyan(.14f, .82f, .78f, 1.f);
inline const FLinearColor Muted(.54f, .65f, .69f, 1.f);
inline const FLinearColor White(.91f, .95f, .96f, 1.f);

inline TSharedRef<STextBlock> Text(const FString& Value, int32 Size = 16, FLinearColor Color = White)
{
    return SNew(STextBlock)
        .Text(FText::FromString(Value))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size))
        .ColorAndOpacity(Color)
        .AutoWrapText(true);
}
}
