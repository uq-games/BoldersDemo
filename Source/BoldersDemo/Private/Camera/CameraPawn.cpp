#include "Camera/CameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

ACameraPawn::ACameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	SetReplicateMovement(false);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	SetRootComponent(CameraRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CameraRoot);
	SpringArm->TargetArmLength = 1200.0f;
	SpringArm->SetRelativeRotation(FRotator(-CameraGroundAngle, 0.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	FloatingMovement->MaxSpeed = 1200.0f;
	FloatingMovement->Acceleration = 8000.0f;
	FloatingMovement->Deceleration = 8000.0f;
}

void ACameraPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Pitch = -FMath::Clamp(CameraGroundAngle, 0.0f, 90.0f);
	SpringArm->SetRelativeRotation(Rotation);
}

void ACameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraPawn: EnhancedInputComponent is unavailable."));
		return;
	}

	if (CameraMoveAction)
	{
		EnhancedInput->BindAction(CameraMoveAction, ETriggerEvent::Triggered, this, &ACameraPawn::HandleCameraMove);
	}
	if (CameraRotateAction)
	{
		EnhancedInput->BindAction(CameraRotateAction, ETriggerEvent::Triggered, this, &ACameraPawn::HandleCameraRotate);
	}
	if (CameraZoomAction)
	{
		EnhancedInput->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &ACameraPawn::HandleCameraZoom);
	}
	if (!CameraMoveAction || !CameraRotateAction || !CameraZoomAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraPawn: One or more camera input actions are not assigned."));
	}
}

void ACameraPawn::HandleCameraMove(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;
	const FVector2D MoveInput = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0f, SpringArm->GetComponentRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MoveInput.Y);
	AddMovementInput(Right, MoveInput.X);
}

void ACameraPawn::HandleCameraRotate(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;
	const float YawDelta = Value.Get<float>() * RotationSpeed * GetWorld()->GetDeltaSeconds();
	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Yaw += YawDelta;
	SpringArm->SetRelativeRotation(Rotation);
}

void ACameraPawn::HandleCameraZoom(const FInputActionValue& Value)
{
	if (!IsLocallyControlled()) return;
	const float NewLength = SpringArm->TargetArmLength - Value.Get<float>() * ZoomStep;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinArmLength, FMath::Max(MinArmLength, MaxArmLength));
}
