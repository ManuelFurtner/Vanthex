#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Vanthex.h"
#include "VanthexGridV3Layout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Editor diagnostic: inspect the real source geometry, not just its bounding box.
bool VanthexInspectStairs()
{
 UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_Stair_01.SM_Stair_01"));
 FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
 if (!Description)
 {
  UE_LOG(LogVanthex, Error, TEXT("STAIR_INSPECTION missing SM_Stair_01 source mesh description"));
  return false;
 }
 const FStaticMeshAttributes Attributes(*Description);
 const auto Positions = Attributes.GetVertexPositions();
 const auto MaterialNames = Attributes.GetPolygonGroupMaterialSlotNames();
 FString Csv = TEXT("Triangle,PolygonGroup,MaterialSlot,AX,AY,AZ,BX,BY,BZ,CX,CY,CZ,NX,NY,NZ,Area\n");
 for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
 {
  const auto Vertices = Description->GetTriangleVertices(Triangle);
  const FVector A(Positions[Vertices[0]]), B(Positions[Vertices[1]]), C(Positions[Vertices[2]]);
  // Match Unreal's clockwise mesh winding (StaticMeshOperations.cpp).
  const FVector Cross = FVector::CrossProduct(C - A, B - A);
  const FVector Normal = Cross.GetSafeNormal();
  const FPolygonGroupID Group = Description->GetTrianglePolygonGroup(Triangle);
  FString Material = MaterialNames[Group].ToString();
  Material.ReplaceInline(TEXT("\""), TEXT("\"\""));
  Csv += FString::Printf(TEXT("%d,%d,\"%s\",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),
   Triangle.GetValue(), Group.GetValue(), *Material,
   A.X, A.Y, A.Z, B.X, B.Y, B.Z, C.X, C.Y, C.Z, Normal.X, Normal.Y, Normal.Z, Cross.Size() * .5);
 }
 const FString Output = FPaths::ProjectSavedDir() / TEXT("StairMeshTriangles.csv");
 const bool Saved = FFileHelper::SaveStringToFile(Csv, *Output);
 const FBox Bounds = Mesh->GetBoundingBox();
 UE_LOG(LogVanthex, Display, TEXT("STAIR_INSPECTION %s triangles=%d vertices=%d bounds_min=%s bounds_max=%s csv=%s"),
  Saved ? TEXT("PASS") : TEXT("FAIL"), Description->Triangles().Num(), Description->Vertices().Num(),
  *Bounds.Min.ToString(), *Bounds.Max.ToString(), *Output);
 return Saved;
}

namespace
{
struct FVisibleStairTriangle
{
 FVector A, B, C;
};

// A vertical ray against the actual rendered triangles; simple collision is deliberately unused.
bool VisibleTreadHeight(const TArray<FVisibleStairTriangle>& Triangles, const FVector& XY, double& OutHeight)
{
 bool Found = false;
 OutHeight = -TNumericLimits<double>::Max();
 for (const FVisibleStairTriangle& Triangle : Triangles)
 {
  const FVector AB = Triangle.B - Triangle.A, AC = Triangle.C - Triangle.A;
  const double Denominator = AB.X * AC.Y - AB.Y * AC.X;
  if (FMath::Abs(Denominator) < 1.e-8) continue;
  const double PX = XY.X - Triangle.A.X, PY = XY.Y - Triangle.A.Y;
  const double U = (PX * AC.Y - PY * AC.X) / Denominator;
  const double V = (AB.X * PY - AB.Y * PX) / Denominator;
  constexpr double EdgeTolerance = 1.e-5;
  if (U < -EdgeTolerance || V < -EdgeTolerance || U + V > 1. + EdgeTolerance) continue;
  OutHeight = FMath::Max(OutHeight, Triangle.A.Z + U * AB.Z + V * AC.Z);
  Found = true;
 }
 return Found;
}
}

bool VanthexValidateVisibleStairs(UWorld* World, TArray<FString>& Out)
{
 UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_Stair_01.SM_Stair_01"));
 const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
 if (!World || !Description)
 {
  Out.Add(TEXT("FAIL visible stairs: world or source triangle geometry unavailable"));
  return false;
 }
 const FStaticMeshConstAttributes Attributes(*Description);
 const auto Positions = Attributes.GetVertexPositions();
 TArray<FVisibleStairTriangle> SourceTriangles;
 SourceTriangles.Reserve(Description->Triangles().Num());
 for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
 {
  const auto Vertices = Description->GetTriangleVertices(Triangle);
  SourceTriangles.Add({FVector(Positions[Vertices[0]]), FVector(Positions[Vertices[1]]), FVector(Positions[Vertices[2]])});
 }

 TArray<FTransform> Instances;
 for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
 {
  TInlineComponentArray<UStaticMeshComponent*> Components;
  Actor->GetComponents(Components);
  for (const UStaticMeshComponent* Component : Components)
  {
   if (Component->GetStaticMesh() != Mesh || !Component->IsVisible() || Actor->IsHidden()) continue;
   if (const auto* Group = Cast<UInstancedStaticMeshComponent>(Component))
   {
    for (int32 Index = 0; Index < Group->GetInstanceCount(); ++Index)
    {
     FTransform Transform;
     if (Group->GetInstanceTransform(Index, Transform, true)) Instances.Add(Transform);
    }
   }
   else Instances.Add(Component->GetComponentTransform());
  }
 }
 const auto& Links = GridV3::VerticalLinks();
 constexpr int32 SectionsPerLink = 3;
 constexpr int32 SamplesPerLink = 21;
 TArray<int32> Matches;
 Matches.Init(0, Links.Num());
 TArray<TArray<FVisibleStairTriangle>> LinkTreads;
 LinkTreads.SetNum(Links.Num());
 bool Passed = Instances.Num() == Links.Num() * SectionsPerLink;
 Out.Add(FString::Printf(TEXT("%s visible stair instances: found=%d expected=%d; actual source mesh has %d triangles"),
  Passed ? TEXT("PASS") : TEXT("FAIL"), Instances.Num(), Links.Num() * SectionsPerLink, SourceTriangles.Num()));
 for (int32 InstanceIndex = 0; InstanceIndex < Instances.Num(); ++InstanceIndex)
 {
  const FTransform& Transform = Instances[InstanceIndex];
  const FVector Center = Transform.TransformPosition(Mesh->GetBoundingBox().GetCenter());
  int32 LinkIndex = INDEX_NONE;
  double ClosestSquared = TNumericLimits<double>::Max();
  for (int32 Index = 0; Index < Links.Num(); ++Index)
  {
   const double DistanceSquared = FVector::DistSquared2D(Center, (Links[Index].Bottom + Links[Index].Top) * .5);
   if (DistanceSquared < ClosestSquared) { ClosestSquared = DistanceSquared; LinkIndex = Index; }
  }
  if (LinkIndex == INDEX_NONE) { Passed = false; continue; }
  ++Matches[LinkIndex];
  for (const FVisibleStairTriangle& Source : SourceTriangles)
  {
   FVisibleStairTriangle Triangle{Transform.TransformPosition(Source.A), Transform.TransformPosition(Source.B), Transform.TransformPosition(Source.C)};
   // World-space normal also respects any non-uniform instance scaling.
   const FVector Normal = FVector::CrossProduct(Triangle.C - Triangle.A, Triangle.B - Triangle.A).GetSafeNormal();
   if (Normal.Z >= .85) LinkTreads[LinkIndex].Add(Triangle);
  }
 }
 for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
 {
  const auto& Link = Links[LinkIndex];
  const auto& Treads = LinkTreads[LinkIndex];
  const FVector Delta = Link.Top - Link.Bottom;
  const double Run = Delta.Size2D();
  const FVector Forward = Delta.GetSafeNormal2D();
  const FVector Across(-Forward.Y, Forward.X, 0.);
  bool LinkPass = !Treads.IsEmpty() && Run > UE_SMALL_NUMBER;
  bool HavePrevious = false;
  double PreviousHeight = 0., MinHeight = TNumericLimits<double>::Max(), MaxHeight = -TNumericLimits<double>::Max(), MaxError = 0.;
  int32 SamplesFound = 0;
  for (int32 Sample = 0; Sample < SamplesPerLink; ++Sample)
  {
   // Include both landing transitions, ten centimetres inside a 20-metre run.
   const double Fraction = Sample == 0 ? .005 : Sample == SamplesPerLink - 1 ? .995 : Sample / 20.;
   const FVector Expected = Link.Bottom + Delta * Fraction;
   double Height = 0.;
   FVector Probe = Expected;
   bool Found = VisibleTreadHeight(Treads, Probe, Height);
   // A centre ray may meet a grate perforation or a tread seam.
   // Keep the fallback local and compare against the ramp at the actual sampled XY.
   if (!Found) for (const double Along : {0., -5., 5.})
   {
    for (const double Side : {0., -5., 5.})
    {
     Probe = Expected + Forward * Along + Across * Side;
     if (VisibleTreadHeight(Treads, Probe, Height)) { Found = true; break; }
    }
    if (Found) break;
   }
   if (!Found)
   {
    Out.Add(FString::Printf(TEXT("FAIL visible stair %d sample %.3f: no upward tread at centreline (+/-5cm); expected_z=%.2f"), Link.Id, Fraction, Expected.Z));
    LinkPass = false;
    continue;
   }
   ++SamplesFound;
   const double ProbeFraction = Run > UE_SMALL_NUMBER ? FMath::Clamp(Fraction + FVector::DotProduct(Probe - Expected, Forward) / Run, 0., 1.) : Fraction;
   const double ExpectedProbeHeight = Link.Bottom.Z + Delta.Z * ProbeFraction;
   const double Error = FMath::Abs(Height - ExpectedProbeHeight);
   const bool HeightAligned = Error <= 20.;
   const bool NonDescending = !HavePrevious || Height >= PreviousHeight - 1.;
   LinkPass &= HeightAligned && NonDescending;
   MinHeight = FMath::Min(MinHeight, Height);
   MaxHeight = FMath::Max(MaxHeight, Height);
   MaxError = FMath::Max(MaxError, Error);
   Out.Add(FString::Printf(TEXT("%s visible stair %d sample %.3f: tread_z=%.2f expected_z=%.2f error=%.2fcm offset_xy=%.2fcm ascending=%d"),
    HeightAligned && NonDescending ? TEXT("INFO") : TEXT("FAIL"), Link.Id, Fraction, Height, ExpectedProbeHeight, Error, FVector::Dist2D(Probe, Expected), NonDescending));
   PreviousHeight = Height;
   HavePrevious = true;
  }
  LinkPass &= SamplesFound == SamplesPerLink;
  Passed &= LinkPass;
  Out.Add(FString::Printf(TEXT("%s visible stair %d: %d/%d actual tread samples across %d sections; height range %.2f..%.2f; maximum ramp mismatch %.2fcm (limit20cm)"),
   LinkPass ? TEXT("PASS") : TEXT("FAIL"), Link.Id, SamplesFound, SamplesPerLink, Matches[LinkIndex], SamplesFound ? MinHeight : 0., SamplesFound ? MaxHeight : 0., MaxError));
 }
 for (int32 Index = 0; Index < Matches.Num(); ++Index)
 {
  const bool Complete = Matches[Index] == SectionsPerLink;
  Passed &= Complete;
  Out.Add(FString::Printf(TEXT("%s visible stair link %d has %d matching mesh instances (expected%d)"), Complete ? TEXT("PASS") : TEXT("FAIL"), Links[Index].Id, Matches[Index], SectionsPerLink));
 }
 return Passed;
}
#endif
