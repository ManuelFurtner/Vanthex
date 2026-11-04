#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Slate-only: safe to paint while the game thread is blocked loading a map. */
class SVanthexLoadingScreen final : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SVanthexLoadingScreen) {} SLATE_END_ARGS()
 void Construct(const FArguments& Args);
};
