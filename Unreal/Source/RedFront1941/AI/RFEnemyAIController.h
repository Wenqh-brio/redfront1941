// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Data/RFDataTypes.h"
#include "RFEnemyAIController.generated.h"

/** What the AI believes is happening; drives the behaviour tree's main branch. */
UENUM(BlueprintType)
enum class ERFAlertState : uint8
{
	Idle			UMETA(DisplayName = "Idle / unaware"),
	Suspicious		UMETA(DisplayName = "Suspicious (heard or glimpsed)"),
	Engaging		UMETA(DisplayName = "Engaging"),
	Suppressed		UMETA(DisplayName = "Suppressed (pinned)"),
	Relocating		UMETA(DisplayName = "Relocating after firing"),
	Broken			UMETA(DisplayName = "Morale broken (retreating)")
};

/**
 * Enemy soldier AI.
 *
 * Contract-driven parameters (enemies.json "ai"):
 *   * awareness_m            detection radius in metres (MG42 team 420, sniper 500).
 *   * reaction_s             delay between detection and the first aimed shot
 *                            (Waffen-SS 0.85 s, Volkssturm 1.8 s).
 *   * suppression_resist     how much incoming fire it takes to pin (MG42 1.3).
 *   * morale_break_chance    probability of breaking when pinned or leaderless
 *                            (Volkssturm 0.35, Romanians 0.25).
 *   * relocates_after_shot   snipers displace to a new firing position after each shot.
 *   * calls_support + call_support_s  the squad leader asks for mortars after N seconds
 *                            of contact (40 s for a squad leader, 55 s for Waffen-SS).
 *
 * Everything is in seconds, metres, or 0-1 fractions. The controller directly drives
 * planar movement and the soldier's contract-backed weapon component.
 */
UCLASS()
class REDFRONT1941_API ARFEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARFEnemyAIController();

	/** Applies the default perception radii. */
	virtual void BeginPlay() override;

	/** Runs perception, reaction, combat, suppression decay and morale. */
	virtual void Tick(float DeltaSeconds) override;

	/** Installs the enemy contract row that drives this AI. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|AI")
	void InitializeFromDefinition(const FRFEnemyDef& InDefinition);

	/** Contract row currently driving this AI. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	const FRFEnemyDef& GetDefinition() const { return Definition; }

	/** Current alert state. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	ERFAlertState GetAlertState() const { return AlertState; }

	/** Effective awareness radius in metres after suppression/weather penalties. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	float GetEffectiveAwarenessM() const;

	/** Seconds of visible contact accumulated; the reaction gate is reaction_s. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	float GetContactTimeS() const { return ContactTimeS; }

	/** Suppression level 0-1; 1 means fully pinned. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	float GetSuppressionLevel() const { return SuppressionLevel; }

	/**
	 * Registers incoming fire near this unit.
	 * @param DangerLevel 0-1 severity of the burst (a single rifle round is ~0.05,
	 *        sustained MG fire is ~0.6).
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|AI")
	void NotifyIncomingFire(float DangerLevel);

	/** Rolls the morale break chance; returns true when the unit breaks and retreats. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|AI")
	bool RollMoraleBreak();

	/** True when this unit should call its own support fire mission right now. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	bool ShouldCallSupport() const;

	/** Marks the support request as made so it only happens once per contact. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|AI")
	void MarkSupportRequested() { bSupportRequested = true; }

	/** Notifies the AI that its squad leader died (raises the break chance). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|AI")
	void NotifyLeaderLost();

	/** True when smoke or terrain currently blocks the unit's view of the target. */
	UFUNCTION(BlueprintPure, Category = "RedFront|AI")
	bool IsTargetObscured() const;

protected:
	/** Enemy contract row. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	FRFEnemyDef Definition;

	/** Current alert state. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	ERFAlertState AlertState = ERFAlertState::Idle;

	/** Seconds of continuous contact with a visible target. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	float ContactTimeS = 0.0f;

	/** Seconds of contact accumulated for the support call timer. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	float ContactElapsedS = 0.0f;

	/** Suppression 0-1; decays over time when no fire is incoming. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	float SuppressionLevel = 0.0f;

	/** Suppression decay per second once fire stops (design: 0.12). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|AI")
	float SuppressionDecayPerS = 0.12f;

	/** Extra break chance added when the squad leader is dead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|AI")
	float LeaderLostBreakBonus = 0.15f;

	/** True once this unit has requested its support mission for the current contact. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	bool bSupportRequested = false;

	/** True when the squad leader has been killed. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	bool bLeaderLost = false;

	/** Best known target actor. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|AI")
	TObjectPtr<AActor> CurrentTarget;

	/** Picks the highest-priority visible enemy and updates awareness. */
	void UpdatePerception(float DeltaSeconds);

	/** Recomputes AlertState from awareness, suppression and morale. */
	void UpdateAlertState();

	/** Handles the sniper "relocates_after_shot" rule. */
	void TickSniperRelocation(float DeltaSeconds);

	/** Turns the contract-driven alert state into planar movement and weapon fire. */
	void TickEngagement(float DeltaSeconds);

	/** Seconds until a non-automatic weapon may fire again. */
	float ShotCooldownS = 0.0f;
};
