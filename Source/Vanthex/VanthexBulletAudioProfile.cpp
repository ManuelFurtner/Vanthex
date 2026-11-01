#include "VanthexBulletAudioProfile.h"

UVanthexBulletAudioProfile::UVanthexBulletAudioProfile()
{
 SurfaceTagPriority = {TEXT("MetalGrate"), TEXT("Grate"), TEXT("Steel"), TEXT("Metal"), TEXT("Concrete")};
 SurfaceTags.Add(TEXT("MetalGrate"), TEXT("MetalGrate"));
 SurfaceTags.Add(TEXT("Grate"), TEXT("MetalGrate"));
 SurfaceTags.Add(TEXT("Steel"), TEXT("Steel"));
 SurfaceTags.Add(TEXT("Metal"), TEXT("Steel"));
 SurfaceTags.Add(TEXT("Concrete"), TEXT("Concrete"));
}
