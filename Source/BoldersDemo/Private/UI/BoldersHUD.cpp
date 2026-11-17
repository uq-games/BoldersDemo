#include "UI/BoldersHUD.h"
#include "Characters/BaseCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Player/BoldersPlayerController.h"
#include "Engine/Engine.h"

void ABoldersHUD::DrawHealthBar(const ABaseCharacter& Character, float X, float Y, float Width, float Height, FLinearColor Color)
{
	const float Ratio = FMath::Clamp(Character.GetHealth() / FMath::Max(Character.GetMaxHealth(), 1.f), 0.f, 1.f);
	if (HealthBarFrame)
		// Crop the transparent padding using UVs; preserve the generated source image.
		DrawTexture(HealthBarFrame, X, Y, Width, Height, 20.f/2172.f, 225.f/724.f, 2132.f/2172.f, 270.f/724.f);
	else DrawRect(FLinearColor(.06f,.06f,.07f,.95f), X, Y, Width, Height);
	const float InnerX = X + Width*.075f, InnerY = Y + Height*.25f;
	DrawRect(Color, InnerX, InnerY, Width*.85f*Ratio, Height*.5f);
	DrawRect(Color*1.25f, InnerX, InnerY, Width*.85f*Ratio, Height*.12f);
}

void ABoldersHUD::DrawParty(const TArray<ABaseCharacter*>& Members)
{
	const float Scale = FMath::Clamp(Canvas->SizeX / 1280.f, .65f, 1.15f);
	const float X = 18*Scale, Width = 280*Scale;
	float Y = 18*Scale;
	const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	for (const ABaseCharacter* Member : Members)
	{
		const FLinearColor Color = Member->PlayerSlot == 1 ? FLinearColor(.08f,.65f,.72f) : FLinearColor(.9f,.52f,.12f);
		DrawRect(FLinearColor(.018f,.025f,.04f,.88f), X, Y, Width, 70*Scale);
		DrawRect(Color, X, Y, 3*Scale, 70*Scale);
		const FString Name = FString::Printf(TEXT("%dP%s"), Member->PlayerSlot,
			PC && PC->GetAssignedCharacter() == Member ? TEXT("  /  YOU") : TEXT("  /  ALLY"));
		DrawText(Name, FLinearColor(.92f,.85f,.64f), X+14*Scale, Y+7*Scale, GEngine->GetSmallFont(), Scale);
		const FString Health = FString::Printf(TEXT("%.0f / %.0f"), Member->GetHealth(), Member->GetMaxHealth());
		float TW, TH; GetTextSize(Health, TW, TH, GEngine->GetSmallFont(), Scale);
		DrawText(Health, FLinearColor::White, X+Width-14*Scale-TW, Y+7*Scale, GEngine->GetSmallFont(), Scale);
		DrawHealthBar(*Member, X+8*Scale, Y+30*Scale, Width-16*Scale, 32*Scale, Color);
		Y += 78*Scale;
	}
}

void ABoldersHUD::DrawHUD()
{
 Super::DrawHUD();
 if (!Canvas || !PlayerOwner) return;
 if (const auto* PC=Cast<ABoldersPlayerController>(PlayerOwner); PC && PC->IsChoosingClass()) return;
 TArray<ABaseCharacter*> Party;
 for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
 {
  if (It->GetFaction() == ECharacterFaction::Enemy)
  {
   const FVector Head = It->GetActorLocation() + FVector(0,0,It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+35);
   const FVector P = Canvas->Project(Head);
   if (P.Z > 0 && P.X >= 0 && P.X <= Canvas->SizeX && P.Y >= 0 && P.Y <= Canvas->SizeY)
   {
    DrawHealthBar(**It, P.X-55, P.Y-10, 110, 16, FLinearColor(.8f,.12f,.12f));
    const FString Value = FString::Printf(TEXT("%.0f / %.0f"), It->GetHealth(), It->GetMaxHealth());
    float TW, TH; GetTextSize(Value, TW, TH, GEngine->GetSmallFont(), .75f);
    DrawText(Value, FLinearColor::White, P.X-TW*.5f, P.Y-27, GEngine->GetSmallFont(), .75f);
   }
  }
  if (It->PlayerSlot <= 0) continue;
  Party.Add(*It);
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
 Party.Sort([](const ABaseCharacter& A, const ABaseCharacter& B) { return A.PlayerSlot < B.PlayerSlot; });
 DrawParty(Party);
}
