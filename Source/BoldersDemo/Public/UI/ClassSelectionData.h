#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClassSelectionData.generated.h"

class ABaseCharacter;
class ACharacterPreview;
class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;
class UPreviewAnimInstance;
class UMaterialInterface;
class UTexture2D;

UCLASS(BlueprintType)
class BOLDERSDEMO_API UClassSelectionData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ClassId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText RoleText;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSubclassOf<ABaseCharacter> CharacterClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USkeletalMesh> PreviewMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSubclassOf<UPreviewAnimInstance> AnimationClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimSequence> HoverAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimSequence> SelectedAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UStaticMesh> WeaponMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName WeaponSocket;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bAnimateBowString = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UStaticMesh> OffhandMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName OffhandSocket;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UStaticMesh> PedestalMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector PedestalScale = FVector(.33f,.27f,.35f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float FeetHeight = 27.f;
};

UCLASS(BlueprintType)
class BOLDERSDEMO_API UClassSelectionCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	// Shared by the local UI and server GameMode; the client sends only ClassId.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UClassSelectionData>> Classes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSubclassOf<ACharacterPreview> PreviewActorClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UMaterialInterface> PortraitMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UTexture2D> Background;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UTexture2D> CardFrame;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UTexture2D> ConfirmPlate;
};
