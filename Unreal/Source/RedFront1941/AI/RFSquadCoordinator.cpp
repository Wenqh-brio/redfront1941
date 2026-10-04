// Copyright RedFront1941. All Rights Reserved.

#include "AI/RFSquadCoordinator.h"

#include "AI/RFEnemyAIController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Support/RFSupportSubsystem.h"

ARFSquadCoordinator::ARFSquadCoordinator()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
}

void ARFSquadCoordinator::BeginPlay()
{
	Super::BeginPlay();

	// Members register themselves on spawn; if none did, adopt every enemy controller
	// within 30 m of the coordinator so a manually placed actor still works.
	if (Members.Num() == 0)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARFEnemyAIController::StaticClass(), Found);
		for (AActor* Actor : Found)
		{
			if (ARFEnemyAIController* Controller = Cast<ARFEnemyAIController>(Actor))
			{
				const APawn* ControlledPawn = Controller->GetPawn();
				if (ControlledPawn != nullptr
					&& FVector::Dist(ControlledPawn->GetActorLocation(), GetActorLocation()) <= 3000.0f)
				{
					AddSquadMember(Controller);
				}
			}
		}
	}

	AssignBaseOfFire();
}

void ARFSquadCoordinator::SetSquadMembers(const TArray<ARFEnemyAIController*>& InMembers)
{
	Members.Reset();
	for (ARFEnemyAIController* Member : InMembers)
	{
		if (Member != nullptr)
		{
			Members.Add(Member);
		}
	}
	AssignBaseOfFire();
}

void ARFSquadCoordinator::AddSquadMember(ARFEnemyAIController* Member)
{
	if (Member != nullptr && !Members.Contains(Member))
	{
		Members.Add(Member);
		AssignBaseOfFire();
	}
}

void ARFSquadCoordinator::RemoveSquadMember(ARFEnemyAIController* Member)
{
	Members.Remove(Member);
	if (BaseOfFireController == Member)
	{
		BaseOfFireController = nullptr;
		AssignBaseOfFire();
	}
}

void ARFSquadCoordinator::AssignBaseOfFire()
{
	// The base of fire is the support weapon: the MG42 team (awareness 420 m, threat 4)
	// or, failing that, the highest-threat member still alive.
	BaseOfFireController = nullptr;
	int32 BestThreat = -1;

	for (ARFEnemyAIController* Member : Members)
	{
		if (Member == nullptr || Member->GetPawn() == nullptr)
		{
			continue;
		}

		const FRFEnemyDef& Definition = Member->GetDefinition();
		const bool bIsSupport = Definition.Role == FName(TEXT("support"));
		const int32 Threat = Definition.Threat + (bIsSupport ? 10 : 0);

		if (Threat > BestThreat)
		{
			BestThreat = Threat;
			BaseOfFireController = Member;
		}
	}
}

void ARFSquadCoordinator::NotifyContact(float DeltaSeconds)
{
	ContactElapsedS += FMath::Max(0.0f, DeltaSeconds);
}

void ARFSquadCoordinator::NotifyLeaderKilled()
{
	bLeaderKilled = true;

	// A leaderless group loses coordination: it falls back to a defensive posture and
	// each member's break chance rises (handled inside the individual controllers).
	Posture = ERFSquadPosture::Defensive;
}

void ARFSquadCoordinator::RequestSupportFire()
{
	if (bSupportRequested)
	{
		return;
	}
	bSupportRequested = true;

	// Enemy support is simplified: it reuses an indirect-fire mission from the contracts
	// via the support subsystem, which owns CP, cooldowns and the comms delay. Enemy CP is
	// checked by the same gate, so the AI cannot spam barrages.
	UWorld* World = GetWorld();
	URFSupportSubsystem* Support = World != nullptr ? World->GetSubsystem<URFSupportSubsystem>() : nullptr;
	if (Support == nullptr)
	{
		return;
	}

	// Pick the cheapest indirect mission available to the owning faction; the AI targets
	// the squad's centre of mass, which is close enough for an area mission.
	FName BestCallIn = NAME_None;
	int32 BestCost = MAX_int32;
	for (const TPair<FName, FRFSupportCallInDef>& Pair : Support->GetAllCallIns())
	{
		if (Pair.Value.CallType != ERFSupportCallType::Indirect || Pair.Value.CpCost >= BestCost)
		{
			continue;
		}
		BestCost = Pair.Value.CpCost;
		BestCallIn = Pair.Key;
	}

	if (BestCallIn.IsNone())
	{
		// No mortar mission was installed for this level: the squad simply has no artillery.
		return;
	}

	const bool bRequested = Support->RequestSupport(BestCallIn, GetActorLocation(), true, false);
	(void)bRequested;
}

void ARFSquadCoordinator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Drop dead members every tick; a squad that loses its base of fire reassigns it.
	for (int32 Index = Members.Num() - 1; Index >= 0; --Index)
	{
		ARFEnemyAIController* Member = Members[Index];
		if (Member == nullptr || Member->GetPawn() == nullptr)
		{
			RemoveSquadMember(Member);
		}
	}

	UpdatePosture();

	if (Posture == ERFSquadPosture::BoundingOverwatch)
	{
		TickBoundingOverwatch(DeltaSeconds);
	}

	EvaluateCounterAttack();

	// Squad-level support request: leaders with calls_support ask for fire after the
	// contract's call_support_s seconds of contact (checked on the members).
	if (!bSupportRequested && ContactElapsedS >= SupportRequestDelayS)
	{
		for (ARFEnemyAIController* Member : Members)
		{
			if (Member != nullptr && Member->ShouldCallSupport())
			{
				Member->MarkSupportRequested();
				RequestSupportFire();
				break;
			}
		}
	}
}

void ARFSquadCoordinator::TickBoundingOverwatch(float DeltaSeconds)
{
	BoundPhaseElapsedS += DeltaSeconds;
	if (BoundPhaseElapsedS < BoundPhaseDurationS)
	{
		return;
	}

	// Swap roles: element A (even indices) and element B (odd indices) alternate between
	// moving and providing fire. The base-of-fire weapon never moves (that is the point
	// of a base of fire), so it is excluded from the swap.
	BoundPhaseElapsedS = 0.0f;
	bFirstElementMoving = !bFirstElementMoving;

	// The Blueprint behaviour tree reads this flag pair through blackboard keys set by the
	// coordinator; no per-member call is needed here.
}

void ARFSquadCoordinator::EvaluateCounterAttack()
{
	if (bCounterAttacked)
	{
		return;
	}

	// Trigger: the squad has been in contact long enough, or the player's squad has been
	// ground down to half strength. A leaderless squad never counter-attacks.
	const bool bTimeTriggered = ContactElapsedS >= CounterAttackDelayS;
	const bool bStrengthTriggered = ObservedPlayerStrength > 0 && ObservedPlayerStrength <= FMath::CeilToInt(8.0f * CounterAttackStrengthRatio);

	if (!bLeaderKilled && (bTimeTriggered || bStrengthTriggered))
	{
		bCounterAttacked = true;
		Posture = ERFSquadPosture::CounterAttack;
	}
}

void ARFSquadCoordinator::UpdatePosture()
{
	if (bCounterAttacked)
	{
		return;
	}

	if (Members.Num() == 0)
	{
		Posture = ERFSquadPosture::Withdrawing;
		return;
	}

	if (ContactElapsedS <= 0.0f)
	{
		Posture = ERFSquadPosture::Defensive;
		return;
	}

	// With a functioning support weapon the squad fights with a base of fire; without one
	// it falls back to bounding overwatch, which is the historically correct German
	// infantry answer to being pinned.
	Posture = BaseOfFireController != nullptr ? ERFSquadPosture::BaseOfFire : ERFSquadPosture::BoundingOverwatch;
}
