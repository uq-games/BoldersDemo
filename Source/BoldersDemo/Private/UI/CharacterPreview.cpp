#include "UI/CharacterPreview.h"
#include "UI/ClassSelectionData.h"
#include "UI/PreviewAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/ConstructorHelpers.h"

ACharacterPreview::ACharacterPreview()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(RootComponent);
	CharacterMesh->SetRelativeScale3D(FVector(.7));
	CharacterMesh->SetRelativeRotation(FRotator(0,-90,0));
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal")); Pedestal->SetupAttachment(RootComponent);
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon")); Weapon->SetupAttachment(CharacterMesh);
	Offhand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Offhand")); Offhand->SetupAttachment(CharacterMesh);
	UpperString = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperString")); UpperString->SetupAttachment(Weapon);
	LowerString = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerString")); LowerString->SetupAttachment(Weapon);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	UpperString->SetStaticMesh(Cylinder.Object); LowerString->SetStaticMesh(Cylinder.Object);
	UpperString->SetCastShadow(false); LowerString->SetCastShadow(false);
	for (UPrimitiveComponent* Part : TArray<UPrimitiveComponent*>{CharacterMesh, Pedestal, Weapon, Offhand, UpperString, LowerString})
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
	}
	auto* Key = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight")); Key->SetupAttachment(RootComponent);
	Key->SetRelativeLocation(FVector(220,-180,300)); Key->SetIntensity(28000); Key->SetAttenuationRadius(800);
	Key->SetLightColor(FLinearColor(1,.82f,.63f)); Key->SetCastShadows(false);
	auto* Fill = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight")); Fill->SetupAttachment(RootComponent);
	Fill->SetRelativeLocation(FVector(-50,200,200)); Fill->SetIntensity(16000); Fill->SetAttenuationRadius(650);
	Fill->SetLightColor(FLinearColor(.55f,.72f,1)); Fill->SetCastShadows(false);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture")); Capture->SetupAttachment(RootComponent);
	const FVector View(310,-350,230);
	Capture->SetRelativeLocation(View); Capture->SetRelativeRotation((FVector(0,0,110)-View).Rotation());
	Capture->FOVAngle = 24.f;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false); Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = false;
	Capture->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	Capture->SetComponentTickInterval(1.f/30.f);
	Capture->AddTickPrerequisiteComponent(CharacterMesh);
	Capture->AddTickPrerequisiteActor(this);
}

void ACharacterPreview::Configure(UClassSelectionData* Data)
{
	if (!Data || !Data->PreviewMesh || !Data->AnimationClass) return;
	Definition = Data;
	const auto* Defaults = GetClass()->GetDefaultObject<ACharacterPreview>();
	FRotator MeshRotation = Defaults->CharacterMesh->GetRelativeRotation();
	MeshRotation.Yaw += Data->PreviewYawOffset;
	CharacterMesh->SetRelativeRotation(MeshRotation);
	Capture->FOVAngle = FMath::Clamp(Defaults->Capture->FOVAngle + Data->PreviewFovOffset, 10.f, 80.f);
	SetActorTickEnabled(Data->bAnimateBowString);
	UpperString->SetVisibility(Data->bAnimateBowString); LowerString->SetVisibility(Data->bAnimateBowString);
	CharacterMesh->SetSkeletalMeshAsset(Data->PreviewMesh);
	CharacterMesh->SetRelativeLocation(FVector(0,0,Data->FeetHeight));
	CharacterMesh->SetAnimInstanceClass(Data->AnimationClass);
	Pedestal->SetStaticMesh(Data->PedestalMesh); Pedestal->SetRelativeScale3D(Data->PedestalScale);
	Weapon->SetStaticMesh(Data->WeaponMesh);
	Weapon->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, Data->WeaponSocket);
	Offhand->SetStaticMesh(Data->OffhandMesh);
	Offhand->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, Data->OffhandSocket);
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor(0,0,0,1);
	RenderTarget->InitCustomFormat(448,512,PF_FloatRGBA,false);
	Capture->TextureTarget = RenderTarget;
	Capture->ShowOnlyActorComponents(this);
}

void ACharacterPreview::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Definition || !Definition->bAnimateBowString) return;
	const auto* Animation=CharacterMesh->GetAnimInstance();
	const FTransform Bow=Weapon->GetComponentTransform();
	const FVector Top=Bow.TransformPosition(FVector(-35,98.5,0));
	const FVector Bottom=Bow.TransformPosition(FVector(-35,-98.5,0));
	const float Draw=Animation ? FMath::Clamp(Animation->GetCurveValue(TEXT("BowDraw")),0.f,1.f) : 0.f;
	const FVector Nock=FMath::Lerp((Top+Bottom)*.5,CharacterMesh->GetSocketLocation(TEXT("handslot_r")),Draw);
	auto Place=[&](UStaticMeshComponent* Part,const FVector& A,const FVector& B)
	{
		Part->SetWorldLocation((A+B)*.5);
		Part->SetWorldRotation(FRotationMatrix::MakeFromZ(B-A).Rotator());
		const double Thickness=.006*Bow.GetScale3D().GetAbsMax();
		Part->SetWorldScale3D(FVector(Thickness,Thickness,(B-A).Length()/100.));
	};
	Place(UpperString,Top,Nock); Place(LowerString,Nock,Bottom);
}

void ACharacterPreview::EndPlay(const EEndPlayReason::Type Reason)
{
	Capture->bCaptureEveryFrame = false;
	Capture->TextureTarget = nullptr;
	Super::EndPlay(Reason);
}
