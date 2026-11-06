#include "VanthexFrontEndMode.h"
#include "VanthexPlayerController.h"

AVanthexFrontEndMode::AVanthexFrontEndMode()
{
 PlayerControllerClass = AVanthexPlayerController::StaticClass();
 DefaultPawnClass = nullptr;
 SpectatorClass = nullptr;
 HUDClass = nullptr;
 bStartPlayersAsSpectators = true;
}
