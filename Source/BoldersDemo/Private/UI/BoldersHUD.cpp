#include "UI/BoldersHUD.h"
#include "Characters/BaseCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"

void ABoldersHUD::DrawHUD()
{
 Super::DrawHUD();
 if (!Canvas || !PlayerOwner) return;
 for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
 {
  if (It->PlayerSlot <= 0) continue;
  FVector2D Screen = FVector2D::ZeroVector;
  const FVector Head = It->GetActorLocation() + FVector(0,0,It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+20);
  const FVector Projected = Canvas->Project(Head);
  if (Projected.Z <= 0) continue;
  Screen = FVector2D(Projected.X, Projected.Y);
  const float Size = MarkerSize;
  const float X = Screen.X-Size*.5f, Y = Screen.Y-Size;
  // Original generated atlas includes the lettering and downward pointer.
  if (MarkerAtlas)
   DrawTexture(MarkerAtlas, X, Y, Size, Size, It->PlayerSlot==1 ? 230.f/2172.f : 1410.f/2172.f, 130.f/724.f, 540.f/2172.f, 520.f/724.f);

 }
}
