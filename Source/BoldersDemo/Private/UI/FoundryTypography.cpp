#include "UI/FoundryTypography.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

bool UFoundryTypography::Apply(UImage* Image, const FString& Text) const
{
	const FFoundryTextSprite* Sprite = Find(Text);
	if (!Image || !Sprite || !Sprite->Atlas) return false;
	FSlateBrush Brush = Image->GetBrush();
	const FVector4& UV = Sprite->UV;
	Brush.SetResourceObject(Sprite->Atlas);
	Brush.SetUVRegion(FBox2f(FVector2f(UV.X, UV.Y), FVector2f(UV.Z, UV.W)));
	Image->SetBrush(Brush);
	if (auto* Slot = Cast<UCanvasPanelSlot>(Image->Slot))
	{
		const FVector2D Original = Slot->GetSize();
		if (Original.X > 0.f && Original.Y > 0.f && Sprite->Aspect > 0.f)
		{
			const double Height = FMath::Min(Original.Y, Original.X / Sprite->Aspect);
			const FVector2D Fitted(Height * Sprite->Aspect, Height);
			Slot->SetPosition(Slot->GetPosition() + (Original - Fitted) * (FVector2D(.5) - Slot->GetAlignment()));
			Slot->SetSize(Fitted);
		}
	}
	return true;
}
