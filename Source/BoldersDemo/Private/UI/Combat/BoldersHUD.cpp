#include "UI/Combat/BoldersHUD.h"
#include "UI/FoundryTypography.h"

#include "AI/Positioning/AttackPositionRules.h"
#include "CanvasItem.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Roles/ArcherCharacter.h"
#include "Characters/Roles/MageCharacter.h"
#include "Characters/Roles/WarriorCharacter.h"
#include "Characters/StatueBoss/StatueBossCharacter.h"
#include "Combat/CharacterCombatComponent.h"
#include "Combat/Environment/VolcanoHazard.h"
#include "Combat/Presentation/CombatGuideStyle.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Game/VolcanoEncounter.h"
#include "Game/VolcanoWaySign.h"
#include "Materials/MaterialInterface.h"
#include "Player/BoldersPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Body(.96f, .92f, .84f);
	const FLinearColor Muted(.67f, .73f, .79f);
	const FLinearColor Gold(.98f, .76f, .48f);
	const FLinearColor Good(.44f, .94f, .80f);
	const FLinearColor Danger(1.f, .53f, .38f);
	const FName BladeDipBox(TEXT("BladeDip"));
	const FName IslandMapOpenBox(TEXT("IslandMapOpen"));
	const FName IslandMapCloseBox(TEXT("IslandMapClose"));
	const FName WaySignOpenBox(TEXT("WaySignOpen"));
	const FName WaySignPlaqueBox(TEXT("WaySignPlaque"));
	const FName WaySignCloseBox(TEXT("WaySignClose"));
	const FName RestartMatchBox(TEXT("RestartMatch"));
	const FLinearColor MapMarkerEdge(.025f, .021f, .024f);
	const FLinearColor MapPlayerOne(.37f, .97f, .94f);
	const FVector4 SocketArt(72, 70, 554, 558);
	const FVector4 BannerArt(630, 234, 1246, 387);
	const FVector4 MapTrimArt(51, 676, 574, 1195);
	const FVector4 PanelArt(634, 818, 1240, 1059);
	const FVector4 FullUV(0, 0, 1, 1);

	FLinearColor PartyColor(const ABaseCharacter& Character)
	{
		return Character.PlayerSlot == 1 ? FLinearColor(.21f, .80f, .83f) : FLinearColor(1.f, .69f, .31f);
	}

	const TCHAR* RoleName(const ABaseCharacter& Character)
	{
		if (Character.IsA<AWarriorCharacter>()) return TEXT("战士");
		if (Character.IsA<AArcherCharacter>()) return TEXT("弓手");
		if (Character.IsA<AMageCharacter>()) return TEXT("法师");
		return TEXT("队员");
	}

	const TCHAR* BladeElementName(EDamageElement Element)
	{
		switch (Element)
		{
		case EDamageElement::Water: return TEXT("水刃");
		case EDamageElement::Ice: return TEXT("冰刃");
		case EDamageElement::Lightning: return TEXT("雷刃");
		default: return TEXT("蘸取");
		}
	}

	FString HealthLabel(const ABaseCharacter& Character)
	{
		return FString::Printf(TEXT("%.0f / %.0f"), Character.GetHealth(), Character.GetMaxHealth());
	}

	void DrawMapTriangle(UCanvas& Canvas, const FVector2D& Tip, const FVector2D& Left, const FVector2D& Right,
		const FLinearColor& Color)
	{
		if (!Canvas.DefaultTexture) return;
		FCanvasTriangleItem Item(Tip, Left, Right, Canvas.DefaultTexture->GetResource());
		Item.SetColor(Color);
		Canvas.DrawItem(Item);
	}

	void DrawMapDiamond(UCanvas& Canvas, const FVector2D& Position, float Radius, const FLinearColor& Color)
	{
		const FVector2D Left = Position - FVector2D(Radius, 0);
		const FVector2D Right = Position + FVector2D(Radius, 0);
		DrawMapTriangle(Canvas, Position - FVector2D(0, Radius), Left, Right, Color);
		DrawMapTriangle(Canvas, Position + FVector2D(0, Radius), Right, Left, Color);
	}

	void DrawIconDisk(UCanvas& Canvas, UTexture2D* Texture, const FVector2D& Center, float Radius,
		const FVector4& UV, const FLinearColor& Color, float Fraction = 1.f)
	{
		if (!Texture || !Texture->GetResource() || Fraction <= 0.f) return;
		constexpr int32 Steps = 32;
		const int32 Count = FMath::CeilToInt(Fraction * Steps);
		const FVector2D UVMid((UV.X + UV.Z) * .5, (UV.Y + UV.W) * .5);
		const FVector2D UVExtent = FVector2D(UV.Z, UV.W) - UVMid;
		TArray<FCanvasUVTri, TInlineAllocator<Steps>> Triangles;
		FVector2D Previous(0, -1);
		for (int32 Segment = 1; Segment <= Count; ++Segment)
		{
			const double Angle = FMath::Min(static_cast<float>(Segment) / Steps, Fraction) * 2.0 * UE_DOUBLE_PI;
			const FVector2D Current(FMath::Sin(Angle), -FMath::Cos(Angle));
			FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
			Triangle.V0_Pos = Center;
			Triangle.V1_Pos = Center + Previous * Radius;
			Triangle.V2_Pos = Center + Current * Radius;
			Triangle.V0_UV = UVMid;
			Triangle.V1_UV = UVMid + Previous * UVExtent;
			Triangle.V2_UV = UVMid + Current * UVExtent;
			Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Color;
			Previous = Current;
		}
		FCanvasTriangleItem Item(TArray<FCanvasUVTri>(Triangles), Texture->GetResource());
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Item);
	}

	struct FTargetLabel
	{
		FString Name;
		FString State;
		FString Hint;
		FLinearColor Accent = FLinearColor(.98f, .70f, .39f);
	};

	const TCHAR* GetHazardRejection(const AVolcanoHazard& Hazard)
	{
		switch (Hazard.GetHazardKind())
		{
		case EVolcanoHazardKind::Eruption: return TEXT("喷口只能用水或冰压制");
		case EVolcanoHazardKind::Puddle:
			if (Hazard.GetHazardState() == EVolcanoHazardState::Steaming) return TEXT("等水槽散去蒸汽");
			if (Hazard.GetHazardState() == EVolcanoHazardState::Frozen) return TEXT("冰封水槽可用火融开");
			if (Hazard.GetHazardState() == EVolcanoHazardState::Active) return TEXT("导电水槽可用冰或火改变状态");
			return TEXT("冷却槽响应冰、火或雷");
		case EVolcanoHazardKind::ExplosiveBarrel: return TEXT("炸药桶已点燃");
		case EVolcanoHazardKind::LightningOrb: return TEXT("导柱正在重新充能");
		}
		return TEXT("当前招式对该机关无效");
	}

	FTargetLabel GetHazardLabel(const AVolcanoHazard& Hazard)
	{
		const auto Phase = Hazard.GetHazardState();
		switch (Hazard.GetHazardKind())
		{
		case EVolcanoHazardKind::Eruption:
			return {TEXT("熔炉喷口"), Phase == EVolcanoHazardState::Active ? TEXT("喷发中") :
				Phase == EVolcanoHazardState::Warning ? TEXT("压力上升") : TEXT("冷却中"),
				TEXT("水或冰可压住喷发")};
		case EVolcanoHazardKind::Puddle:
			return {TEXT("冷却槽"), Phase == EVolcanoHazardState::Frozen ? TEXT("已结冰") :
				Phase == EVolcanoHazardState::Active ? TEXT("导电中") : Phase == EVolcanoHazardState::Steaming ? TEXT("汽化中") : TEXT("可施加潮湿"),
				TEXT("可导电、结冰或汽化"), FLinearColor(.58f, .83f, .85f)};
		case EVolcanoHazardKind::ExplosiveBarrel:
			return {TEXT("炸药桶"), TEXT("未点燃"), TEXT("受击后延时爆炸")};
		case EVolcanoHazardKind::LightningOrb:
			return {TEXT("闪电导柱"), TEXT("已充能"), TEXT("受击后向相连冷却槽放电"),
				FLinearColor(.79f, .71f, 1.f)};
		}
		return {};
	}

	FTargetLabel GetGuardianLabel(const AVolcanoEncounter& Encounter, const ABaseCharacter& Guardian)
	{
		FTargetLabel Label;
		switch (Encounter.GetGuardianRole(&Guardian))
		{
		case EVolcanoGuardianRole::Scrapper:
			Label.Name = TEXT("余烬突袭者"); Label.Hint = TEXT("突刺可侧闪"); break;
		case EVolcanoGuardianRole::Sporecaster:
			Label.Name = TEXT("孢弹投手"); Label.Hint = TEXT("投掷可打断");
			Label.Accent = FLinearColor(.76f, .89f, .55f); break;
		case EVolcanoGuardianRole::Bulwark:
			Label.Name = TEXT("玄武岩卫"); Label.Hint = TEXT("重砸后护壳打开"); break;
		case EVolcanoGuardianRole::None: return Label;
		}
		switch (Encounter.GetGuardianPhase(&Guardian))
		{
	case EVolcanoGuardianPhase::Emerging: Label.State = TEXT("现身中"); break;
	case EVolcanoGuardianPhase::Windup: Label.State = TEXT("蓄力中"); break;
	case EVolcanoGuardianPhase::InFlight: Label.State = TEXT("孢弹在空中"); break;
	case EVolcanoGuardianPhase::Recovery:
		Label.State = Encounter.GetGuardianRole(&Guardian) == EVolcanoGuardianRole::Bulwark ? TEXT("护壳已打开") : TEXT("收势中"); break;
	case EVolcanoGuardianPhase::Interrupted: Label.State = TEXT("已被打断"); break;
		default: Label.State = HealthLabel(Guardian); break;
		}
		return Label;
	}

	const TCHAR* GetOrderLabel(EAttackOrderResult Result, EAttackOrderPhase Phase)
	{
		switch (Result)
		{
	case EAttackOrderResult::Buffered: return TEXT("已预备出招");
	case EAttackOrderResult::CoolingDown: return TEXT("冷却中");
	case EAttackOrderResult::Frozen: return TEXT("已冻结");
	case EAttackOrderResult::InvalidTarget: return TEXT("目标已失效");
	case EAttackOrderResult::Unavailable: return TEXT("无法出招");
	case EAttackOrderResult::OutOfRange: return TEXT("超出攻距");
	case EAttackOrderResult::Obstructed: return TEXT("攻击路线受阻");
	case EAttackOrderResult::Ineffective: return TEXT("当前招式对该机关无效");
		default: break;
		}
		switch (Phase)
		{
	case EAttackOrderPhase::Approaching: return TEXT("接近目标");
	case EAttackOrderPhase::NoApproach: return TEXT("接近路线受阻");
	case EAttackOrderPhase::LostTarget: return TEXT("目标已离开攻距");
	case EAttackOrderPhase::Obstructed: return TEXT("攻击路线受阻");
	case EAttackOrderPhase::Ineffective: return TEXT("机关状态已变化");
		default: return nullptr;
		}
	}
}

ABoldersHUD::ABoldersHUD()
{
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> GuideMaterial(CombatGuideStyle::ReachMaterialPath);
	AttackGuideMaterial = GuideMaterial.Object;
	static const ConstructorHelpers::FObjectFinder<UTexture2D> MapTexture(
		TEXT("/Game/BoldersDemo/Assets/UI/Minimap/T_VolcanoMinimap.T_VolcanoMinimap"));
	VolcanoMinimap = MapTexture.Object;
	static const ConstructorHelpers::FObjectFinder<UTexture2D> PlayerOrbs(
		TEXT("/Game/BoldersDemo/Assets/UI/Minimap/T_MinimapPlayerOrbs.T_MinimapPlayerOrbs"));
	MinimapPlayerOrbs = PlayerOrbs.Object;
	static const ConstructorHelpers::FObjectFinder<UTexture2D> Life(
		TEXT("/Game/BoldersDemo/Assets/UI/CombatHUD/Foundry/T_FoundryLifeFrame.T_FoundryLifeFrame"));
	static const ConstructorHelpers::FObjectFinder<UTexture2D> Kit(
		TEXT("/Game/BoldersDemo/Assets/UI/CombatHUD/Foundry/T_FoundryHudKit.T_FoundryHudKit"));
	static const ConstructorHelpers::FObjectFinder<UTexture2D> Dip(
		TEXT("/Game/BoldersDemo/Assets/UI/CombatHUD/Foundry/T_BladeDip.T_BladeDip"));
	static ConstructorHelpers::FObjectFinderOptional<UFoundryTypography> Lettering(UFoundryTypography::AssetPath);
	FoundryLifeFrame = Life.Object;
	FoundryHudKit = Kit.Object;
	BladeDipArt = Dip.Object;
	Typography = Lettering.Get();
}

void ABoldersHUD::DrawLabel(UCanvas& DrawCanvas, const FString& Text, const FVector2D& Position, float Scale,
	const FLinearColor& Color, bool bCentered)
{
	if (Text.IsEmpty()) return;
	const auto DrawFont = [&DrawCanvas, Scale, &Color](const FString& Value, const FVector2D& At, bool bCenter)
	{
		FCanvasTextItem Item(At, FText::FromString(Value), GEngine->GetSmallFont(), Color);
		Item.Scale = FVector2D(Scale, Scale);
		Item.bCentreX = bCenter;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, .95f));
		DrawCanvas.DrawItem(Item);
	};
	if (!Typography)
	{
		DrawFont(Text, Position, bCentered);
		return;
	}

	const float Height = 13.f * Scale;
	const auto DrawArt = [this, Height, &Color](const FFoundryTextSprite& Art, float X, float Y)
	{
		const FVector4& UV = Art.UV;
		DrawTexture(Art.Atlas, X, Y + 1.f, Height * Art.Aspect, Height,
			UV.X, UV.Y, UV.Z - UV.X, UV.W - UV.Y, Color);
	};
	if (const auto* Art = Typography->Find(Text))
	{
		DrawArt(*Art, Position.X - (bCentered ? Height * Art->Aspect * .5f : 0.f), Position.Y);
		return;
	}
	TArray<FString> Parts;
	Text.ParseIntoArrayWS(Parts);
	if (const auto* Art = Typography->Find(FString::Join(Parts, TEXT(" "))))
	{
		DrawArt(*Art, Position.X - (bCentered ? Height * Art->Aspect * .5f : 0.f), Position.Y);
		return;
	}

	// Fixed words can sit alongside live numeric values without baking those values into the textures.
	TArray<const FFoundryTextSprite*, TInlineAllocator<10>> ArtParts;
	TArray<float, TInlineAllocator<10>> Widths;
	TArray<float, TInlineAllocator<10>> Gaps;
	int32 Offset = 0;
	float TotalWidth = 0.f;
	bool bHasArt = false;
	for (const FString& Part : Parts)
	{
		const auto* Art = Typography->Find(Part);
		float Width;
		if (Art)
		{
			Width = Height * Art->Aspect;
			bHasArt = true;
		}
		else
		{
			float FontHeight;
			DrawCanvas.StrLen(GEngine->GetSmallFont(), Part, Width, FontHeight);
			Width *= Scale;
		}
		ArtParts.Add(Art);
		Widths.Add(Width);
		const int32 Start = Text.Find(Part, ESearchCase::CaseSensitive, ESearchDir::FromStart, Offset);
		const float Gap = Gaps.IsEmpty() ? 0.f : FMath::Clamp(Start - Offset, 1, 5) * 4.f * Scale;
		Gaps.Add(Gap);
		TotalWidth += Gap + Width;
		Offset = Start + Part.Len();
	}
	if (!bHasArt)
	{
		DrawFont(Text, Position, bCentered);
		return;
	}

	float X = Position.X - (bCentered ? TotalWidth * .5f : 0.f);
	for (int32 Part = 0; Part < Parts.Num(); ++Part)
	{
		X += Gaps[Part];
		if (ArtParts[Part]) DrawArt(*ArtParts[Part], X, Position.Y);
		else DrawFont(Parts[Part], {X, Position.Y}, false);
		X += Widths[Part];
	}
}

void ABoldersHUD::DrawKit(const FVector4& Crop, const FVector2D& Position, const FVector2D& Size, FLinearColor Tint)
{
	if (!FoundryHudKit) return;
	constexpr double SourceSize = 1254.0;
	DrawTexture(FoundryHudKit, Position.X, Position.Y, Size.X, Size.Y,
		Crop.X / SourceSize, Crop.Y / SourceSize, (Crop.Z - Crop.X) / SourceSize, (Crop.W - Crop.Y) / SourceSize, Tint);
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
	FVector4 Bounds = HealthBarFillBounds;
	if (FoundryLifeFrame)
	{
		DrawTexture(FoundryLifeFrame, X, Y, Width, Height, 12.f / 2175.f, 203.f / 723.f, 2151.f / 2175.f, 314.f / 723.f);
		Bounds = FVector4(.104, .335, .896, .665);
	}
	else if (HealthBarFrame)
	{
		const FVector4& UV = HealthBarFrameUV;
		DrawTexture(HealthBarFrame, X, Y, Width, Height, UV.X, UV.Y, UV.Z - UV.X, UV.W - UV.Y);
	}
	else
	{
		DrawRect(FLinearColor(.28f, .21f, .13f), X, Y, Width, Height);
		DrawRect(FLinearColor(.025f, .022f, .026f), X + 1, Y + 1, Width - 2, Height - 2);
	}

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
	const float X = 23.f * Scale;
	const float Width = 206.f * Scale;
	float Y = 20.f * Scale;
	for (const ABaseCharacter* Member : Members)
	{
		const bool bLocal = Member == LocalCharacter;
		const FLinearColor Color = Member->IsAlive() ? PartyColor(*Member) : Muted;
		DrawMinimapOrb(Member->PlayerSlot, {X + 11.f * Scale, Y + 22.f * Scale}, (bLocal ? 12.f : 10.f) * Scale);
		DrawLabel(*Canvas, RoleName(*Member), {X + 31.f * Scale, Y + 1.f * Scale}, (bLocal ? 1.02f : .94f) * Scale, Body);

		const FString Health = Member->IsAlive() ? HealthLabel(*Member) : TEXT("已倒下");
		float TextWidth, TextHeight;
		GetTextSize(Health, TextWidth, TextHeight, GEngine->GetSmallFont(), .83f * Scale);
		DrawLabel(*Canvas, Health, {X + Width - 11.f * Scale - TextWidth, Y + 3.f * Scale}, .83f * Scale, Body);
		DrawHealthBar(*Member, X + 27.f * Scale, Y + 15.f * Scale, Width - 27.f * Scale, 25.f * Scale, Color);
		Y += 45.f * Scale;
	}
}

void ABoldersHUD::DrawEnemyHealth(const ABaseCharacter& Character, float Scale)
{
	if (!Character.IsAlive()) return;
	const FVector Head = Character.GetOverheadAnchor(25.f);
	const FVector P = Canvas->Project(Head);
	const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	const bool bFocused = PC && (PC->GetCursorTarget() == &Character || PC->GetSelectedEnemy() == &Character);
	const float Width = (bFocused ? 94.f : 71.f) * Scale;
	const float Height = (bFocused ? 10.f : 8.f) * Scale;
	if (P.Z <= 0 || P.X < Width * .5f || P.X > Canvas->SizeX - Width * .5f || P.Y < 22.f * Scale || P.Y > Canvas->SizeY)
	{
		return;
	}

	DrawHealthBar(Character, P.X - Width * .5f, P.Y - Height, Width, Height, FLinearColor(.92f, .16f, .17f));
	if (bFocused)
	{
		DrawLabel(*Canvas, HealthLabel(Character), {P.X, P.Y - 26.f * Scale}, .83f * Scale, FLinearColor::White, true);
	}

	const TCHAR* AuraLabel = nullptr;
	FLinearColor AuraColor;
	switch (Character.GetElementalAura())
	{
	case EElementalAura::Heat: AuraLabel = TEXT("灼热"); AuraColor = FLinearColor(1.f, .48f, .19f); break;
	case EElementalAura::Chill: AuraLabel = TEXT("寒意"); AuraColor = FLinearColor(.58f, .87f, 1.f); break;
	case EElementalAura::Wet: AuraLabel = TEXT("潮湿"); AuraColor = FLinearColor(.18f, .66f, 1.f); break;
	case EElementalAura::Charge: AuraLabel = TEXT("带电"); AuraColor = FLinearColor(.84f, .51f, 1.f); break;
	case EElementalAura::Frozen: AuraLabel = TEXT("冻结"); AuraColor = FLinearColor(.49f, 1.f, 1.f); break;
	default: break;
	}
	if (AuraLabel)
		DrawLabel(*Canvas, AuraLabel, {P.X, P.Y + 2.f * Scale}, .77f * Scale, AuraColor, true);
}

void ABoldersHUD::DrawAttackIcons(const ABaseCharacter& Character, float Scale, EDamageElement DipElement, bool bInVolcano)
{
	const FCombatHUDLoadout* Loadout = Loadouts.FindByPredicate([&Character](const FCombatHUDLoadout& Entry)
	{
		return Entry.CharacterClass && Character.IsA(Entry.CharacterClass);
	});
	const UAttackLoadout* Attacks = Character.Combat->AttackLoadout;
	if (!Attacks || Attacks->Modules.IsEmpty()) return;

	const bool bWarrior = bInVolcano && Character.IsA<AWarriorCharacter>();
	const int32 Count = Attacks->Modules.Num() + (bWarrior ? 1 : 0);
	const float Step = 79.f * Scale;
	const float CenterY = Canvas->SizeY - 65.f * Scale;
	float X = (Canvas->SizeX - Step * (Count - 1)) * .5f;
	const auto DrawSocket = [this, Scale, CenterY, &X](bool bEmphasized, const FString& Key, const FString& Title,
		const FLinearColor& Accent, const FName& HitBox)
	{
		const float Radius = (bEmphasized ? 35.f : 31.f) * Scale;
		const FVector2D Center(X, CenterY);
		DrawKit(SocketArt, Center - FVector2D(Radius, Radius), FVector2D(Radius * 2.f, Radius * 2.f),
			bEmphasized ? FLinearColor::White : FLinearColor(.78f, .74f, .68f));
		if (bEmphasized)
			DrawMapDiamond(*Canvas, Center + FVector2D(0, Radius + 4.f * Scale), 3.5f * Scale, Accent);
		DrawKit(BannerArt, Center + FVector2D(-12.f, -39.f) * Scale, FVector2D(24.f, 14.f) * Scale);
		DrawLabel(*Canvas, Key, Center + FVector2D(0, -38.f * Scale), .76f * Scale, Body, true);
		DrawLabel(*Canvas, Title, Center + FVector2D(0, 44.f * Scale), .91f * Scale, bEmphasized ? Accent : Body, true);
		AddHitBox(Center - FVector2D(37.f, 40.f) * Scale, FVector2D(74.f, 98.f) * Scale, HitBox, true);
		return Radius;
	};
	const auto ShadeCooldown = [this, Scale, CenterY, &X](float Remaining, float Duration, float Radius)
	{
		if (Remaining <= 0.f) return;
		DrawIconDisk(*Canvas, Canvas->DefaultTexture, {X, CenterY}, Radius, FullUV,
			FLinearColor(.017f, .014f, .02f, .81f), FMath::Clamp(Remaining / FMath::Max(Duration, .01f), 0.f, 1.f));
		DrawLabel(*Canvas, FString::Printf(TEXT("%.1f"), Remaining), {X, CenterY - 7.f * Scale},
			1.11f * Scale, FLinearColor::White, true);
	};
	for (int32 Slot = 0; Slot < Attacks->Modules.Num(); ++Slot)
	{
		const FAttackModule& Attack = Attacks->Modules[Slot];
		const bool bActive = Character.Combat->GetActiveModuleId() == Attack.Id;
		const FLinearColor Accent = CombatGuideStyle::ElementColor(Attack.Element);
		const float Radius = DrawSocket(bActive, FString::FromInt(Slot + 1), Attack.Label.ToString(), Accent, Attack.Id);
		const auto* Icon = Loadout ? Loadout->Icons.FindByPredicate([&Attack](const FCombatHUDIcon& Entry) { return Entry.Id == Attack.Id; }) : nullptr;
		if (Icon && Loadout->Atlas)
			DrawIconDisk(*Canvas, Loadout->Atlas, {X, CenterY}, Radius * .81f, Icon->UV,
				bActive ? FLinearColor::White : FLinearColor(.82f, .84f, .88f));
		ShadeCooldown(Character.Combat->GetCooldownRemaining(Attack.Id), Attack.Cooldown, Radius * .81f);
		X += Step;
	}

	if (!bWarrior) return;
	const EDamageElement Coating = Character.Combat->GetBladeCoating();
	const bool bCoated = Coating != EDamageElement::Physical;
	const bool bReady = DipElement != EDamageElement::Physical;
	const float Cooldown = Character.Combat->GetBladeDipCooldown();
	const FLinearColor Accent = bCoated || bReady
		? CombatGuideStyle::ElementColor(bCoated ? Coating : DipElement) : Muted;
	const float Radius = DrawSocket(bCoated || bReady, TEXT("F"), BladeElementName(Coating), Accent, BladeDipBox);
	if (BladeDipArt)
	{
		const float Size = Radius * 1.52f;
		DrawTexture(BladeDipArt, X - Size * .5f, CenterY - Size * .5f, Size, Size, 0, 0, 1, 1,
			bCoated || bReady ? FLinearColor::White : FLinearColor(.66f, .69f, .73f));
	}
	if (bCoated)
		DrawLabel(*Canvas, FString::Printf(TEXT("%.1f"), Character.Combat->GetBladeCoatingRemaining()),
			{X, CenterY + 11.f * Scale}, .93f * Scale, Accent, true);
	else ShadeCooldown(Cooldown, 11.f, Radius * .81f);
}

void ABoldersHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	if (BoxName == RestartMatchBox)
	{
		if (auto* PC = Cast<ABoldersPlayerController>(PlayerOwner)) PC->HandleRestartRequest();
		return;
	}
	if (BoxName == IslandMapOpenBox || BoxName == IslandMapCloseBox)
	{
		bIslandMapExpanded = BoxName == IslandMapOpenBox;
		CloseWaySign();
		return;
	}
	if (BoxName == WaySignOpenBox || BoxName == WaySignPlaqueBox)
	{
		const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
		const ABaseCharacter* Character = PC ? PC->GetAssignedCharacter() : nullptr;
		const AVolcanoWaySign* Sign = NearbySign.Get();
		if (IsValid(Character) && Sign && Sign->CanRead(*Character)) ReadingSign = Sign;
		return;
	}
	if (BoxName == WaySignCloseBox)
	{
		CloseWaySign();
		return;
	}
	auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	if (PC && BoxName == BladeDipBox)
	{
		PC->RequestBladeDip();
		return;
	}
	const auto* Character = PC ? PC->GetAssignedCharacter() : nullptr;
	const UAttackLoadout* Attacks = Character ? Character->Combat->AttackLoadout.Get() : nullptr;
	if (!Attacks) return;
	const int32 Slot = Attacks->Modules.IndexOfByPredicate([BoxName](const FAttackModule& Attack) { return Attack.Id == BoxName; });
	if (Slot != INDEX_NONE) PC->SelectAttackSlot(Slot);
}

bool ABoldersHUD::CloseIslandMap()
{
	const bool bWasExpanded = bIslandMapExpanded;
	bIslandMapExpanded = false;
	return bWasExpanded;
}

bool ABoldersHUD::CloseWaySign()
{
	const bool bWasReading = ReadingSign.IsValid();
	ReadingSign.Reset();
	return bWasReading;
}

void ABoldersHUD::ReadNearestWaySign()
{
	if (bIslandMapExpanded || ReadingSign.IsValid()) return;
	const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	if (const auto* Character = PC ? PC->GetAssignedCharacter() : nullptr; IsValid(Character))
		ReadingSign = AVolcanoWaySign::FindReadable(*Character);
}

void ABoldersHUD::DrawCombatControls(const ABaseCharacter& Character, const ABoldersPlayerController& PC, float Scale,
	EDamageElement DipElement, const FVector& DipContact)
{
	const auto* Attack = Character.Combat->GetActiveModule();
	if (!Attack) return;

	const float Center = Canvas->SizeX * .5f;
	const TCHAR* Feedback = GetOrderLabel(PC.GetRecentAttackOrderResult(), Character.Combat->GetOrderPhase());
	const auto* Hazard = Cast<AVolcanoHazard>(PC.GetCursorTarget());
	if (PC.GetRecentAttackOrderResult() == EAttackOrderResult::Ineffective && Hazard
		&& !Hazard->CanReactTo(Character.Combat->GetEffectiveAttackElement(Attack)))
		Feedback = GetHazardRejection(*Hazard);
	if (!Feedback && PC.HadRecentBladeDipRejection())
		Feedback = TEXT("需靠近可用的冷却槽");
	if (!Feedback)
	{
		const AActor* Target = PC.GetCursorTarget();
		if ((!Hazard || Hazard->CanReactTo(Character.Combat->GetEffectiveAttackElement(Attack)))
			&& AttackPositionRules::CanTarget(Character, Target))
		{
			const FVector Origin = Character.GetActorLocation();
			if (!AttackPositionRules::IsInRange(Character, *Target, Attack->Range, Origin))
				Feedback = TEXT("超出攻距");
			else if (!AttackPositionRules::CanAttack(Character, *Target, Attack->Range, Origin))
				Feedback = TEXT("路线受阻");
		}
	}
	if (Feedback) DrawLabel(*Canvas, Feedback, {Center, Canvas->SizeY - 134.f * Scale}, .91f * Scale, Danger, true);

	if (DipElement != EDamageElement::Physical)
	{
		const FVector Anchor = Canvas->Project(DipContact + FVector(0, 0, 44));
		if (Anchor.Z <= 0) return;
		const FVector2D Point(Anchor.X, Anchor.Y);
		DrawKit(SocketArt, Point - FVector2D(16, 18) * Scale, FVector2D(32, 32) * Scale);
		if (BladeDipArt)
			DrawTexture(BladeDipArt, Point.X - 10.f * Scale, Point.Y - 12.f * Scale, 20.f * Scale, 20.f * Scale, 0, 0, 1, 1);
		DrawLabel(*Canvas, TEXT("F"), Point + FVector2D(0, 12.f * Scale), .87f * Scale, CombatGuideStyle::ElementColor(DipElement), true);
	}
}

void ABoldersHUD::DrawMinimapOrb(int32 PlayerSlot, const FVector2D& Center, float Radius)
{
	if (!MinimapPlayerOrbs) return;
	// Measured tiles in the original 1261 x 1247 atlas. Use normalized UVs at every mip.
	const float U = (PlayerSlot == 2 ? 680.f : 69.f) / 1261.f;
	DrawTexture(MinimapPlayerOrbs, Center.X - Radius, Center.Y - Radius, Radius * 2.f, Radius * 2.f,
		U, 376.f / 1247.f, 504.f / 1261.f, 504.f / 1247.f);
}

void ABoldersHUD::DrawMinimapPlayerMarker(int32 PlayerSlot, const FVector2D& Point, float Radius,
	const FVector2D& Heading)
{
	const FLinearColor Ring = PlayerSlot == 2 ? FLinearColor(1.f, .73f, .30f) : MapPlayerOne;
	DrawIconDisk(*Canvas, Canvas->DefaultTexture, Point, Radius * 1.14f, FullUV, MapMarkerEdge);
	DrawIconDisk(*Canvas, Canvas->DefaultTexture, Point, Radius, FullUV, Ring);
	DrawIconDisk(*Canvas, Canvas->DefaultTexture, Point, Radius * .74f, FullUV, MapMarkerEdge);
	DrawLabel(*Canvas, FString::FromInt(PlayerSlot), Point - FVector2D(0, Radius * .67f),
		Radius * .097f, FLinearColor::White, true);

	if (Heading.IsNearlyZero()) return;
	const FVector2D Right(-Heading.Y, Heading.X);
	const FVector2D Base = Point + Heading * Radius * 1.25f;
	const FVector2D InnerBase = Point + Heading * Radius * 1.4f;
	DrawMapTriangle(*Canvas, Point + Heading * Radius * 2.25f,
		Base + Right * Radius * .68f, Base - Right * Radius * .68f, MapMarkerEdge);
	DrawMapTriangle(*Canvas, Point + Heading * Radius * 2.03f,
		InnerBase + Right * Radius * .44f, InnerBase - Right * Radius * .44f, FLinearColor::White);
}

void ABoldersHUD::DrawIslandMinimap(const AVolcanoEncounter& Encounter, const ABaseCharacter& Character,
	const TArray<ABaseCharacter*>& Party, float Scale)
{
	const bool bExpanded = bIslandMapExpanded;
	const float Size = bExpanded
		? FMath::Min3(Canvas->SizeX - 60.f * Scale, Canvas->SizeY - 112.f * Scale, 690.f * Scale)
		: 207.f * Scale;
	const float Border = (bExpanded ? 8.f : 4.f) * Scale;
	const float Header = bExpanded ? 41.f * Scale : Border;
	const float Footer = bExpanded ? 36.f * Scale : Border;
	const FVector2D PanelSize(Size + 2.f * Border, Header + Size + Footer);
	const FVector2D PanelTopLeft = bExpanded
		? (FVector2D(Canvas->SizeX, Canvas->SizeY) - PanelSize) * .5
		: FVector2D(Canvas->SizeX - PanelSize.X - 20.f * Scale, 20.f * Scale);
	const FVector2D MapTopLeft = PanelTopLeft + FVector2D(Border, Header);

	if (bExpanded)
	{
		DrawRect(FLinearColor(.006f, .009f, .015f, .78f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		AddHitBox(FVector2D::ZeroVector, FVector2D(Canvas->SizeX, Canvas->SizeY), IslandMapCloseBox, true, 100);
	}
	else AddHitBox(PanelTopLeft, PanelSize, IslandMapOpenBox, true, 10);

	DrawTexture(VolcanoMinimap, MapTopLeft.X, MapTopLeft.Y, Size, Size, 0.f, 0.f, 1.f, 1.f);
	DrawKit(MapTrimArt, MapTopLeft - FVector2D(Border, Border), FVector2D(Size + 2.f * Border));
	if (bExpanded)
	{
		DrawKit(BannerArt, PanelTopLeft + FVector2D(0, -1.f * Scale),
			{PanelSize.X, 35.f * Scale});
		DrawLabel(*Canvas, TEXT("火山铸造岛"), PanelTopLeft + FVector2D(19, 8) * Scale, 1.1f * Scale, Gold);
		DrawLabel(*Canvas, TEXT("Esc  收起"), PanelTopLeft + FVector2D(PanelSize.X - 76.f * Scale, 10.f * Scale),
			.85f * Scale, Body);
	}
	else
	{
		const FVector2D Button = MapTopLeft + FVector2D(Size - 15.f * Scale, 15.f * Scale);
		DrawKit(SocketArt, Button - FVector2D(12, 12) * Scale, FVector2D(24, 24) * Scale);
		DrawLabel(*Canvas, TEXT("+"), Button - FVector2D(0, 9.f * Scale), 1.24f * Scale, Gold, true);
	}

	const auto KeepMarkerInside = [MapTopLeft, Size, Scale, bExpanded](const FVector2D& Point, double Margin = 0.0)
	{
		const FVector2D Offset = Point - MapTopLeft;
		const double Inset = FMath::Max((bExpanded ? 14.0 : 11.0) * Scale, Margin);
		return MapTopLeft + FVector2D(FMath::Clamp(Offset.X, Inset, Size - Inset),
			FMath::Clamp(Offset.Y, Inset, Size - Inset));
	};
	const auto MapPoint = [MapTopLeft, Size, &KeepMarkerInside](const AActor& Actor) -> TOptional<FVector2D>
	{
		// Orthographic terrain reference: center (1080, -50), width 144 m, down-map is world +Y.
		const FVector2D Fraction = (FVector2D(Actor.GetActorLocation()) - FVector2D(1080, -50)) / 14400.0 + FVector2D(.5, .5);
		if ((Fraction - FVector2D(.5, .5)).GetAbsMax() > .5) return {};
		return KeepMarkerInside(MapTopLeft + Fraction * Size);
	};
	const auto Dot = [this, Scale, &MapPoint, bExpanded](const AActor& Actor, float Radius, FLinearColor Color)
	{
		const TOptional<FVector2D> Point = MapPoint(Actor);
		if (!Point.IsSet()) return;
		const float R = Radius * Scale * (bExpanded ? 1.4f : 1.f);
		const float Edge = Scale;
		DrawMapDiamond(*Canvas, Point.GetValue(), R + Edge, MapMarkerEdge);
		DrawMapDiamond(*Canvas, Point.GetValue(), R, Color);
	};
	for (TActorIterator<AVolcanoHazard> It(GetWorld()); It; ++It)
	{
		if (It->HasGroundWarning() && It->GetHazardKind() != EVolcanoHazardKind::Puddle)
			Dot(**It, 2.1f, Danger);
		else if (It->GetHazardKind() == EVolcanoHazardKind::Puddle)
		{
			const EVolcanoHazardState State = It->GetHazardState();
			const EDamageElement Element = State == EVolcanoHazardState::Frozen ? EDamageElement::Ice
				: State == EVolcanoHazardState::Active ? EDamageElement::Lightning : EDamageElement::Water;
			Dot(**It, 1.7f, CombatGuideStyle::ElementColor(Element));
		}
	}
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFaction() != ECharacterFaction::Enemy || !It->IsAlive() || It->IsHidden()) continue;
		const auto* Boss = Cast<AStatueBossCharacter>(*It);
		if (Boss && !Boss->IsAwakened()) continue;
		Dot(**It, Boss ? 3.4f : 2.2f, FLinearColor(.97f, .25f, .23f));
	}
	const float PlayerRadius = (bExpanded ? 13.f : 10.f) * Scale;
	const FVector2D Heading = FVector2D(Character.GetActorForwardVector()).GetSafeNormal();
	TOptional<FVector2D> Player = Character.IsAlive() ? MapPoint(Character) : TOptional<FVector2D>();
	if (Player.IsSet()) Player = KeepMarkerInside(Player.GetValue(), PlayerRadius * 2.25f);
	if (const AActor* Objective = Encounter.GetTravelObjective())
	{
		if (const TOptional<FVector2D> Anchor = MapPoint(*Objective); Anchor.IsSet())
		{
			const float Radius = (bExpanded ? 10.f : 7.3f) * Scale;
			FVector2D Point = Anchor.GetValue();
			if (Player.IsSet())
			{
				const FVector2D Offset = Point - Player.GetValue();
				const FVector2D Right(-Heading.Y, Heading.X);
				const double Along = FVector2D::DotProduct(Offset, Heading);
				const double Side = FVector2D::DotProduct(Offset, Right);
				const double Clearance = Radius + PlayerRadius * .83 + 2.f * Scale;
				if (Along > -Clearance && Along < PlayerRadius * 2.25 + Radius && FMath::Abs(Side) < Clearance)
				{
					Point = KeepMarkerInside(Point + Right * (FMath::Sign(Side) * Clearance - Side));
					DrawLine(Anchor->X, Anchor->Y, Point.X, Point.Y, Gold, Scale);
				}
			}
			DrawMapDiamond(*Canvas, Point, Radius + 1.6f * Scale, MapMarkerEdge);
			DrawMapDiamond(*Canvas, Point, Radius, FLinearColor(1.f, .66f, .19f));
			DrawMapDiamond(*Canvas, Point, Radius * .47f, FLinearColor(1.f, .94f, .69f));
		}
	}
	const float TeammateRadius = (bExpanded ? 10.f : 7.4f) * Scale;
	for (const ABaseCharacter* Member : Party)
	{
		if (Member == &Character || !Member->IsAlive()) continue;
		const TOptional<FVector2D> Teammate = MapPoint(*Member);
		if (!Teammate.IsSet()) continue;
		const FVector2D Anchor = Teammate.GetValue();
		FVector2D Point = Anchor;
		const float Separation = PlayerRadius + TeammateRadius + 4.f * Scale;
		if (Player.IsSet() && FVector2D::DistSquared(Anchor, Player.GetValue()) < FMath::Square(Separation))
		{
			FVector2D Direction = (Anchor - Player.GetValue()).GetSafeNormal();
			if (Direction.IsNearlyZero()) Direction = FVector2D(1, 0);
			Point = KeepMarkerInside(Player.GetValue() + Direction * Separation);
			DrawLine(Anchor.X, Anchor.Y, Point.X, Point.Y, PartyColor(*Member), Scale);
		}
		const FVector2D Facing = FVector2D(Member->GetActorForwardVector()).GetSafeNormal();
		DrawMinimapPlayerMarker(Member->PlayerSlot, KeepMarkerInside(Point, TeammateRadius * 2.25f), TeammateRadius, Facing);
	}
	if (Player.IsSet()) DrawMinimapPlayerMarker(Character.PlayerSlot, Player.GetValue(), PlayerRadius, Heading);
	if (bExpanded)
	{
		DrawKit(BannerArt, MapTopLeft + FVector2D(0, Size + 4.f * Scale), FVector2D(176, 29) * Scale);
		const FVector2D Legend = MapTopLeft + FVector2D(12.f * Scale, Size + Footer * .5f);
		DrawMinimapPlayerMarker(1, Legend, 6.f * Scale, FVector2D(0, -1));
		DrawLabel(*Canvas, TEXT("1P"), Legend + FVector2D(12, -7) * Scale, .86f * Scale, Body);
		DrawMinimapPlayerMarker(2, Legend + FVector2D(58.f * Scale, 0), 6.f * Scale, FVector2D(0, -1));
		DrawLabel(*Canvas, TEXT("2P"), Legend + FVector2D(70, -7) * Scale, .86f * Scale, Body);
		DrawMapDiamond(*Canvas, Legend + FVector2D(113.f * Scale, 0), 5.f * Scale, Gold);
		DrawLabel(*Canvas, TEXT("目标"), Legend + FVector2D(125, -7) * Scale, .86f * Scale, Body);
	}
}

void ABoldersHUD::DrawWaySign(const ABaseCharacter& Character, float Scale)
{
	if (bIslandMapExpanded)
	{
		NearbySign.Reset();
		return;
	}
	if (const AVolcanoWaySign* Sign = ReadingSign.Get())
	{
		const TArray<FText> Instructions = Sign->GetInstructions();
		const float Width = FMath::Min(430.f * Scale, Canvas->SizeX - 40.f * Scale);
		const float Height = (77.f + 22.f * Instructions.Num()) * Scale;
		const float Center = Canvas->SizeX * .5f;
		const FVector2D Origin(Center - Width * .5f, Canvas->SizeY * .36f);
		DrawRect(FLinearColor(.008f, .01f, .017f, .55f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		DrawKit(PanelArt, Origin, {Width, Height});
		DrawLabel(*Canvas, Sign->GetTitle().ToString(), {Center, Origin.Y + 18.f * Scale}, 1.19f * Scale, Gold, true);
		float Y = Origin.Y + 46.f * Scale;
		for (const FText& Line : Instructions)
		{
			DrawLabel(*Canvas, Line.ToString(), {Center, Y}, .95f * Scale, Body, true);
			Y += 22.f * Scale;
		}
		DrawLabel(*Canvas, TEXT("Esc  /  点击收起"), {Center, Y + 4.f * Scale}, .82f * Scale, Muted, true);
		AddHitBox(FVector2D::ZeroVector, FVector2D(Canvas->SizeX, Canvas->SizeY), WaySignCloseBox, true, 120);
		return;
	}

	const AVolcanoWaySign* Sign = AVolcanoWaySign::FindReadable(Character);
	NearbySign = Sign;
	if (!Sign) return;
	const FVector Projected = Canvas->Project(Sign->GetPlaqueLocation());
	if (Projected.Z <= 0.f) return;
	const FVector2D Point(Projected.X, Projected.Y);
	const FVector2D Size = FVector2D(142, 24) * Scale;
	const FVector2D Origin = Point - FVector2D(Size.X * .5, 66.f * Scale);
	DrawKit(BannerArt, Origin, Size);
	DrawLabel(*Canvas, FString(TEXT("E   ")) + Sign->GetTitle().ToString(), Origin + FVector2D(Size.X * .5, 4.f * Scale),
		.9f * Scale, Gold, true);
	AddHitBox(Origin, Size, WaySignOpenBox, true, 12);
	AddHitBox(Point - FVector2D(52, 39) * Scale, FVector2D(104, 78) * Scale, WaySignPlaqueBox, true, 12);
}

void ABoldersHUD::DrawVolcano(const AVolcanoEncounter& Encounter, const ABoldersPlayerController& PC, float Scale)
{
	const float Center = Canvas->SizeX * .5f;
	const float Top = 20.f * Scale;
	const auto Stage = Encounter.GetStage();
	const auto* Boss = Encounter.GetBoss();
	if (Encounter.IsFinished())
	{
		const bool bVictory = Stage == EVolcanoEncounterStage::Victory;
		const FLinearColor Accent = bVictory ? Gold : Danger;
		const auto& Run = Encounter.GetRunResult();
		const int32 Seconds = FMath::FloorToInt(Run.Seconds);
		const float Y = Canvas->SizeY * .24f;
		DrawRect(FLinearColor(.014f, .012f, .017f, .47f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		DrawKit(PanelArt, {Center - 193.f * Scale, Y}, FVector2D(386, 172) * Scale);
		DrawLabel(*Canvas, bVictory ? TEXT("铸造岛已夺回") : TEXT("全队倒下"), {Center, Y + 22.f * Scale}, 1.58f * Scale, Accent, true);
		DrawLabel(*Canvas, FString::Printf(TEXT("战斗用时  %02d:%02d"), Seconds / 60, Seconds % 60),
			{Center, Y + 53.f * Scale}, 1.f * Scale, Body, true);
		FString Party;
		for (const auto& Member : Run.Party)
		{
			if (!Party.IsEmpty()) Party += TEXT("        ");
			Party += Member.Health > 0.f ? FString::Printf(TEXT("%dP  %.0f / %.0f"), Member.Slot, Member.Health, Member.MaxHealth)
				: FString::Printf(TEXT("%dP  已倒下"), Member.Slot);
		}
		DrawLabel(*Canvas, Party, {Center, Y + 80.f * Scale}, .94f * Scale, Body, true);
		const FVector2D Button(Center - 76.f * Scale, Y + 111.f * Scale);
		const FVector2D Size = FVector2D(152, 37) * Scale;
		DrawKit(BannerArt, Button, Size);
		DrawLabel(*Canvas, TEXT("再战  [R]"), {Center, Button.Y + 9.f * Scale}, 1.09f * Scale, Gold, true);
		AddHitBox(Button, Size, RestartMatchBox, true, 110);
		return;
	}
	if (Stage == EVolcanoEncounterStage::Guardians && Encounter.GetRemainingGuardians() > 0)
	{
		DrawKit(BannerArt, {Center - 142.f * Scale, Top}, FVector2D(284, 34) * Scale);
		const FString Status = FString::Printf(TEXT("守卫  %d / 2 波        剩余 %d"),
			Encounter.GetWave(), Encounter.GetRemainingGuardians());
		DrawLabel(*Canvas, Status, {Center, Top + 9.f * Scale}, 1.02f * Scale, Gold, true);
	}
	if (Stage == EVolcanoEncounterStage::Statue && IsValid(Boss))
	{
		FString Action;
		switch (Boss->GetCurrentAction())
		{
		case EStatueBossAction::Awakening: Action = TEXT("苏醒"); break;
		case EStatueBossAction::Enraging: Action = TEXT("狂暴蓄势"); break;
		case EStatueBossAction::Staggered: Action = TEXT("失衡"); break;
		case EStatueBossAction::AxeSlam: Action = TEXT("重砸"); break;
		case EStatueBossAction::Cleave: Action = TEXT("旋扫"); break;
		case EStatueBossAction::LeapingSlam: Action = TEXT("跳劈"); break;
		default: break;
		}
		if (Boss->IsRecovering() && Boss->GetCurrentAction() != EStatueBossAction::Staggered) Action = TEXT("收势");
		const TCHAR* Title = Boss->IsEnraged() ? TEXT("狂暴熔炉骑士") : TEXT("熔炉骑士");
		DrawLabel(*Canvas, Title, {Center, Top - 7.f * Scale}, 2.30f * Scale, FLinearColor::White, true);
		DrawHealthBar(*Boss, Center - 186.f * Scale, Top + 23.f * Scale, 372.f * Scale, 31.f * Scale,
			Boss->IsEnraged() ? FLinearColor(1.f, .27f, .055f) : FLinearColor(.83f, .12f, .07f));
		if (!Action.IsEmpty()) DrawLabel(*Canvas, Action, {Center, Top + 52.f * Scale}, 1.06f * Scale, Body, true);
	}

	const auto* Character = PC.GetAssignedCharacter();
	if (!IsValid(Character)) return;
	if (!Character->IsAlive())
	{
		const FVector2D Origin(Center - 151.f * Scale, Canvas->SizeY - 99.f * Scale);
		DrawKit(BannerArt, Origin, FVector2D(302, 36) * Scale);
		const TCHAR* Downed = Stage == EVolcanoEncounterStage::Statue ? TEXT("已倒下    等待本局结算")
			: TEXT("已倒下    等队友推进后复活");
		DrawLabel(*Canvas, Downed, {Center, Origin.Y + 9.f * Scale},
			.95f * Scale, Body, true);
		return;
	}
	const AActor* Cursor = PC.GetCursorTarget();
	FTargetLabel Inspected;
	if (const auto* Hazard = Cast<AVolcanoHazard>(Cursor))
	{
		Inspected = GetHazardLabel(*Hazard);
		if (!Hazard->CanReactTo(Character->Combat->GetEffectiveAttackElement(Character->Combat->GetActiveModule())))
			Inspected.Hint = GetHazardRejection(*Hazard);
	}
	else if (const auto* Guardian = Cursor ? Cast<ABaseCharacter>(Cursor) : PC.GetSelectedEnemy())
		Inspected = GetGuardianLabel(Encounter, *Guardian);
	if (Inspected.Name.IsEmpty()) return;

	const float Width = 261.f * Scale;
	const FVector2D Position(Canvas->SizeX - Width - 22.f * Scale, Canvas->SizeY - 141.f * Scale);
	DrawKit(PanelArt, Position, {Width, 58.f * Scale});
	DrawLabel(*Canvas, Inspected.Name + TEXT("    ") + Inspected.State, Position + FVector2D(15, 8) * Scale,
		.92f * Scale, Inspected.Accent);
	DrawLabel(*Canvas, Inspected.Hint, Position + FVector2D(15, 31) * Scale, .85f * Scale, Body);
}

void ABoldersHUD::PostRender()
{
	if (!bShowHUD || bShowDebugInfo)
	{
		CloseIslandMap();
		CloseWaySign();
		UpdateAttackGuide(nullptr, nullptr, false);
	}
	Super::PostRender();
}

void ABoldersHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	const auto* PC = Cast<ABoldersPlayerController>(PlayerOwner);
	if (PC && PC->IsChoosingClass())
	{
		CloseIslandMap();
		CloseWaySign();
		UpdateAttackGuide(nullptr, nullptr, false);
		return;
	}

	for (auto It = HealthPresentation.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.f, Canvas->SizeY / 760.f), .48f, 1.35f);
	const auto* Encounter = PC ? PC->GetEncounter() : nullptr;
	const bool bFinished = Encounter && Encounter->IsFinished();
	const ABaseCharacter* LocalCharacter = PC ? PC->GetAssignedCharacter() : nullptr;
	if (const AVolcanoWaySign* Sign = ReadingSign.Get(); Sign
		&& (!IsValid(LocalCharacter) || bFinished || !Sign->CanRead(*LocalCharacter))) CloseWaySign();
	const bool bCanShowMap = Encounter && IsValid(LocalCharacter) && !bFinished && VolcanoMinimap
		&& Canvas->SizeX >= 700 && Canvas->SizeY >= 520
		&& GetWorld()->GetMapName().EndsWith(TEXT("L_VolcanoCombat"));
	if (!bCanShowMap) CloseIslandMap();
	UpdateAttackGuide(LocalCharacter, PC, !bFinished && !bIslandMapExpanded && !ReadingSign.IsValid());
	TArray<ABaseCharacter*> Party;
	for (TActorIterator<ABaseCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFaction() == ECharacterFaction::Enemy && (!Encounter || Encounter->GetBoss() != *It))
			DrawEnemyHealth(**It, Scale);
		if (It->PlayerSlot <= 0) continue;

		Party.Add(*It);
	}
	Party.Sort([](const ABaseCharacter& A, const ABaseCharacter& B) { return A.PlayerSlot < B.PlayerSlot; });
	DrawParty(Party, LocalCharacter, Scale);
	if (IsValid(LocalCharacter) && LocalCharacter->IsAlive() && !bFinished)
	{
		EDamageElement DipElement = EDamageElement::Physical;
		FVector DipContact = FVector::ZeroVector;
		if (Encounter && LocalCharacter->IsA<AWarriorCharacter>() && !LocalCharacter->IsFrozen()
			&& LocalCharacter->Combat->GetBladeDipCooldown() <= 0.f
			&& LocalCharacter->Combat->GetOrderPhase() != EAttackOrderPhase::Windup)
			LocalCharacter->Combat->FindBladeDipSource(DipElement, DipContact);
		DrawAttackIcons(*LocalCharacter, Scale, DipElement, Encounter != nullptr);
		DrawCombatControls(*LocalCharacter, *PC, Scale, DipElement, DipContact);
	}
	if (Encounter && PC) DrawVolcano(*Encounter, *PC, Scale);
	if (bCanShowMap) DrawIslandMinimap(*Encounter, *LocalCharacter, Party, Scale);
	if (Encounter && !bFinished && IsValid(LocalCharacter) && LocalCharacter->IsAlive())
		DrawWaySign(*LocalCharacter, Scale);
}
