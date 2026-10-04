// Copyright RedFront1941. All Rights Reserved.

#include "Core/RFGameState.h"

ARFGameState::ARFGameState()
{
	// Contract default: 2 CP at start, ceiling 10, 0.035 CP/s regeneration.
	CommandPoints = 2.0f;
	DifficultyScaling.Id = FName(TEXT("regular"));
	DifficultyScaling.Difficulty = ERFDifficulty::Regular;
}

void ARFGameState::SetCurrentLevel(const FRFLevelDef& InLevel)
{
	CurrentLevel = InLevel;
	MissionTimeS = 0.0f;
	bMissionComplete = false;
	InitializeObjectives();
}

void ARFGameState::SetDifficulty(ERFDifficulty NewDifficulty)
{
	Difficulty = NewDifficulty;

	// campaigns.json difficulty_scaling: design baseline for "regular"; the drain
	// multipliers are the only values stated in prose rather than numbers.
	switch (NewDifficulty)
	{
	case ERFDifficulty::Recruit:
		DifficultyScaling.Id = FName(TEXT("recruit"));
		DifficultyScaling.EnemyHpMult = 0.8f;
		DifficultyScaling.EnemyAccuracyMult = 0.8f;
		DifficultyScaling.PlayerDamageMult = 1.2f;
		DifficultyScaling.CpRegenMult = 1.4f;
		DifficultyScaling.SurvivalDrainMult = 1.0f;
		break;
	case ERFDifficulty::Veteran:
		DifficultyScaling.Id = FName(TEXT("veteran"));
		DifficultyScaling.EnemyHpMult = 1.2f;
		DifficultyScaling.EnemyAccuracyMult = 1.15f;
		DifficultyScaling.PlayerDamageMult = 0.9f;
		DifficultyScaling.CpRegenMult = 0.85f;
		DifficultyScaling.SurvivalDrainMult = 1.2f;
		break;
	case ERFDifficulty::Historical:
		// Historical: single life, no enemy markers, historical ammo basis, +50% drain.
		DifficultyScaling.Id = FName(TEXT("historical"));
		DifficultyScaling.EnemyHpMult = 1.35f;
		DifficultyScaling.EnemyAccuracyMult = 1.3f;
		DifficultyScaling.PlayerDamageMult = 0.8f;
		DifficultyScaling.CpRegenMult = 0.7f;
		DifficultyScaling.SurvivalDrainMult = 1.5f;
		break;
	default:
		DifficultyScaling.Id = FName(TEXT("regular"));
		DifficultyScaling.EnemyHpMult = 1.0f;
		DifficultyScaling.EnemyAccuracyMult = 1.0f;
		DifficultyScaling.PlayerDamageMult = 1.0f;
		DifficultyScaling.CpRegenMult = 1.0f;
		DifficultyScaling.SurvivalDrainMult = 1.0f;
		break;
	}
	DifficultyScaling.Difficulty = NewDifficulty;
}

void ARFGameState::SetMissionComplete(bool bComplete)
{
	bMissionComplete = bComplete;
}

void ARFGameState::InitializeObjectives()
{
	ObjectiveStatuses.Reset();
	for (const FRFObjectiveDef& Objective : CurrentLevel.Objectives)
	{
		FRFObjectiveStatus Status;
		Status.ObjectiveId = Objective.Id;
		// Contracts contain no prerequisite graph; primary and optional objectives are
		// therefore all available from deployment instead of leaving secondary markers locked.
		Status.State = ERFObjectiveState::Active;
		ObjectiveStatuses.Add(Status);
	}
}

void ARFGameState::SetObjectiveState(FName ObjectiveId, ERFObjectiveState NewState)
{
	for (FRFObjectiveStatus& Status : ObjectiveStatuses)
	{
		if (Status.ObjectiveId == ObjectiveId)
		{
			if (Status.State == NewState)
			{
				return;
			}
			Status.State = NewState;
			Status.ResolvedAtS = (NewState == ERFObjectiveState::Completed || NewState == ERFObjectiveState::Failed)
				? MissionTimeS
				: -1.0f;

			// Every completed objective is worth objective_bonus CP (default 2).
			if (NewState == ERFObjectiveState::Completed)
			{
				AddCommandPoints(static_cast<float>(CommandPointRules.ObjectiveBonus));
			}
			return;
		}
	}
}

ERFObjectiveState ARFGameState::GetObjectiveState(FName ObjectiveId) const
{
	for (const FRFObjectiveStatus& Status : ObjectiveStatuses)
	{
		if (Status.ObjectiveId == ObjectiveId)
		{
			return Status.State;
		}
	}
	return ERFObjectiveState::Inactive;
}

float ARFGameState::GetObjectiveProgress(FName ObjectiveId) const
{
	for (const FRFObjectiveStatus& Status : ObjectiveStatuses)
	{
		if (Status.ObjectiveId == ObjectiveId)
		{
			return Status.Progress;
		}
	}
	return 0.0f;
}

bool ARFGameState::IsObjectiveInteractable(FName ObjectiveId) const
{
	for (const FRFObjectiveDef& Objective : CurrentLevel.Objectives)
	{
		if (Objective.Id == ObjectiveId)
		{
			return Objective.Condition.IsNone()
				|| Objective.Condition == FName(TEXT("interact"));
		}
	}
	return false;
}

void ARFGameState::SetObjectiveProgress(FName ObjectiveId, float Progress)
{
	for (FRFObjectiveStatus& Status : ObjectiveStatuses)
	{
		if (Status.ObjectiveId == ObjectiveId)
		{
			Status.Progress = FMath::Max(0.0f, Progress);
			return;
		}
	}
}

void ARFGameState::AddCommandPoints(float Delta)
{
	const float Ceiling = static_cast<float>(CommandPointRules.Max);
	CommandPoints = FMath::Clamp(CommandPoints + Delta, 0.0f, Ceiling);
}

void ARFGameState::SetCommandPointRules(const FRFCommandPointRules& NewRules)
{
	CommandPointRules = NewRules;
	CommandPoints = FMath::Clamp(CommandPoints, 0.0f, static_cast<float>(NewRules.Max));
}

void ARFGameState::SetSquadOrder(ERFSquadOrder NewOrder)
{
	SquadOrder = NewOrder;
}

void ARFGameState::SetSquadStrength(int32 NewStrength)
{
	SquadStrength = FMath::Max(0, NewStrength);
}

int32 ARFGameState::AdvanceEndlessWave()
{
	++EndlessWave;
	// Endless rules: every 5th wave (5, 10, 15, ...) grants a free support mission
	// and is a boss wave; the bonus is applied as CP here so the support subsystem
	// does not need to know about wave structure.
	if (EndlessWave > 0 && EndlessWave % 5 == 0)
	{
		AddCommandPoints(2.0f);
	}
	return EndlessWave;
}

void ARFGameState::AdvanceMissionTime(float DeltaSeconds)
{
	if (!bMissionComplete)
	{
		MissionTimeS += FMath::Max(0.0f, DeltaSeconds);
	}
}
