#include "UI/Combat/BoldersHUD.h"

#include "CanvasItem.h"
#include "Characters/BaseCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Player/BoldersPlayerController.h"

namespace
{
	FString HealthLabel(const ABaseCharacter& Character)
	{
		return FString::Printf(TEXT("%.0f / %.0f"), Character.GetHealth(), Character.GetMaxHealth());
	}

	void DrawLabel(UCanvas& Canvas, const FString& Text, const FVector2D& Position, float Scale,
		const FLinearColor& Color, bool bCentered = false)
	{
		FCanvasTextItem Item(Position, FText::FromString(Text), GEngine->GetSmallFont(), Color);
		Item.Scale = FVector2D(Scale, Scale);
		Item.bCentreX = bCentered;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, .95f));
		Canvas.DrawItem(Item);
	}
}

float ABoldersHUD::GetDamageTrail(const ABaseCharacter& Character, float Ratio)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const TWeakObjectPtr<const ABaseCharacter> Key(&Character);
	auto* State = HealthPresentation.Find(Key);
	if (!State)
	{
		HealthPresentation.Add(Key, {Ratio, Ratio, Now});
		return Ratio;
	}

	constexpr double Hold = .23;
	constexpr double Fade = .42;
	const float Progress = FMath::Clamp(static_cast<float>((Now - State->DamageTime - Hold) / Fade), 0.f, 1.f);
	const float PreviousTrail = FMath::Lerp(State->TrailStart, State->PreviousRatio, Progress);
	if (Ratio < State->PreviousRatio)
	{
		State->TrailStart = PreviousTrail;
		State->DamageTime = Now;
	}
	else if (Ratio > State->PreviousRatio)
	{
		State->TrailStart = Ratio;
	}
	const float Result = Ratio < State->PreviousRatio ? PreviousTrail : FMath::Max(Ratio, PreviousTrail);
	State->PreviousRatio = Ratio;
	return Result;
}

void ABoldersHUD::DrawHealthBar(const ABaseCharacter& Character, float X, float Y, float Width, float Height, FLinearColor Color)
{
	const float Ratio = FMath::Clamp(Character.GetHealth() / FMath::Max(Character.GetMaxHealth(), 1.f), 0.f, 1.f);
	const float Trail = GetDamageTrail(Character, Ratio);
	if (HealthBarFrame)
	{
		const FVector4& UV = HealthBarFrameUV;
		DrawTexture(HealthBarFrame, X, Y, Width, Height, UV.X, UV.Y, UV.Z - UV.X, UV.W - UV.Y);
	}
	else
	{
		DrawRect(FLinearColor(.28f, .21f, .13f), X, Y, Width, Height);
		DrawRect(FLinearColor(.025f, .022f, .026f), X + 1, Y + 1, Width - 2, Height - 2);
	}

	const FVector4& Bounds = HealthBarFillBounds;
	const float InnerX = X + Width * Bounds.X;
	const float InnerY = Y + Height * Bounds.Y;
	const float InnerWidth = Width * (Bounds.Z - Bounds.X);
	const float InnerHeight = Height * (Bounds.W - Bounds.Y);
	const float FillWidth = InnerWidth * Ratio;
	const float TrailWidth = InnerWidth * (Trail - Ratio);
	if (TrailWidth > 0)
	{
		DrawRect(FLinearColor(.91f, .46f, .12f), InnerX + FillWidth, InnerY, TrailWidth, InnerHeight);
	}
	if (FillWidth > 0)
	{
		DrawRect(Color * .72f, InnerX, InnerY, FillWidth, InnerHeight);
		DrawRect(Color, InnerX, InnerY, FillWidth, InnerHeight * .58f);
		DrawRect(FMath::Lerp(Color, FLinearColor::White, .37f), InnerX, InnerY, FillWidth, FMath::Max(1.f, InnerHeight * .13f));
	}
}

void ABoldersHUD::DrawParty(const TArray<ABaseCharacter*>& Members, const ABaseCharacter* LocalCharacter, float Scale)
{
	const float X = 21.f * Scale;
	const float Width = 249.f * Scale;
	float Y = 21.f * Scale;
	for (const ABaseCharacter* Member : Members)
	{
		const bool bLocal = Member == LocalCharacter;
		const FLinearColor Color = Member->PlayerSlot == 1
			? FLinearColor(.045f, .58f, .61f) : FLinearColor(.92f, .47f, .09f);
		DrawRect(FLinearColor(.023f, .018f, .015f, .74f), X, Y, Width, 56.f * Scale);
		DrawRect(FLinearColor(.56f, .39f, .20f, .65f), X, Y, Width, Scale);
		DrawRect(Color, X, Y, bLocal ? 3.f * Scale : Scale, 56.f * Scale);

		const FString Name = FString::Printf(TEXT("%dP  /  %s"), Member->PlayerSlot, bLocal ? TEXT("YOU") : TEXT("ALLY"));
		DrawLabel(*Canvas, Name, {X + 12.f * Scale, Y + 7.f * Scale}, Scale, FLinearColor(.96f, .85f, .66f));

		const FString Health = HealthLabel(*Member);
		float TextWidth, TextHeight;
		GetTextSize(Health, TextWidth, TextHeight, GEngine->GetSmallFont(), Scale);
		DrawLabel(*Canvas, Health, {X + Width - 12.f * Scale - TextWidth, Y + 7.f * Scale}, Scale, FLinearColor::White);
		DrawHealthBar(*Member, X + 10.f * Scale, Y + 28.f * Scale, Width - 20.f * Scale, 18.f * Scale, Color);
		Y += 63.f * Scale;
	}
}

void ABoldersHUD::DrawEnemyHealth(const ABaseCharacter& Character, float Scale)
{
	if (!Character.IsAlive()) return;
	const FVector Head = Character.GetActorLocation() + FVector(0, 0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 35);
	const FVector P = Canvas->Project(Head);
	const float Width = 112.f * Scale;
	const float Height = 11.f * Scale;
	if (P.Z <= 0 || P.X < Width * .5f || P.X > Canvas->SizeX - Width * .5f || P.Y < 22.f * Scale || P.Y > Canvas->SizeY)
	{
		return;
	}

	DrawHealthBar(Character, P.X - Width * .5f, P.Y - Height, Width, Height, FLinearColor(.83f, .07f, .11f));
	if (Character.GetHealth() < Character.GetMaxHealth())
	{
		DrawLabel(*Canvas, HealthLabel(Character), {P.X, P.Y - 26.f * Scale}, .83f * Scale, FLinearColor::White, true);
	}

	const TCHAR* AuraLabel = nullptr;
	FLinearColor AuraColor;
	switch (Character.GetElementalAura())
	{
	case EElementalAura::Heat: AuraLabel = TEXT("HEAT"); AuraColor = FLinearColor(1.f, .48f, .19f); break;
	case EElementalAura::Chill: AuraLabel = TEXT("CHILL"); AuraColor = FLinearColor(.58f, .87f, 1.f); break;
	case EElementalAura::Wet: AuraLabel = TEXT("WET"); AuraColor = FLinearColor(.18f, .66f, 1.f); break;
	case EElementalAura::Charge: AuraLabel = TEXT("CHARGE"); AuraColor = FLinearColor(.84f, .51f, 1.f); break;
	case EElementalAura::Frozen: AuraLabel = TEXT("FROZEN"); AuraColor = FLinearColor(.49f, 1.f, 1.f); break;
	default: break;
	}
	if (AuraLabel)
		DrawLabel(*Canvas, AuraLabel, {P.X, P.Y + 2.f * Scale}, .77f * Scale, AuraColor, true);
}

void ABoldersHUD::DrawAttackIcons(const ABaseCharacter& Character, float Scale)
{
	const FCombatHUDLoadout* Loadout = Loadouts.FindByPredicate([&Character](const FCombatHUDLoadout& Entry)
	{
		return Entry.CharacterClass && Character.IsA(Entry.CharacterClass);
	});
	const UAttackLoadout* Attacks = Character.Combat->AttackLoadout;
	if (!Loadout || !Loadout->Atlas || !Attacks || Attacks->Modules.IsEmpty()) return;

	const float Size = 60.f * Scale;
	const float Gap = 7.f * Scale;
	const float Width = Size * Attacks->Modules.Num() + Gap * (Attacks->Modules.Num() - 1);
	const float Y = Canvas->SizeY - Size - 22.f * Scale;
	float X = (Canvas->SizeX - Width) * .5f;
	for (int32 Slot = 0; Slot < Attacks->Modules.Num(); ++Slot)
	{
		const FAttackModule& Attack = Attacks->Modules[Slot];
		const auto* Icon = Loadout->Icons.FindByPredicate([&Attack](const FCombatHUDIcon& Entry) { return Entry.Id == Attack.Id; });
		if (Icon)
		{
			const FVector4& UV = Icon->UV;
			DrawTexture(Loadout->Atlas, X, Y, Size, Size, UV.X, UV.Y, UV.Z - UV.X, UV.W - UV.Y);
		}
		const float Cooldown = Character.Combat->GetCooldownRemaining(Attack.Id);
		if (Cooldown > 0.f)
		{
			const float Unready = FMath::Clamp(Cooldown / FMath::Max(.01f, Attack.Cooldown), 0.f, 1.f);
			DrawRect(FLinearColor(.012f, .012f, .02f, .78f), X, Y, Size, Size * Unready);
			DrawLabel(*Canvas, FString::Printf(TEXT("%.1f"), Cooldown), {X + Size * .5f, Y + Size * .36f},
				Scale, FLinearColor::White, true);
		}
		DrawLabel(*Canvas, FString::FromInt(Slot + 1), {X + 5.f * Scale, Y + 3.f * Scale}, .83f * Scale, FLinearColor::White);
		if (Character.Combat->GetActiveModuleId() == Attack.Id)
		{
			DrawRect(FLinearColor(1.f, .74f, .34f), X, Y + Size + Scale, Size, 2.f * Scale);
		}
		AddHitBox({X, Y}, {Size, Size}, Attack.Id, true);
		X += Size + Gap;
	}
}

void ABoldersHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	const auto* Character = PC ? PC->GetAssignedCharacter() : nullptr;
	const UAttackLoadout* Attacks = Character ? Character->Combat->AttackLoadout.Get() : nullptr;
	if (!Attacks) return;
	const int32 Slot = Attacks->Modules.IndexOfByPredicate([BoxName](const FAttackModule& Attack) { return Attack.Id == BoxName; });
	if (Slot != INDEX_NONE) PC->SelectAttackSlot(Slot);
}

void ABoldersHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	if (PC && PC->IsChoosingClass()) return;

	for (auto It = HealthPresentation.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	const float Scale = FMath::Clamp(Canvas->SizeX / 1280.f, .65f, 1.25f);
	TArray<ABaseCharacter*> Party;
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFaction() == ECharacterFaction::Enemy) DrawEnemyHealth(**It, Scale);
		if (It->PlayerSlot <= 0) continue;

		Party.Add(*It);
		const FVector Head = It->GetActorLocation() + FVector(0, 0, It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20);
		const FVector Projected = Canvas->Project(Head);
		if (MarkerAtlas && Projected.Z > 0)
		{
			DrawTexture(MarkerAtlas, Projected.X - MarkerSize * .5f, Projected.Y - MarkerSize, MarkerSize, MarkerSize,
				It->PlayerSlot == 1 ? 230.f/2172.f : 1410.f/2172.f, 130.f/724.f, 540.f/2172.f, 520.f/724.f);
		}
	}
	Party.Sort([](const ABaseCharacter& A, const ABaseCharacter& B) { return A.PlayerSlot < B.PlayerSlot; });
	const ABaseCharacter* LocalCharacter = PC ? PC->GetAssignedCharacter() : nullptr;
	DrawParty(Party, LocalCharacter, Scale);
	if (IsValid(LocalCharacter)) DrawAttackIcons(*LocalCharacter, Scale);
}
