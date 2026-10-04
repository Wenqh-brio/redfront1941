// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFSquadCoordinator.generated.h"

class ARFEnemyAIController;

/** Squad-level tactical posture. */
UENUM(BlueprintType)
enum class ERFSquadPosture : uint8
{
	Defensive		UMETA(DisplayName = "Defensive (in cover, watching approaches)"),
	BoundingOverwatch UMETA(DisplayName = "Bounding overwatch"),
	BaseOfFire		UMETA(DisplayName = "Base of fire (MG pinned on the objective)"),
	CounterAttack	UMETA(DisplayName = "Counter-attack"),
	Withdrawing		UMETA(DisplayName = "Withdrawing")
};

/**
 * Squad-level enemy AI coordinator.
 *
 * One coordinator exists per enemy squad (typically one per German Gruppe: 9 men, one
 * MG42 team, one leader). It owns the decisions that individual soldiers cannot make:
 *
 *   * Bounding overwatch: alternating fire-and-movement elements. Element A fires while
 *     element B moves, then they swap. This is why the player cannot simply out-shoot
 *     one defender at a time.
 *   * Base of fire: the MG42 team is assigned a firing position on the objective and is
 *     never used as a manoeuvre element (its awareness of 420 m and threat 4 make it the
 *     fight's centre of gravity).
 *   * Counter-attack trigger: when the player's squad strength drops below
 *     CounterAttackStrengthRatio, or when the squad has been out of contact for
 *     CounterAttackDelayS, the coordinator commits the reserve.
 *   * Support request: leaders with calls_support ask for mortar fire after
 *     call_support_s seconds of contact, once per engagement.
 *
 * Units: seconds and metres; strengths are head counts.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFSquadCoordinator : public AActor
{
	GENERATED_BODY()

public:
	ARFSquadCoordinator();

	/** Collects member controllers from the world. */
	virtual void BeginPlay() override;

	/** Runs the tactic state machine. */
	virtual void Tick(float DeltaSeconds) override;

	/** Replaces the coordinator's member list (called on squad spawn). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void SetSquadMembers(const TArray<ARFEnemyAIController*>& InMembers);

	/** Registers a member controller. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void AddSquadMember(ARFEnemyAIController* Member);

	/** Removes a dead member and reassigns the base-of-fire role if it held it. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void RemoveSquadMember(ARFEnemyAIController* Member);

	/** Current posture. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	ERFSquadPosture GetPosture() const { return Posture; }

	/** The controller currently assigned as the base of fire (usually the MG team). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	ARFEnemyAIController* GetBaseOfFireController() const { return BaseOfFireController; }

	/** True once the squad has committed its counter-attack. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	bool HasCounterAttacked() const { return bCounterAttacked; }

	/** Seconds of continuous contact observed by the squad. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Squad")
	float GetContactElapsedS() const { return ContactElapsedS; }

	/** Notifies the coordinator that contact has begun or continues (called by members). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void NotifyContact(float DeltaSeconds);

	/** Notifies the coordinator that a member's leader was killed. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void NotifyLeaderKilled();

	/** Player squad strength, set by the game state, used for the counter-attack trigger. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void SetEnemySquadStrength(int32 PlayerSquadStrength) { ObservedPlayerStrength = PlayerSquadStrength; }

	/** Issues an enemy support fire mission through the support subsystem. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Squad")
	void RequestSupportFire();

protected:
	/** Living member controllers. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	TArray<TObjectPtr<ARFEnemyAIController>> Members;

	/** Member assigned to the base-of-fire role. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	TObjectPtr<ARFEnemyAIController> BaseOfFireController;

	/** Current posture. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	ERFSquadPosture Posture = ERFSquadPosture::Defensive;

	/** Seconds of contact in the current engagement. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	float ContactElapsedS = 0.0f;

	/** True once the counter-attack has been committed. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	bool bCounterAttacked = false;

	/** True once the leader has died. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	bool bLeaderKilled = false;

	/** Last known strength of the player's squad (head count). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Squad")
	int32 ObservedPlayerStrength = 8;

	/** Seconds between the two bounding elements' movement phases. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Squad")
	float BoundPhaseDurationS = 6.0f;

	/** Seconds of contact before the reserve counter-attacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Squad")
	float CounterAttackDelayS = 25.0f;

	/** Player strength ratio below which the counter-attack is triggered immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Squad")
	float CounterAttackStrengthRatio = 0.5f;

	/** Seconds of contact required before the squad calls its own support fire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Squad")
	float SupportRequestDelayS = 40.0f;

	/** Seconds into the current bounding phase. */
	float BoundPhaseElapsedS = 0.0f;

	/** True while the first element is moving and the second is firing. */
	bool bFirstElementMoving = true;

	/** True once the squad has already requested support this engagement. */
	bool bSupportRequested = false;

	/** Selects the highest-threat member as the base of fire. */
	void AssignBaseOfFire();

	/** Advances the bounding-overwatch role swap. */
	void TickBoundingOverwatch(float DeltaSeconds);

	/** Evaluates the counter-attack trigger. */
	void EvaluateCounterAttack();

	/** Chooses a posture from the current tactical situation. */
	void UpdatePosture();
};
