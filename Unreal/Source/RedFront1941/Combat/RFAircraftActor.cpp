// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFAircraftActor.h"

#include "Combat/RFDamageTypes.h"
#include "Core/RFGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ARFAircraftActor::ARFAircraftActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetCanBeDamaged(true);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("AircraftCollision"));
	CollisionBox->SetBoxExtent(FVector(105.0f, 95.0f, 18.0f));
	CollisionBox->SetCollisionProfileName(TEXT("RF_2DSprite"));
	RootComponent = CollisionBox;

	FuselageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuselage"));
	FuselageMesh->SetupAttachment(RootComponent);
	FuselageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FuselageMesh->SetRelativeScale3D(FVector(1.8f, 0.2f, 0.18f));

	WingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wings"));
	WingMesh->SetupAttachment(RootComponent);
	WingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingMesh->SetRelativeScale3D(FVector(0.42f, 1.8f, 0.12f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FuselageMesh->SetStaticMesh(CubeMesh.Object);
		WingMesh->SetStaticMesh(CubeMesh.Object);
	}

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetWorldSize(26.0f);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 42.0f));
	Nameplate->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Nameplate->SetTextRenderColor(FColor(255, 220, 160));
	Tags.Add(FName(TEXT("RF_Aircraft")));
}

void ARFAircraftActor::InitializeFromDefinition(const FRFAirUnitDef& InDefinition,
	ERFFaction PlayerFaction, float InitialAttackDelayS)
{
	AircraftId = InDefinition.Id;
	Health = FMath::Max(1.0f, InDefinition.Hp);
	BombRadiusM = FMath::Max(0.0f, InDefinition.BombRadiusM);
	bStrafes = InDefinition.bStrafes;
	bHasSiren = InDefinition.bHasSiren;
	bFriendlyToPlayer = InDefinition.Faction == PlayerFaction;
	AttackCooldownS = FMath::Max(0.0f, InitialAttackDelayS);
	Nameplate->SetText(InDefinition.NameZh);
	Tags.AddUnique(bFriendlyToPlayer ? FName(TEXT("RF_Friendly")) : FName(TEXT("RF_Enemy")));
}

void ARFAircraftActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Health <= 0.0f)
	{
		return;
	}

	AActor* Target = FindTarget();
	if (Target == nullptr)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const FVector2D Direction(ToTarget.X, ToTarget.Y);
	if (!bFlightStarted && !Direction.IsNearlyZero())
	{
		FlightDirection = Direction.GetSafeNormal();
		bFlightStarted = true;
		SetLifeSpan(24.0f);
	}
	if (!FlightDirection.IsNearlyZero())
	{
		SetActorRotation(FVector(FlightDirection.X, FlightDirection.Y, 0.0f).Rotation());
	}

	const float DistanceCm = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (DistanceCm > 18000.0f || bHasAttacked || AttackCooldownS <= 0.0f)
	{
		SetActorLocation(GetActorLocation() +
			FVector(FlightDirection.X, FlightDirection.Y, 0.0f) * 7200.0f * DeltaSeconds,
			false);
	}

	AttackCooldownS = FMath::Max(0.0f, AttackCooldownS - DeltaSeconds);
	if (bHasSiren && !bWarningSent && DistanceCm <= 18000.0f)
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			PlayerController->ClientMessage(TEXT("空袭警报：斯图卡俯冲来袭，立即寻找掩体！"));
		}
		bWarningSent = true;
	}
	if (AttackCooldownS <= 0.0f && DistanceCm <= 18000.0f)
	{
		MakeAttackPass(Target);
		bHasAttacked = true;
		AttackCooldownS = 2.8f;
	}
}

AActor* ARFAircraftActor::FindTarget() const
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

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (AActor* Candidate : Candidates)
	{
		if (Candidate == nullptr || Candidate == this || Candidate->IsActorBeingDestroyed())
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestTarget;
}

void ARFAircraftActor::MakeAttackPass(AActor* Target)
{
	if (Target == nullptr)
	{
		return;
	}

	if (BombRadiusM > 0.0f && !bBombReleased)
	{
		RFDamage::ApplyExplosionDamage(GetWorld(), Target->GetActorLocation(), BombRadiusM,
			90.0f, 25.0f, this, this);
		bBombReleased = true;
		UE_LOG(LogTemp, Log, TEXT("RFAircraftActor: %s released its bomb run near %s."),
			*AircraftId.ToString(), *GetNameSafe(Target));
		return;
	}

	if (bStrafes)
	{
		RFDamage::ApplyExplosionDamage(GetWorld(), Target->GetActorLocation(), 7.0f,
			22.0f, 0.0f, this, this, 2.0f);
		UE_LOG(LogTemp, VeryVerbose, TEXT("RFAircraftActor: %s strafed %s."),
			*AircraftId.ToString(), *GetNameSafe(Target));
	}
}

float ARFAircraftActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = FMath::Clamp(DamageAmount, 0.0f, Health);
	Health -= AppliedDamage;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		SetActorEnableCollision(false);
		FuselageMesh->SetVisibility(false, true);
		WingMesh->SetVisibility(false, true);
		Nameplate->SetText(FText::FromString(FString::Printf(TEXT("%s — 击落"),
			*AircraftId.ToString())));
		SetLifeSpan(6.0f);
		UE_LOG(LogTemp, Log, TEXT("RFAircraftActor: %s shot down by %s."),
			*AircraftId.ToString(), *GetNameSafe(DamageCauser));
		if (ActorHasTag(FName(TEXT("RF_Enemy"))) && GetWorld() != nullptr)
		{
			if (ARFGameMode* GameMode = GetWorld()->GetAuthGameMode<ARFGameMode>())
			{
				GameMode->NotifyEnemyEliminated(AircraftId);
			}
		}
	}
	return AppliedDamage;
}
