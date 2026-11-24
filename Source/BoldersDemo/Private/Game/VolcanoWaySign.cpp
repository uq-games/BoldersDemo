#include "Game/VolcanoWaySign.h"

#include "Characters/BaseCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "VolcanoWaySign"

AVolcanoWaySign::AVolcanoWaySign()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	PrimaryActorTick.bCanEverTick = false;

	static const ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Materials/M_FoundryBasalt.M_FoundryBasalt"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Iron(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Materials/M_FoundryIron.M_FoundryIron"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Bronze(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Materials/M_FoundryFixture_Bronze.M_FoundryFixture_Bronze"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Entry(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns/MI_WaySign_Entry.MI_WaySign_Entry"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> CastingRoad(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns/MI_WaySign_CastingRoad.MI_WaySign_CastingRoad"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Core(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns/MI_WaySign_Core.MI_WaySign_Core"));
	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> Cooling(
		TEXT("/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns/MI_WaySign_Cooling.MI_WaySign_Cooling"));
	PlaqueMaterials = {Entry.Object, CastingRoad.Object, Core.Object, Cooling.Object};

	const auto AddPiece = [this](FName Name, const FVector& Position, const FVector& Scale,
		UMaterialInterface* Material, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		auto* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(GetRootComponent());
		Piece->SetStaticMesh(Cube.Object);
		Piece->SetRelativeLocationAndRotation(Position, Rotation);
		Piece->SetRelativeScale3D(Scale);
		Piece->SetMaterial(0, Material);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCanEverAffectNavigation(false);
	};
	AddPiece(TEXT("Footing"), {0, 0, 8}, {.66, .52, .16}, Stone.Object, FRotator(0, 45, 0));
	AddPiece(TEXT("Stem"), {0, 0, 56}, {.19, .19, .82}, Iron.Object);
	AddPiece(TEXT("Collar"), {0, 0, 73}, {.27, .27, .12}, Bronze.Object, FRotator(0, 45, 0));
	AddPiece(TEXT("LowerBrace"), {0, 0, 22}, {.36, .31, .15}, Bronze.Object);
	AddPiece(TEXT("PlaqueBacking"), {0, 0, 105}, {1.23, .53, .14}, Stone.Object);

	PlaqueFace = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("IllustratedPlaque"));
	PlaqueFace->SetupAttachment(GetRootComponent());
	PlaqueFace->SetRelativeLocation(FVector(0, 0, 113.f));
	PlaqueFace->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaqueFace->SetCanEverAffectNavigation(false);
	PlaqueFace->SetCastShadow(false);
}

void AVolcanoWaySign::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (PlaqueFace->GetNumSections() == 0)
	{
		// U follows local +X so the arrow points along the actor's authored heading.
		const TArray<FVector> Vertices{{-81, -54, 0}, {81, -54, 0}, {81, 54, 0}, {-81, 54, 0}};
		const TArray<int32> Indices{0, 1, 2, 0, 2, 3};
		const TArray<FVector2D> UVs{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
		PlaqueFace->CreateMeshSection_LinearColor(0, Vertices, Indices, {}, UVs, {}, {}, false, false);
	}
	UpdatePlaqueArt();
}

void AVolcanoWaySign::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdatePlaqueArt();
}

void AVolcanoWaySign::UpdatePlaqueArt()
{
	const int32 Index = static_cast<int32>(Kind);
	if (PlaqueMaterials.IsValidIndex(Index)) PlaqueFace->SetMaterial(0, PlaqueMaterials[Index]);
	PlaqueFace->SetRelativeScale3D(FVector(1, Kind == EVolcanoWaySignKind::CastingRoad ? 1.19 : 1, 1));
}

const AVolcanoWaySign* AVolcanoWaySign::FindReadable(const ABaseCharacter& Reader)
{
	const AVolcanoWaySign* Closest = nullptr;
	double Best = TNumericLimits<double>::Max();
	for (TActorIterator<AVolcanoWaySign> It(Reader.GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(Reader.GetActorLocation(), It->GetPlaqueLocation());
		if (Distance >= Best || !It->CanRead(Reader)) continue;
		Closest = *It;
		Best = Distance;
	}
	return Closest;
}

FVector AVolcanoWaySign::GetPlaqueLocation() const
{
	return PlaqueFace->GetComponentLocation();
}

bool AVolcanoWaySign::CanRead(const ABaseCharacter& Reader) const
{
	if (!Reader.IsAlive() || Reader.GetWorld() != GetWorld()) return false;
	const FVector Delta = GetPlaqueLocation() - Reader.GetActorLocation();
	if (Delta.SizeSquared2D() > FMath::Square(350.f) || FMath::Abs(Delta.Z) > 115.f) return false;
	return !GetWorld()->LineTraceTestByObjectType(Reader.GetActorLocation() + FVector(0, 0, 38.f), GetPlaqueLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(WaySignSight), false, &Reader));
}

FText AVolcanoWaySign::GetTitle() const
{
	switch (Kind)
	{
	case EVolcanoWaySignKind::CastingRoad: return LOCTEXT("Road", "旧铸道");
	case EVolcanoWaySignKind::Core: return LOCTEXT("Core", "核心石台");
	case EVolcanoWaySignKind::Cooling: return LOCTEXT("Cooling", "冷却槽");
	default: return LOCTEXT("Entry", "铸造岛入口");
	}
}

TArray<FText> AVolcanoWaySign::GetInstructions() const
{
	switch (Kind)
	{
	case EVolcanoWaySignKind::CastingRoad:
		return {LOCTEXT("RoadRoute", "沿旧铸道前往西侧下层。"),
			LOCTEXT("RoadGuard", "岩卫重砸后，趁收势反击。")};
	case EVolcanoWaySignKind::Core:
		return {LOCTEXT("CoreRoute", "清掉西侧守卫，沿坡道上台。"),
			LOCTEXT("CoreBoss", "石像会苏醒，先躲预警再反击。")};
	case EVolcanoWaySignKind::Cooling:
		return {LOCTEXT("CoolingReaction", "冰冻水面，导柱通电。"),
			LOCTEXT("CoolingDip", "战士靠近水槽按 F 附魔。")};
	default:
		return {LOCTEXT("EntryRoute", "沿外缘下行，找到入口守卫。"),
			LOCTEXT("EntryInput", "WASD 移动，左键攻击"),
			LOCTEXT("EntryMap", "Tab 切换攻距，点击地图展开")};
	}
}

#undef LOCTEXT_NAMESPACE
