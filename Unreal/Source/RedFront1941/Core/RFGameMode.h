// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/RFDataTypes.h"
#include "RFGameMode.generated.h"

class ARFGameState;
class ARFPlayerController;
class ARFCharacter;
class URFDataTableLoader;

/** High-level flow state of a RedFront mission. */
UENUM(BlueprintType)
enum class ERFMatchState : uint8
{
	Preparing		UMETA(DisplayName = "Preparing (loadout / briefing)"),
	Playing			UMETA(DisplayName = "Playing"),
	Paused			UMETA(DisplayName = "Paused"),
	Success			UMETA(DisplayName = "Mission Success"),
	Failure			UMETA(DisplayName = "Mission Failure")
};

/**
 * Mission director for RedFront1941.
 *
 * It owns the mission clock, the win/lose rules (required objectives + player death),
 * and the endless-mode wave gate. It deliberately contains no combat maths: those live
 * in Combat/ and Support/ so they stay unit-testable.
 */
UCLASS(Config = Game)
class REDFRONT1941_API ARFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARFGameMode();

	/** Applies the configured default difficulty and prepares the first mission. */
	virtual void BeginPlay() override;

	/** Drives the mission clock and win/lose evaluation. */
	virtual void Tick(float DeltaSeconds) override;

	/** Selects the faction-specific player character from the active campaign map. */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// ---------------------------- Mission flow ------------------------------

	/**
	 * Starts a level by contract id from DT_Levels_*.
	 * @param LevelId Contract id, e.g. "sov_01_brest_fortress".
	 * @return True when the level was found and installed into the game state.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Flow")
	bool StartLevelById(FName LevelId);

	/** Installs a level contract directly (used by the editor and by endless mode). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Flow")
	void StartLevel(const FRFLevelDef& Level);

	/** Current flow state. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Flow")
	ERFMatchState GetMatchState() const { return MatchState; }

	/** Ends the mission with the given result; freezes the clock. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Flow")
	void EndMission(bool bSuccess);

	/** Reports a neutralized enemy contract unit to active elimination objectives. */
	void NotifyEnemyEliminated(FName EnemyId);

	/** Counts one rescued survivor for an active rescue objective. */
	bool NotifySurvivorRescued(FName ObjectiveId);

	/** Counts one destroyed target for an active destroy_target objective. */
	bool NotifyObjectiveTargetDestroyed(FName ObjectiveId);

	/** True when every required objective is complete and none has failed. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Flow")
	bool AreRequiredObjectivesMet() const;

	/** True when at least one required objective has failed. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Flow")
	bool HasFailedRequiredObjective() const;

	// ------------------------------- Endless --------------------------------

	/** True when the current level runs the endless wave table. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Endless")
	bool IsEndlessMode() const { return bEndlessMode; }

	/** Enables endless mode; waves then advance through Endless_NextWave. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Endless")
	void SetEndlessMode(bool bEnabled) { bEndlessMode = bEnabled; }

	/** Starts the next endless wave, bounded by the 45 s prep window. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Endless")
	void StartNextEndlessWave();

	/** Seconds left in the endless wave-prep window (0 when a wave is running). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Endless")
	float GetWavePrepRemainingS() const { return WavePrepRemainingS; }

	// ------------------------------- Support --------------------------------

	/** Runtime JSON loader shared by the whole session (owned by the game mode). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data")
	URFDataTableLoader* GetDataTableLoader() const { return DataTableLoader; }

	/** Player controller of the local player, or null in a headless run. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Flow")
	ARFPlayerController* GetRFPlayerController() const;

protected:
	/** Procedurally generated faction pawn Blueprints; native ARFCharacter is the fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Characters")
	TSubclassOf<ARFCharacter> SovietPlayerCharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Characters")
	TSubclassOf<ARFCharacter> USPlayerCharacterClass;

	/** German placeholder pawn used for contract-spawned enemy soldiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Characters")
	TSubclassOf<ARFCharacter> GermanEnemyCharacterClass;

	/** Flow state of the current mission. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Flow")
	ERFMatchState MatchState = ERFMatchState::Preparing;

	/** Difficulty tier applied at BeginPlay (overridable in DefaultGame/Blueprints). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Config, Category = "RedFront|Rules")
	ERFDifficulty DefaultDifficulty = ERFDifficulty::Regular;

	/** Level started automatically at BeginPlay; empty means "wait for the front-end". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Config, Category = "RedFront|Rules")
	FName DefaultLevelId = NAME_None;

	/** True when the level runs the endless wave table instead of scripted objectives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Endless")
	bool bEndlessMode = false;

	/** Wave-prep window between endless waves in seconds (endless rules: 45 s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Endless")
	float WavePrepDurationS = 45.0f;

	/** Countdown to the next endless wave in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Endless")
	float WavePrepRemainingS = 0.0f;

	/** Runtime loader that builds the DataTables from Content/RedFront/Data/*.json. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "RedFront|Data")
	TObjectPtr<URFDataTableLoader> DataTableLoader;

private:
	/** Game state cast cached at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<ARFGameState> RFGameState;

	/** Evaluates required-objective completion after any objective change. */
	void EvaluateMissionProgress();

	/** Fails active objectives whose contract time limits have elapsed. */
	void EvaluateObjectiveDeadlines();

	/** Advances objectives with spatial/time conditions. */
	void EvaluateObjectiveConditions(float DeltaSeconds);

	/** Initializes the current blockout map once its player pawn and contracts exist. */
	void TryInitializeMapGameplay();

	/** Installs a contract-backed weapon component on a soldier. */
	void EquipContractWeapon(ARFCharacter* Soldier, const FRFWeaponDef& WeaponDefinition);

	/** Spawns the infantry described by the current map's RF_EnemyClass markers. */
	void SpawnMapEnemies(const FRFLevelDef& Level);

	/** True after map markers have been consumed for this world. */
	bool bMapGameplayInitialized = false;

	/** Runtime infantry cap per marker and map; excess contract counts are logged. */
	UPROPERTY(EditDefaultsOnly, Category = "RedFront|AI", meta = (ClampMin = "1"))
	int32 MaxEnemyUnitsPerMarker = 8;

	UPROPERTY(EditDefaultsOnly, Category = "RedFront|AI", meta = (ClampMin = "1"))
	int32 MaxEnemyUnitsPerMap = 32;
};
