// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFVehicleActor.h"

#include "Combat/RFBallistics.h"
#include "Combat/RFDamageTypes.h"
#include "Core/RFGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARFVehicleActor::ARFVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	SetCanBeDamaged(true);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	CollisionBox->SetBoxExtent(FVector(110.0f, 52.0f, 34.0f));
	CollisionBox->SetCollisionProfileName(TEXT("RF_2DSprite"));
	RootComponent = CollisionBox;

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	HullMesh->SetupAttachment(RootComponent);
	HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullMesh->SetRelativeScale3D(FVector(2.0f, 0.9f, 0.45f));
	HullMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Turret"));
	TurretMesh->SetupAttachment(RootComponent);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TurretMesh->SetRelativeScale3D(FVector(0.72f, 0.58f, 0.28f));
	TurretMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		HullMesh->SetStaticMesh(CubeMesh.Object);
		TurretMesh->SetStaticMesh(CubeMesh.Object);
	}

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetVerticalAlignment(EVRTA_TextCenter);
	Nameplate->SetWorldSize(34.0f);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 66.0f));
	Nameplate->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Nameplate->SetTextRenderColor(FColor(255, 232, 190));

	ArmorComponent = CreateDefaultSubobject<URFVehicleArmorComponent>(TEXT("Armor"));
	Tags.Add(FName(TEXT("RF_Vehicle")));
}

void ARFVehicleActor::InitializeFromDefinition(const FRFVehicleDef& InDefinition,
	ERFFaction PlayerFaction)
{
	VehicleId = InDefinition.Id;
	Faction = InDefinition.Faction;
	MaxHealth = FMath::Max(1.0f, InDefinition.Hp);
	Health = MaxHealth;
	ArmorComponent->InitializeFromDefinition(InDefinition);
	Nameplate->SetText(InDefinition.NameZh);
	MoveSpeedCms = InDefinition.AI.bEmplaced
		? 0.0f : FMath::Max(0.0f, InDefinition.SpeedKph) * (100000.0f / 3600.0f);
	EngageRangeM = FMath::Max(25.0f, InDefinition.AI.EngageRangeM);
	GunPenetrationMm = FMath::Max(0.0f, InDefinition.PenetrationMm);
	GunDamage = FMath::Clamp(65.0f + GunPenetrationMm * 0.25f, 65.0f, 100.0f);
	GunAccuracy = FMath::Clamp(0.88f - InDefinition.AI.ReactionS * 0.04f, 0.55f, 0.85f);
	ReactionDurationS = FMath::Max(0.0f, InDefinition.AI.ReactionS);
	ReactionRemainingS = ReactionDurationS;
	bFriendlyToPlayer = Faction == PlayerFaction;
	bTargetsAircraft = InDefinition.AI.bTargetsAircraft;

	if (bFriendlyToPlayer)
	{
		Tags.AddUnique(FName(TEXT("RF_Friendly")));
	}
	else
	{
		Tags.AddUnique(FName(TEXT("RF_Hostile")));
		Tags.AddUnique(FName(TEXT("RF_Enemy")));
	}
	Tags.AddUnique(FName(*FString::Printf(TEXT("RF_VehicleId:%s"), *VehicleId.ToString())));
}

void ARFVehicleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickCombat(DeltaSeconds);
}

AActor* ARFVehicleActor::FindCombatTarget() const
{
	TArray<AActor*> Candidates;
	if (bFriendlyToPlayer)
	{
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("RF_Enemy")), Candidates);
	}
	else
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* PlayerPawn = PlayerController->GetPawn())
			{
				Candidates.Add(PlayerPawn);
			}
		}
		TArray<AActor*> FriendlyCandidates;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("RF_Friendly")), FriendlyCandidates);
		for (AActor* Candidate : FriendlyCandidates)
		{
			Candidates.AddUnique(Candidate);
		}
	}
	if (bTargetsAircraft)
	{
		TArray<AActor*> AircraftCandidates;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(),
			bFriendlyToPlayer ? FName(TEXT("RF_Enemy")) : FName(TEXT("RF_Friendly")),
			AircraftCandidates);
		for (AActor* Candidate : AircraftCandidates)
		{
			if (Candidate != nullptr && Candidate->ActorHasTag(FName(TEXT("RF_Aircraft"))))
			{
				Candidates.AddUnique(Candidate);
			}
		}
	}

	AActor* BestTarget = nullptr;
	AActor* BestAircraftTarget = nullptr;
	float BestDistanceSquared = FMath::Square(EngageRangeM * 100.0f);
	float BestAircraftDistanceSquared = BestDistanceSquared;
	for (AActor* Candidate : Candidates)
	{
		if (Candidate == nullptr || Candidate == this || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared > BestDistanceSquared)
		{
			continue;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RFVehicleSight), false, this);
		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			Hit, GetActorLocation(), Candidate->GetActorLocation(), ECC_Pawn, QueryParams);
		if (bBlocked && Hit.GetActor() != Candidate)
		{
			continue;
		}

		if (Candidate->ActorHasTag(FName(TEXT("RF_Aircraft"))))
		{
			BestAircraftTarget = Candidate;
			BestAircraftDistanceSquared = DistanceSquared;
		}
		else
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestAircraftTarget != nullptr ? BestAircraftTarget : BestTarget;
}

void ARFVehicleActor::TickCombat(float DeltaSeconds)
{
	if (Health <= 0.0f || EngageRangeM <= 0.0f)
	{
		return;
	}

	ShotCooldownS = FMath::Max(0.0f, ShotCooldownS - DeltaSeconds);
	AActor* Target = FindCombatTarget();
	if (Target == nullptr)
	{
		ReactionRemainingS = ReactionDurationS;
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float DistanceCm = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	const float EngageRangeCm = EngageRangeM * 100.0f;
	if (MoveSpeedCms > 0.0f && DistanceCm > EngageRangeCm * 0.85f)
	{
		const FVector MoveDirection(ToTarget.X, ToTarget.Y, 0.0f);
		const FVector Step = MoveDirection.GetSafeNormal() *
			FMath::Min(DistanceCm - EngageRangeCm * 0.85f, MoveSpeedCms * DeltaSeconds);
		FHitResult MoveHit;
		SetActorLocation(GetActorLocation() + Step, true, &MoveHit);
	}

	const FVector Facing(ToTarget.X, ToTarget.Y, 0.0f);
	if (!Facing.IsNearlyZero())
	{
		SetActorRotation(Facing.Rotation());
	}

	ReactionRemainingS = FMath::Max(0.0f, ReactionRemainingS - DeltaSeconds);
	if (DistanceCm <= EngageRangeCm && ReactionRemainingS <= 0.0f && ShotCooldownS <= 0.0f)
	{
		FireAt(Target);
		ShotCooldownS = GunPenetrationMm >= 75.0f ? 3.5f : 1.25f;
	}
}

void ARFVehicleActor::FireAt(AActor* Target)
{
	if (Target == nullptr)
	{
		return;
	}

	FRFWeaponDef Gun;
	Gun.Id = VehicleId;
	Gun.Faction = Faction;
	Gun.Category = ERFWeaponCategory::RifleBolt;
	Gun.Damage = GunDamage;
	Gun.PenetrationMm = GunPenetrationMm;
	Gun.Accuracy = GunAccuracy;
	Gun.EffectiveRangeM = EngageRangeM;
	const FVector Aim = Target->GetActorLocation() - GetActorLocation();
	const float SpreadDeg = 0.5f + (1.0f - GunAccuracy) * 7.5f;
	const FRFShotResult Result = RFBallistics::ResolveShot(
		GetActorLocation(), FVector2D(Aim.X, Aim.Y), SpreadDeg, Gun, this, nullptr);
	if (Result.Result != ERFPenetrationResult::NoTarget)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("RFVehicleActor: %s fired at %s, result=%d damage=%.1f."),
			*VehicleId.ToString(), *GetNameSafe(Target), static_cast<int32>(Result.Result),
			Result.AppliedDamage);
	}
}

float ARFVehicleActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = FMath::Clamp(DamageAmount, 0.0f, Health);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	Health -= AppliedDamage;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		SetActorEnableCollision(false);
		HullMesh->SetVisibility(false, true);
		Nameplate->SetText(FText::FromString(FString::Printf(TEXT("%s — 击毁"),
			*VehicleId.ToString())));
		SetLifeSpan(12.0f);
		UE_LOG(LogTemp, Log, TEXT("RFVehicleActor: %s destroyed by %s."),
			*VehicleId.ToString(), *GetNameSafe(DamageCauser));
		if (ActorHasTag(FName(TEXT("RF_Enemy"))) && GetWorld() != nullptr)
		{
			if (ARFGameMode* GameMode = GetWorld()->GetAuthGameMode<ARFGameMode>())
			{
				GameMode->NotifyEnemyEliminated(VehicleId);
			}
		}
	}
	return AppliedDamage;
}
