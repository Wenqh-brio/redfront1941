// Copyright RedFront1941. All Rights Reserved.

#include "Support/RFSupportSubsystem.h"

#include "Core/RFGameState.h"
#include "Engine/World.h"
#include "Support/RFAirStrike.h"
#include "Support/RFArtilleryStrike.h"
#include "Support/RFTankCallIn.h"

void URFSupportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Contract defaults, so CP regeneration and cooldowns are live before any level has
	// installed its own rules (support.json command_points / comms).
	CommandPointRules.Start = 2;
	CommandPointRules.Max = 10;
	CommandPointRules.RegenPerS = 0.035f;
	CommandPointRules.ObjectiveBonus = 2;
	CommandPointRules.KillStreakBonus = 0.2f;
	CommsRules.bRadioRequired = true;
	CommsRules.RadioWeightKg = 2.4f;
	CommsRules.RadioSlots = 1;
	CommsRules.DestroyedLockoutS = 60.0f;
	CommsRules.LineBreakDelayMult = 2.0f;
	CommsRules.SignalFlareDelayS = 6.0f;
	CommsRules.SignalFlareSpreadMult = 0.5f;
	CommsRules.CounterIntelWindowS = 20.0f;
}

void URFSupportSubsystem::InitializeContracts(const TArray<FRFSupportCallInDef>& CallIns,
	const FRFCommandPointRules& InCommandPointRules, const FRFCommsRules& InCommsRules)
{
	CallInDefinitions.Reset();
	for (const FRFSupportCallInDef& CallIn : CallIns)
	{
		CallInDefinitions.Add(CallIn.Id, CallIn);
	}

	CommandPointRules = InCommandPointRules;
	CommandPointRules.Max = FMath::Max(1, CommandPointRules.Max);
	CommsRules = InCommsRules;

	if (UWorld* World = GetWorld())
	{
		if (ARFGameState* GameState = World->GetGameState<ARFGameState>())
		{
			GameState->SetCommandPointRules(CommandPointRules);
			GameState->SetCommsRules(CommsRules);
		}
	}
}

void URFSupportSubsystem::TickSupport(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	ARFGameState* GameState = World != nullptr ? World->GetGameState<ARFGameState>() : nullptr;

	// --- Cooldowns and lockouts.
	for (TPair<FName, float>& Pair : CooldownRemainingS)
	{
		Pair.Value = FMath::Max(0.0f, Pair.Value - DeltaSeconds);
	}
	RadioLockoutRemainingS = FMath::Max(0.0f, RadioLockoutRemainingS - DeltaSeconds);
	LineBreakRemainingS = FMath::Max(0.0f, LineBreakRemainingS - DeltaSeconds);

	// --- Command points: regen x difficulty multiplier, capped by the contract ceiling.
	if (GameState != nullptr)
	{
		const float RegenMult = GameState->GetDifficultyScaling().CpRegenMult;
		const float RegenPerS = CommandPointRules.RegenPerS * RegenMult;
		CpFractionAccumulator += RegenPerS * DeltaSeconds;

		if (CpFractionAccumulator >= 1.0f)
		{
			const float WholePoints = FMath::FloorToFloat(CpFractionAccumulator);
			CpFractionAccumulator -= WholePoints;
			GameState->AddCommandPoints(WholePoints);
		}

		// --- Advance the queue: a mission enters Firing once its delay has elapsed.
		const float MissionTimeS = GameState->GetMissionTimeS();
		for (FRFSupportRequest& Request : Queue)
		{
			if (Request.State != ERFSupportState::Queued)
			{
				continue;
			}

			// A broken line doubles the remaining delay of everything already called in.
			const float EffectiveDelay = IsLineBroken() ? Request.DelayS * CommsRules.LineBreakDelayMult : Request.DelayS;
			if (MissionTimeS - Request.RequestedAtS >= EffectiveDelay)
			{
				Request.State = ERFSupportState::Inbound;
				LaunchSupportMission(Request);
			}
		}

		// Drop finished missions (the effect actors own their own lifetime).
		Queue.RemoveAll([](const FRFSupportRequest& Request)
		{
			return Request.State == ERFSupportState::Complete || Request.State == ERFSupportState::Aborted;
		});
	}
}

float URFSupportSubsystem::ResolveSpreadM(const FRFSupportCallInDef& CallIn, bool bObserverLOS, bool bHasBinoculars) const
{
	float Spread = CallIn.SpreadBaseM;

	// No observer line of sight doubles the dispersion: fire without eyes is harassment,
	// not a fire mission (support.json design_rule).
	if (!bObserverLOS)
	{
		Spread *= 2.0f;
	}

	// Binoculars grant the contract's -35% artillery_accuracy_bonus while held.
	if (bHasBinoculars)
	{
		Spread *= 0.65f;
	}
	return FMath::Max(1.0f, Spread);
}

bool URFSupportSubsystem::RequestSupport(FName CallInId, const FVector& TargetPoint,
	bool bObserverLOS, bool bHasBinoculars)
{
	const FRFSupportCallInDef* Definition = CallInDefinitions.Find(CallInId);
	if (Definition == nullptr)
	{
		return false;
	}

	// Radio gate: destroyed set means a 60 s lockout; broken line doubles delays.
	if (CommsRules.bRadioRequired && !IsRadioOperational())
	{
		return false;
	}

	// Cooldown gate.
	if (GetCooldownRemainingS(CallInId) > 0.0f)
	{
		return false;
	}

	// Observer gate: missions that require line of sight are refused outright when the
	// observer has none, rather than fired blindly.
	if (Definition->RequiresObserverLOS() && !bObserverLOS)
	{
		return false;
	}

	UWorld* World = GetWorld();
	ARFGameState* GameState = World != nullptr ? World->GetGameState<ARFGameState>() : nullptr;
	if (GameState == nullptr)
	{
		return false;
	}

	// Command point gate.
	if (GameState->GetCommandPoints() < static_cast<float>(Definition->CpCost))
	{
		return false;
	}

	GameState->AddCommandPoints(-static_cast<float>(Definition->CpCost));
	CooldownRemainingS.Add(CallInId, Definition->CooldownS);

	FRFSupportRequest Request;
	Request.CallIn = *Definition;
	Request.TargetPoint = FVector(TargetPoint.X, TargetPoint.Y, 0.0f);
	Request.RequestedAtS = GameState->GetMissionTimeS();
	Request.bObserverConfirmed = bObserverLOS;
	// Night and smoke conditions require a signal flare, which costs 6 s but halves spread.
	Request.bFlareRequired = false;
	Request.DelayS = Definition->DelayS + (Request.bFlareRequired ? CommsRules.SignalFlareDelayS : 0.0f);
	Request.SpreadM = ResolveSpreadM(*Definition, bObserverLOS, bHasBinoculars);
	if (Request.bFlareRequired)
	{
		Request.SpreadM *= CommsRules.SignalFlareSpreadMult;
	}
	Request.State = ERFSupportState::Queued;

	LastDangerRadiusM = Definition->RadiusM;
	Queue.Add(Request);
	return true;
}

bool URFSupportSubsystem::ConfirmPendingRequest()
{
	// The wheel selection is resolved here: the first call-in of the pending type that
	// passes its gates is fired, matching the "CallSupport_* then Support_Confirm" flow.
	UWorld* World = GetWorld();
	ARFGameState* GameState = World != nullptr ? World->GetGameState<ARFGameState>() : nullptr;
	if (World == nullptr || GameState == nullptr)
	{
		return false;
	}

	// The target is the request target registered by the controller (the cursor point on
	// the gameplay plane); the aim comes from the pawn as a stable fallback.
	FVector TargetPoint = PendingTargetPoint;
	if (TargetPoint.IsNearlyZero())
	{
		if (const APawn* Pawn = World->GetFirstPlayerController() != nullptr
			? World->GetFirstPlayerController()->GetPawn() : nullptr)
		{
			TargetPoint = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 3000.0f;
		}
	}

	for (const TPair<FName, FRFSupportCallInDef>& Pair : CallInDefinitions)
	{
		if (Pair.Value.CallType != PendingCallInType)
		{
			continue;
		}
		if (RequestSupport(Pair.Key, TargetPoint, bPendingObserverLOS, bPendingHasBinoculars))
		{
			return true;
		}
	}
	return false;
}

void URFSupportSubsystem::SetPendingCallInType(ERFSupportCallType CallType)
{
	PendingCallInType = CallType;
}

void URFSupportSubsystem::SetPendingTarget(const FVector& TargetPoint)
{
	PendingTargetPoint = FVector(TargetPoint.X, TargetPoint.Y, 0.0f);
}

void URFSupportSubsystem::SetPendingObservation(bool bObserverLOS, bool bHasBinoculars)
{
	bPendingObserverLOS = bObserverLOS;
	bPendingHasBinoculars = bHasBinoculars;
}

float URFSupportSubsystem::GetCooldownRemainingS(FName CallInId) const
{
	const float* Found = CooldownRemainingS.Find(CallInId);
	return Found != nullptr ? *Found : 0.0f;
}

bool URFSupportSubsystem::IsRadioOperational() const
{
	return RadioLockoutRemainingS <= 0.0f;
}

void URFSupportSubsystem::NotifyRadioDestroyed()
{
	// support.json: "无线电台被摧毁则 60 秒内无法呼叫".
	RadioLockoutRemainingS = CommsRules.DestroyedLockoutS;

	// Already-queued missions are delayed: the line break penalty applies retroactively.
	NotifyLineBreak(CommsRules.DestroyedLockoutS);
}

void URFSupportSubsystem::NotifyRadioRestored()
{
	RadioLockoutRemainingS = 0.0f;
}

void URFSupportSubsystem::NotifyLineBreak(float DurationS)
{
	LineBreakRemainingS = FMath::Max(LineBreakRemainingS, DurationS);
}

void URFSupportSubsystem::LaunchSupportMission(FRFSupportRequest& Request)
{
	bool bLaunched = false;

	switch (Request.CallIn.CallType)
	{
	case ERFSupportCallType::Indirect:
	case ERFSupportCallType::AreaSaturation:
	case ERFSupportCallType::Obscurant:
		bLaunched = ARFArtilleryStrike::StartFireMission(this, Request.CallIn, Request.TargetPoint,
			Request.SpreadM, nullptr) != nullptr;
		break;

	case ERFSupportCallType::Armor:
	case ERFSupportCallType::ArmorSiege:
	{
		// The level supplies the rear-road route (RoutePoints); an empty route means the
		// level has no tank access from the rear, so the call-in aborts instead of
		// teleporting armour onto the objective.
		if (Request.RoutePoints.Num() < 2)
		{
			Request.State = ERFSupportState::Aborted;
			return;
		}
		bLaunched = ARFTankCallIn::RequestTankSupport(this, Request.CallIn, Request.TargetPoint,
			Request.RoutePoints, nullptr) != nullptr;
		break;
	}

	case ERFSupportCallType::Air:
		bLaunched = ARFAirStrike::RequestAirSupport(this, Request.CallIn, Request.TargetPoint,
			Request.bObserverConfirmed, Request.AAThreatCountInCorridor, nullptr) != nullptr;
		break;

	case ERFSupportCallType::Infantry:
	default:
		// Infantry attachments are spawned by the squad coordinator, not here: they are
		// troops on the ground, not a fire mission.
		bLaunched = true;
		break;
	}

	if (!bLaunched)
	{
		Request.State = ERFSupportState::Aborted;
		return;
	}

	Request.State = ERFSupportState::Firing;
}
