// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/RFDataTypes.h"
#include "RFGameState.generated.h"

/** Runtime state of a single mission objective. */
UENUM(BlueprintType)
enum class ERFObjectiveState : uint8
{
	Inactive	UMETA(DisplayName = "Inactive"),
	Active		UMETA(DisplayName = "Active"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed")
};

/** Live status of one objective while the mission runs. */
USTRUCT(BlueprintType)
struct FRFObjectiveStatus
{
	GENERATED_BODY()

	/** Objective id from the level contract ("obj1"). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objectives")
	FName ObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objectives")
	ERFObjectiveState State = ERFObjectiveState::Inactive;

	/** Elapsed mission seconds when the objective resolved; negative while unresolved. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objectives")
	float ResolvedAtS = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objectives")
	float Progress = 0.0f;
};

/** Standing squad order issued by the player (the squad AI fills in the tactics). */
UENUM(BlueprintType)
enum class ERFSquadOrder : uint8
{
	Hold		UMETA(DisplayName = "Hold Position"),
	Advance		UMETA(DisplayName = "Advance")
};

/**
 * Replicated mission state for one RedFront level: current level contract, objective
 * progress, command-point pool, and the endless-mode wave counter.
 *
 * Design intent: this is the only place other systems read "what is happening now",
 * so HUD, AI coordinator and support subsystem never talk to each other directly.
 */
UCLASS()
class REDFRONT1941_API ARFGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARFGameState();

	// ------------------------------- Mission --------------------------------

	/** Installs the level contract that is being played (from DT_Levels_*). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Mission")
	void SetCurrentLevel(const FRFLevelDef& InLevel);

	/** Level contract currently being played. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Mission")
	const FRFLevelDef& GetCurrentLevel() const { return CurrentLevel; }

	/** Difficulty tier selected for this run. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Mission")
	ERFDifficulty GetDifficulty() const { return Difficulty; }

	/** Sets the difficulty tier; drives enemy HP/accuracy and survival drain multipliers. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Mission")
	void SetDifficulty(ERFDifficulty NewDifficulty);

	/** Scaling row resolved for the current difficulty (campaigns.json difficulty_scaling). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Mission")
	const FRFDifficultyScaling& GetDifficultyScaling() const { return DifficultyScaling; }

	/** Mission elapsed time in seconds; drives par time and objective timers. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Mission")
	float GetMissionTimeS() const { return MissionTimeS; }

	/** True once the level has been won (all required objectives completed). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Mission")
	bool IsMissionComplete() const { return bMissionComplete; }

	/** Marks the mission finished; the game mode performs the actual end-of-level flow. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Mission")
	void SetMissionComplete(bool bComplete);

	// ------------------------------ Objectives ------------------------------

	/** Rebuilds the status list from the current level contract, all Inactive. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Objectives")
	void InitializeObjectives();

	/** Moves an objective into a new state and stamps the resolve time when it ends. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Objectives")
	void SetObjectiveState(FName ObjectiveId, ERFObjectiveState NewState);

	/** Current status of an objective; returns Inactive when the id is unknown. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Objectives")
	ERFObjectiveState GetObjectiveState(FName ObjectiveId) const;

	UFUNCTION(BlueprintPure, Category = "RedFront|Objectives")
	float GetObjectiveProgress(FName ObjectiveId) const;

	UFUNCTION(BlueprintPure, Category = "RedFront|Objectives")
	bool IsObjectiveInteractable(FName ObjectiveId) const;

	void SetObjectiveProgress(FName ObjectiveId, float Progress);

	/** All objective statuses, in contract order. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Objectives")
	const TArray<FRFObjectiveStatus>& GetObjectiveStatuses() const { return ObjectiveStatuses; }

	// --------------------------- Command points -----------------------------

	/** Current command points (fractional between spends). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetCommandPoints() const { return CommandPoints; }

	/** Adds CP, clamped to the contract ceiling; negative amounts are allowed spends. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void AddCommandPoints(float Delta);

	/** Comms rules resolved from support.json "comms". */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	const FRFCommsRules& GetCommsRules() const { return CommsRules; }

	/** Command point economy resolved from support.json "command_points". */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	const FRFCommandPointRules& GetCommandPointRules() const { return CommandPointRules; }

	/** Installs the CP economy (called once per level by the game mode). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void SetCommandPointRules(const FRFCommandPointRules& NewRules);

	/** Overrides the comms rules (called by the support subsystem when the radio dies). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void SetCommsRules(const FRFCommsRules& NewRules) { CommsRules = NewRules; }

	// ------------------------------ Squad order -----------------------------

	/** Last order issued to the player's squad. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	ERFSquadOrder GetSquadOrder() const { return SquadOrder; }

	/** Issues a squad order (Advance/Hold); the squad coordinator reads this. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void SetSquadOrder(ERFSquadOrder NewOrder);

	/** Squad strength in men, used by the HUD and by morale calculations. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	int32 GetSquadStrength() const { return SquadStrength; }

	/** Updates squad strength after a casualty or a reinforcement. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void SetSquadStrength(int32 NewStrength);

	// ------------------------------- Endless --------------------------------

	/** Current endless wave number (1-based; 0 outside endless mode). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Endless")
	int32 GetEndlessWave() const { return EndlessWave; }

	/** Advances to the next endless wave and returns the new number. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Endless")
	int32 AdvanceEndlessWave();

	/** Ticks mission time. Called by the game mode so pause handling stays authoritative. */
	void AdvanceMissionTime(float DeltaSeconds);

protected:
	/** Level contract in play. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Mission")
	FRFLevelDef CurrentLevel;

	/** Objective progress for the current level. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objectives")
	TArray<FRFObjectiveStatus> ObjectiveStatuses;

	/** Mission clock in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Mission")
	float MissionTimeS = 0.0f;

	/** True when the mission has ended successfully. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Mission")
	bool bMissionComplete = false;

	/** Selected difficulty tier. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Mission")
	ERFDifficulty Difficulty = ERFDifficulty::Regular;

	/** Resolved difficulty scaling row. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Mission")
	FRFDifficultyScaling DifficultyScaling;

	/** Command point pool. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float CommandPoints = 2.0f;

	/** CP ceiling and regeneration from support.json command_points. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FRFCommandPointRules CommandPointRules;

	/** Radio/comms constraints. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FRFCommsRules CommsRules;

	/** Current squad order. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	ERFSquadOrder SquadOrder = ERFSquadOrder::Hold;

	/** Squad strength in men. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	int32 SquadStrength = 8;

	/** Endless wave counter. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Endless")
	int32 EndlessWave = 0;
};
