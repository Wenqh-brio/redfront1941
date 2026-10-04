// Copyright RedFront1941. All Rights Reserved.

#include "Core/RFCharacter.h"

#include "Combat/RFWeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "PaperFlipbookComponent.h"
#include "PaperFlipbook.h"
#include "Survival/RFInventoryComponent.h"
#include "Survival/RFSurvivalComponent.h"
#include "Survival/RFWeightComponent.h"

ARFCharacter::ARFCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 2D contract: no capsule-in-3D. The inherited capsule is shrunk to a thin slab
	// and kept only because APaperCharacter owns it; all gameplay traces use the
	// PlaneCollision box below.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(34.0f, 44.0f);
		Capsule->SetCollisionProfileName(TEXT("RF_2DSprite"));
	}

	PlaneCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PlaneCollision"));
	PlaneCollision->SetupAttachment(RootComponent);
	PlaneCollision->SetBoxExtent(FVector(20.0f, 34.0f, 44.0f));
	PlaneCollision->SetCollisionProfileName(TEXT("RF_2DSprite"));
	PlaneCollision->SetGenerateOverlapEvents(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 3200.0f));
	Camera->SetRelativeRotation(FRotationMatrix::MakeFromXZ(
		FVector(0.0f, 0.0f, -1.0f), FVector(0.0f, 1.0f, 0.0f)).Rotator());
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 3600.0f;

	// The flipbook is the visible soldier; it must not occlude the plane traces.
	if (UPaperFlipbookComponent* FlipbookComponent = GetSprite())
	{
		FlipbookComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FlipbookComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -44.0f));
	}

	WeightComponent = CreateDefaultSubobject<URFWeightComponent>(TEXT("WeightComponent"));
	InventoryComponent = CreateDefaultSubobject<URFInventoryComponent>(TEXT("InventoryComponent"));
	SurvivalComponent = CreateDefaultSubobject<URFSurvivalComponent>(TEXT("SurvivalComponent"));

	// Gameplay is planar: lock the actor upright and remove gravity-driven falls.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 0.0f;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		Move->MaxWalkSpeed = BaseWalkSpeedCms;
		Move->BrakingDecelerationWalking = 2048.0f;
		Move->SetPlaneConstraintEnabled(true);
		Move->SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
		Move->SetPlaneConstraintOrigin(FVector(0.0f, 0.0f, 44.0f));
	}
	bUseControllerRotationYaw = false;
}

void ARFCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Feet-anchored sprites: keep the flipbook on the plane and re-evaluate speed.
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, 44.0f), false, nullptr, ETeleportType::TeleportPhysics);
	AimWorldLocation = GetActorLocation() + FVector(1000.0f, 0.0f, 0.0f);
	ApplyMovementSpeed();
	RefreshFlipbookForState();
}

void ARFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Facing follows the aim vector, not velocity: it is a shooter, and the player
	// must be able to face a target while backing away.
	const FVector2D DesiredFacing = GetAimDirection2D();
	if (!DesiredFacing.IsNearlyZero())
	{
		Facing = FacingFromDirection2D(DesiredFacing);
	}

	// Sprint is an overlay on top of the base stance, so releasing the key returns
	// the soldier to whatever stance (stand/crouch/prone) was held before.
	const bool bCanSprint = bSprintRequested && !MoveInput2D.IsNearlyZero()
		&& (WeightComponent == nullptr || !WeightComponent->IsSprintLocked());
	Stance = bCanSprint ? ERFStance::Sprint : BaseStance;

	ApplyMovementSpeed();
	RefreshFlipbookForState();
}

void ARFCharacter::SetMoveInput2D(const FVector2D& WorldDirection2D)
{
	MoveInput2D = WorldDirection2D.GetSafeNormal();
	if (!MoveInput2D.IsNearlyZero())
	{
		// Planar movement only; Z is pinned by the movement component's plane constraint.
		AddMovementInput(FVector(MoveInput2D.X, MoveInput2D.Y, 0.0f), 1.0f);
	}
}

void ARFCharacter::SetSprinting(bool bInSprint)
{
	if (!bInSprint)
	{
		bSprintRequested = false;
		ApplyMovementSpeed();
		return;
	}

	// Overloaded soldiers cannot sprint (classes.json "超载：...无法冲刺").
	if (WeightComponent != nullptr && WeightComponent->IsSprintLocked())
	{
		bSprintRequested = false;
		return;
	}
	bSprintRequested = true;
}

void ARFCharacter::SetStance(ERFStance NewStance)
{
	// Sprint is an overlay handled in Tick; explicit stances are stand/crouch/prone.
	if (NewStance == ERFStance::Sprint)
	{
		SetSprinting(true);
		return;
	}
	bSprintRequested = false;
	BaseStance = NewStance;
	Stance = NewStance;
	ApplyMovementSpeed();
}

void ARFCharacter::SetAimWorldLocation(const FVector& WorldLocation)
{
	AimWorldLocation = FVector(WorldLocation.X, WorldLocation.Y, GetActorLocation().Z);
}

FVector2D ARFCharacter::GetAimDirection2D() const
{
	const FVector Delta = AimWorldLocation - GetActorLocation();
	const FVector2D Planar(Delta.X, Delta.Y);
	return Planar.GetSafeNormal();
}

ERFCharacterFacing ARFCharacter::FacingFromDirection2D(const FVector2D& Direction2D)
{
	if (Direction2D.IsNearlyZero())
	{
		return ERFCharacterFacing::East;
	}

	// Screen-space convention: +X is East (screen right), +Y is North (screen up).
	// The 8 buckets are 45 degrees wide and centred on the cardinal directions.
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Direction2D.Y, Direction2D.X));
	const int32 Bucket = FMath::RoundToInt(AngleDeg / 45.0f) & 7;

	switch (Bucket)
	{
	case 0:  return ERFCharacterFacing::East;
	case 1:  return ERFCharacterFacing::NorthEast;
	case 2:  return ERFCharacterFacing::North;
	case 3:  return ERFCharacterFacing::NorthWest;
	case 4:  return ERFCharacterFacing::West;
	case 5:  return ERFCharacterFacing::SouthWest;
	case 6:  return ERFCharacterFacing::South;
	default: return ERFCharacterFacing::SouthEast;
	}
}

FVector2D ARFCharacter::Direction2DFromFacing(ERFCharacterFacing Facing)
{
	// 45-degree steps starting at East, matching FacingFromDirection2D's buckets.
	const float AngleDeg = static_cast<float>(static_cast<uint8>(Facing)) * 45.0f;
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	return FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)).GetSafeNormal();
}

void ARFCharacter::RefreshFlipbookForState()
{
	UPaperFlipbookComponent* FlipbookComponent = GetSprite();
	if (FlipbookComponent == nullptr || FacingFlipbooks.Num() == 0)
	{
		return;
	}

	const int32 Index = FMath::Clamp(static_cast<int32>(Facing), 0, FacingFlipbooks.Num() - 1);
	UPaperFlipbook* Desired = FacingFlipbooks[Index];
	if (Desired != nullptr && FlipbookComponent->GetFlipbook() != Desired)
	{
		FlipbookComponent->SetFlipbook(Desired);
	}
}

void ARFCharacter::EquipWeapon(URFWeaponBase* NewWeapon)
{
	CurrentWeapon = NewWeapon;
}

void ARFCharacter::ApplyMovementSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move == nullptr)
	{
		return;
	}

	// speed = base * load band * stance * weapon mobility * armour penalty
	float Speed = BaseWalkSpeedCms;

	if (WeightComponent != nullptr)
	{
		Speed *= WeightComponent->GetSpeedMultiplier();
	}

	switch (Stance)
	{
	case ERFStance::Sprint:	Speed *= SprintMultiplier; break;
	case ERFStance::Crouch:	Speed *= CrouchSpeedMultiplier; break;
	case ERFStance::Prone:	Speed *= ProneSpeedMultiplier; break;
	default: break;
	}

	if (CurrentWeapon != nullptr)
	{
		// mobility_mod from weapons.json (0.35 for the Maxim, 1.05 for the M1 Carbine).
		Speed *= WeightComponent != nullptr ? WeightComponent->GetWeaponMobilityModifier() : 1.0f;
	}

	if (SurvivalComponent != nullptr)
	{
		// Exhaustion, pain and hypothermia all reduce the achievable speed.
		Speed *= SurvivalComponent->GetMobilityMultiplier();
	}

	Move->MaxWalkSpeed = FMath::Max(30.0f, Speed);
}
