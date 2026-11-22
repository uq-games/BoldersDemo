#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Data/AttackLoadout.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class UCharacterCombatComponent;
class UAttackPositioningComponent;
class UFactionRingComponent;
class UAbilitySystemComponent;
class UBoldersAttributeSet;
class UBaseCharacterAnimInstance;
class UAnimMontage;
struct FOnAttributeChangeData;
struct FGameplayTag;

UENUM(BlueprintType)
enum class ECharacterFaction : uint8 { Neutral, Player, Enemy };

UCLASS()
class BOLDERSDEMO_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ABaseCharacter();
	virtual void OnConstruction(const FTransform& Transform) override;
	// Select the generated class (ABP_*_C), not the editor-only UAnimBlueprint asset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Animation")
	TSubclassOf<UBaseCharacterAnimInstance> AnimationBlueprintClass;
	UFUNCTION(BlueprintPure, Category="Character|Animation")
	UBaseCharacterAnimInstance* GetCharacterAnimation() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UFUNCTION(BlueprintPure, Category="Combat") bool IsAlive() const;
	UFUNCTION(BlueprintPure, Category="Combat") bool IsFrozen() const;
	UFUNCTION(BlueprintPure, Category="Combat") EElementalAura GetElementalAura() const;
	UFUNCTION(BlueprintPure, Category="Combat") ECharacterFaction GetFaction() const { return Faction; }
	UFUNCTION(BlueprintPure, Category="Combat") float GetHealth() const;
	UFUNCTION(BlueprintPure, Category="Combat") float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category="Combat") bool IsHostileTo(const ABaseCharacter* Other) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat")
	bool ReceiveDamageFrom(ABaseCharacter* Source, float Damage);
	bool ReceiveDamageAt(ABaseCharacter* Source, float Damage, const FVector& Contact);
	bool ReceiveElementDamage(ABaseCharacter* Source, float Damage, EDamageElement Element);
	void ApplyFreezeFrom(ABaseCharacter* Source);
	void ApplyElementalAura(ABaseCharacter* Source, EElementalAura Aura);
	void ClearElementalAura(EElementalAura Aura);
	void ApplyCombatImpulse(const FVector& VelocityChange);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Character")
	int32 PlayerSlot = 0;
	// Local presentation only; does not change selection or gameplay authority.
	UPROPERTY(EditDefaultsOnly, Category="Combat|Presentation") TObjectPtr<UMaterialInterface> SwingMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Presentation") TObjectPtr<UMaterialInterface> ImpactMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Presentation") TObjectPtr<UStaticMesh> FrozenVisualMesh;
	UFUNCTION(NetMulticast, Unreliable) void MulticastAttackVisual(FVector_NetQuantize Location, FRotator Facing, bool bImpact);
	UFUNCTION(NetMulticast, Unreliable) void MulticastModuleVisual(FName ModuleId, EAttackVisualStage Stage,
		FVector_NetQuantize Location, FVector_NetQuantize End);
	UFUNCTION(NetMulticast, Unreliable) void MulticastElementalReaction(EElementalReaction Reaction,
		FVector_NetQuantize Location, FVector_NetQuantize End);
	UFUNCTION(NetMulticast, Unreliable) void MulticastModuleWindup(FName ModuleId, float Duration, FVector_NetQuantize Aim);
	void SetHoverOutline(bool bEnabled, bool bSelected = false);
	// Accepted does not mean arrived. Only the server may execute this request.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character|Movement")
	bool RequestMoveToLocation(const FVector& Destination);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character|Equipment")
	TObjectPtr<UCharacterCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character|AI")
	TObjectPtr<UAttackPositioningComponent> AttackPositioning;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character|Presentation")
	TObjectPtr<UFactionRingComponent> FactionRing;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleFrozenChanged(const FGameplayTag Tag, int32 Count);
	void ApplyAnimationBlueprint();
	UFUNCTION(NetMulticast, Reliable) void MulticastPlayHitReaction();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character|Animation")
	TObjectPtr<UAnimMontage> HitReactionMontage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
	TObjectPtr<UBoldersAttributeSet> AttributeSet;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category="Combat")
	ECharacterFaction Faction = ECharacterFaction::Neutral;
	UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1"))
	float InitialMaxHealth = 100.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Resistance", meta=(ClampMin="-.75", ClampMax=".9"))
	float PhysicalResistance = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Resistance", meta=(ClampMin="-.75", ClampMax=".9"))
	float FireResistance = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Resistance", meta=(ClampMin="-.75", ClampMax=".9"))
	float IceResistance = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Resistance", meta=(ClampMin="-.75", ClampMax=".9"))
	float WaterResistance = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="Combat|Resistance", meta=(ClampMin="-.75", ClampMax=".9"))
	float LightningResistance = 0.f;
	UPROPERTY(Transient) TWeakObjectPtr<AActor> LocalWindupVisual;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LocalFrozenVisual;

private:
	void StopCombatMovement();
	void UpdateFrozenVisual(bool bFrozen);
};
