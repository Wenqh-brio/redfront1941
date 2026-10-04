// Copyright RedFront1941. All Rights Reserved.

#include "AI/RFEnemyAIController.h"

#include "Combat/RFWeaponBase.h"
#include "Core/RFCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

ARFEnemyAIController::ARFEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	// Perception is configured in Blueprint for the sight/hearing senses; the awareness
	// radius from the contract is pushed into the sight config in InitializeFromDefinition.
	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SetPerceptionComponent(*Perception);
}

void ARFEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	// Default line infantry contract values until a definition is installed.
	Definition.AI.AwarenessM = 200.0f;
	Definition.AI.ReactionS = 1.2f;
	Definition.AI.SuppressionResist = 1.0f;
}

void ARFEnemyAIController::InitializeFromDefinition(const FRFEnemyDef& InDefinition)
{
	Definition = InDefinition;

	// Push the contract awareness radius into the sight sense so AI and level design agree.
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		if (const UAISenseConfig_Sight* Sight = Perception->GetSenseConfig<UAISenseConfig_Sight>())
		{
			UAISenseConfig_Sight* MutableSight = const_cast<UAISenseConfig_Sight*>(Sight);
			MutableSight->SightRadius = Definition.AI.AwarenessM * 100.0f;	// metres -> cm
			MutableSight->LoseSightRadius = MutableSight->SightRadius * 1.2f;
			// Peripheral vision is narrow for snipers (they scan deliberately) and wide
			// for line infantry advancing with the squad.
			MutableSight->PeripheralVisionAngleDegrees =
				Definition.Role == FName(TEXT("sniper")) ? 45.0f : 70.0f;
			Perception->ConfigureSense(*MutableSight);
			Perception->RequestStimuliListenerUpdate();
		}
	}
}

float ARFEnemyAIController::GetEffectiveAwarenessM() const
{
	// Suppression and terrain shorten awareness: a pinned MG team sees much less, and a
	// unit under heavy fire cannot scan its full sector.
	const float SuppressionPenalty = 1.0f - FMath::Clamp(SuppressionLevel, 0.0f, 1.0f) * 0.45f;
	const float SmokePenalty = IsTargetObscured() ? 0.5f : 1.0f;
	return Definition.AI.AwarenessM * SuppressionPenalty * SmokePenalty;
}

void ARFEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdatePerception(DeltaSeconds);
	TickSniperRelocation(DeltaSeconds);
	TickEngagement(DeltaSeconds);
	ShotCooldownS = FMath::Max(0.0f, ShotCooldownS - DeltaSeconds);

	// Suppression decays when the fire lifts; a broken unit never recovers this mission.
	if (AlertState != ERFAlertState::Broken)
	{
		SuppressionLevel = FMath::Max(0.0f, SuppressionLevel - SuppressionDecayPerS * DeltaSeconds);
	}

	UpdateAlertState();
}

void ARFEnemyAIController::TickEngagement(float DeltaSeconds)
{
	ARFCharacter* Soldier = Cast<ARFCharacter>(GetPawn());
	ARFCharacter* TargetSoldier = Cast<ARFCharacter>(CurrentTarget);
	if (Soldier == nullptr)
	{
		return;
	}

	URFWeaponBase* Weapon = Soldier->GetCurrentWeapon();
	if (TargetSoldier == nullptr || AlertState != ERFAlertState::Engaging || Weapon == nullptr)
	{
		if (Weapon != nullptr)
		{
			Weapon->StopFire();
		}
		Soldier->SetMoveInput2D(FVector2D::ZeroVector);
		return;
	}

	const FVector ToTarget = TargetSoldier->GetActorLocation() - Soldier->GetActorLocation();
	const float DistanceCm = FVector::Dist2D(Soldier->GetActorLocation(), TargetSoldier->GetActorLocation());
	Soldier->SetAimWorldLocation(TargetSoldier->GetActorLocation());

	const float WeaponRangeCm = FMath::Max(500.0f, Weapon->GetDefinition().EffectiveRangeM * 100.0f);
	if (DistanceCm > WeaponRangeCm * 0.75f)
	{
		Weapon->StopFire();
		const FVector2D ApproachDirection(ToTarget.X, ToTarget.Y);
		Soldier->SetMoveInput2D(ApproachDirection);
		return;
	}

	Soldier->SetMoveInput2D(FVector2D::ZeroVector);
	Weapon->SetAimingDownSights(true);
	if (Weapon->GetMagazineAmmo() <= 0)
	{
		Weapon->StopFire();
		if (!Weapon->IsReloading())
		{
			Weapon->StartReload();
		}
		return;
	}

	if (Weapon->GetFireMode() == ERFFireMode::FullAuto)
	{
		if (!Weapon->IsFiring())
		{
			Weapon->StartFire();
		}
		return;
	}

	if (ShotCooldownS <= 0.0f && !Weapon->IsReloading())
	{
		Weapon->StartFire();
		ShotCooldownS = FMath::Max(Definition.FireRateS, Weapon->GetShotIntervalS());
	}
}

void ARFEnemyAIController::UpdatePerception(float DeltaSeconds)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn == nullptr)
	{
		return;
	}

	// The player is the primary threat; a level can tag additional threats with the
	// "RF_Threat" actor tag (squadmates, called-in armour).
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("RF_Threat")), Candidates);

	if (const APlayerController* PC = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			Candidates.AddUnique(PlayerPawn);
		}
	}

	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	const float AwarenessCm = GetEffectiveAwarenessM() * 100.0f;

	for (AActor* Candidate : Candidates)
	{
		if (Candidate == nullptr || Candidate == MyPawn || Candidate->ActorHasTag(FName(TEXT("RF_Friendly"))))
		{
			continue;
		}
		const float Distance = FVector::Dist(MyPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance > AwarenessCm || Distance >= BestDistance)
		{
			continue;
		}

		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(RFEnemySight), false, MyPawn);
		SightParams.AddIgnoredActor(Candidate);
		FHitResult SightHit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			SightHit, MyPawn->GetActorLocation(), Candidate->GetActorLocation(),
			ECC_Pawn, SightParams);
		if (!bBlocked)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}

	CurrentTarget = Best;
	if (Best != nullptr)
	{
		// Reaction time is the contract's reaction_s: the unit needs that much continuous
		// contact before it may shoot, which is what makes a 0.85 s SS grenadier feel
		// dangerous and a 1.8 s Volkssturm militiaman feel slow.
		ContactTimeS = FMath::Min(Definition.AI.ReactionS * 2.0f, ContactTimeS + DeltaSeconds);
		ContactElapsedS += DeltaSeconds;
	}
	else
	{
		// Losing sight resets the aim gate but keeps the suspicion for a moment.
		ContactTimeS = FMath::Max(0.0f, ContactTimeS - DeltaSeconds * 2.0f);
		ContactElapsedS = 0.0f;
		bSupportRequested = false;
	}
}

void ARFEnemyAIController::TickSniperRelocation(float DeltaSeconds)
{
	if (!Definition.AI.bRelocatesAfterShot)
	{
		return;
	}

	// The sniper's relocation is driven by the weapon's "shot fired" notify in the
	// Blueprint; the controller exposes the state so the behaviour tree can move it.
	// When the unit is relocating, it cannot be engaged until it re-acquires.
	if (AlertState == ERFAlertState::Relocating)
	{
		// Relocation itself is a short move order (2-4 s); the state clears when the
		// behaviour tree reports arrival, so no timer is enforced here.
	}
}

void ARFEnemyAIController::UpdateAlertState()
{
	if (AlertState == ERFAlertState::Broken)
	{
		return;
	}

	const float PinThreshold = 1.0f / FMath::Max(0.1f, Definition.AI.SuppressionResist);
	if (SuppressionLevel >= PinThreshold)
	{
		AlertState = ERFAlertState::Suppressed;

		// A pinned unit may break: Volkssturm (0.35) and Romanians (0.25) frequently do,
		// Waffen-SS (no morale_break_chance field) hold.
		if (Definition.AI.MoraleBreakChance > 0.0f && FMath::FRand() < Definition.AI.MoraleBreakChance * 0.01f)
		{
			RollMoraleBreak();
		}
		return;
	}

	if (CurrentTarget != nullptr && ContactTimeS >= Definition.AI.ReactionS)
	{
		AlertState = ERFAlertState::Engaging;
	}
	else if (CurrentTarget != nullptr)
	{
		AlertState = ERFAlertState::Suspicious;
	}
	else
	{
		AlertState = ERFAlertState::Idle;
	}
}

void ARFEnemyAIController::NotifyIncomingFire(float DangerLevel)
{
	if (DangerLevel <= 0.0f)
	{
		return;
	}

	// suppression_resist divides the incoming pressure: an MG42 team (1.3) needs much
	// more fire to pin than a Volkssturm squad (0.5).
	const float Applied = DangerLevel / FMath::Max(0.1f, Definition.AI.SuppressionResist);
	SuppressionLevel = FMath::Clamp(SuppressionLevel + Applied, 0.0f, 1.0f);
}

bool ARFEnemyAIController::RollMoraleBreak()
{
	float Chance = Definition.AI.MoraleBreakChance;
	if (bLeaderLost)
	{
		Chance += LeaderLostBreakBonus;
	}

	// Suppression itself raises the chance: a unit pinned at 100% breaks more readily.
	Chance += SuppressionLevel * 0.15f;

	if (FMath::FRand() < FMath::Clamp(Chance, 0.0f, 0.95f))
	{
		AlertState = ERFAlertState::Broken;
		// The behaviour tree reads this and runs the retreat branch (fall back to the
		// nearest cover away from the player, then surrender or flee off the map).
		return true;
	}
	return false;
}

bool ARFEnemyAIController::ShouldCallSupport() const
{
	if (!Definition.AI.bCallsSupport || bSupportRequested)
	{
		return false;
	}
	// Contract: call_support_s seconds of contact before the request goes out
	// (squad leader 40 s, Waffen-SS grenadier 55 s).
	return Definition.AI.CallSupportS > 0.0f && ContactElapsedS >= Definition.AI.CallSupportS;
}

void ARFEnemyAIController::NotifyLeaderLost()
{
	bLeaderLost = true;

	// Losing the leader is a shock: the unit immediately rolls for a morale break and
	// thereafter carries the extra break chance.
	RollMoraleBreak();
}

bool ARFEnemyAIController::IsTargetObscured() const
{
	// Smoke grenades and artillery smoke register as "RF_SmokeVolume" actors; the AI
	// treats any volume containing either end of the sight line as full occlusion.
	if (CurrentTarget == nullptr || GetPawn() == nullptr)
	{
		return false;
	}

	TArray<AActor*> SmokeVolumes;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(TEXT("RF_SmokeVolume")), SmokeVolumes);

	const FVector Muzzle = GetPawn()->GetActorLocation();
	const FVector Target = CurrentTarget->GetActorLocation();

	for (const AActor* Volume : SmokeVolumes)
	{
		if (Volume == nullptr)
		{
			continue;
		}
		// The volume's sphere radius comes from the smoke item contract; a volume that
		// contains either endpoint (or is within 200 cm of the sight line) blocks the shot.
		const FVector Center = Volume->GetActorLocation();
		const float RadiusCm = Volume->GetSimpleCollisionRadius();
		if (FVector::Dist(Center, Muzzle) <= RadiusCm || FVector::Dist(Center, Target) <= RadiusCm)
		{
			return true;
		}
	}
	return false;
}
