// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "RFSupportSubsystem.generated.h"

class ARFAirStrike;
class ARFArtilleryStrike;
class ARFTankCallIn;
class ARFGameState;

/** One queued or running support mission. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFSupportRequest
{
	GENERATED_BODY()

	/** Call-in contract row this request came from. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FRFSupportCallInDef CallIn;

	/** Target point on the gameplay plane, world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FVector TargetPoint = FVector::ZeroVector;

	/** Mission second at which the request was confirmed. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float RequestedAtS = 0.0f;

	/** Effective delay in seconds after the observer/flare/comms modifiers. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float DelayS = 0.0f;

	/** Effective dispersion radius in metres after observer/binocular corrections. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float SpreadM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	ERFSupportState State = ERFSupportState::Idle;

	/** True when a forward observer held line of sight at confirmation time. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	bool bObserverConfirmed = false;

	/** True when a signal flare was required (night/smoke) and fired. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	bool bFlareRequired = false;

	/** Rear-road route for armour call-ins, world centimetres (empty = no tank access). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	TArray<FVector> RoutePoints;

	/** Enemy anti-aircraft units counted along the ingress corridor (air call-ins). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	int32 AAThreatCountInCorridor = 0;
};

/**
 * Command points, cooldowns and the support call-in queue.
 *
 * Contract rules implemented (support.json):
 *   * CP: start 2, max 10, regen 0.035 /s x difficulty cp_regen_mult, +2 per objective.
 *   * Triple gating: CP cost + per-call-in cooldown + comms delay (minimum 25 s in
 *     practice; the cheapest 82 mm mission is 12 s but is rarely the one you need).
 *   * Observer: the spread is multiplied by 1.0 without line of sight, 0.65 with a
 *     binocular-equipped observer, and 0.5 with a signal flare.
 *   * Radio: if the squad radio is destroyed, all call-ins are locked out for 60 s and
 *     already-queued missions have their delay doubled (line_break_effect).
 *   * Counter-intel: 20 s after a call, enemy counter-battery may fire (hard+ only).
 *
 * All timers are seconds; CP are whole points spent, tracked fractionally.
 */
UCLASS()
class REDFRONT1941_API URFSupportSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Installs the contract tables and starts CP regeneration. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Ticks CP regeneration and the call-in queue; called from the game mode. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void TickSupport(float DeltaSeconds);

	/** Installs the parsed support contract rows and comms/CP rules. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void InitializeContracts(const TArray<FRFSupportCallInDef>& CallIns,
		const FRFCommandPointRules& InCommandPointRules, const FRFCommsRules& InCommsRules);

	/**
	 * Attempts to queue a support mission.
	 * @param CallInId     support.json id, e.g. "sov_howitzer_122".
	 * @param TargetPoint  Aim point on the plane, world centimetres.
	 * @param bObserverLOS True when a forward observer has line of sight to the target.
	 * @param bHasBinoculars True when the observer carries binoculars (-35% spread).
	 * @return True when the request entered the queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	bool RequestSupport(FName CallInId, const FVector& TargetPoint, bool bObserverLOS, bool bHasBinoculars);

	/** Confirms the wheel selection made by CallSupport_* inputs. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	bool ConfirmPendingRequest();

	/** Selects a call-in type for the pending request (from the three support keys). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void SetPendingCallInType(ERFSupportCallType CallType);

	/** Registers the cursor target point for the pending request, world centimetres. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void SetPendingTarget(const FVector& TargetPoint);

	/** Registers the observation state (observer line of sight, binoculars) for the request. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void SetPendingObservation(bool bObserverLOS, bool bHasBinoculars);

	/** Call-ins currently queued or running. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	const TArray<FRFSupportRequest>& GetQueue() const { return Queue; }

	/** Every installed call-in contract, keyed by id (used by UI and by the AI). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	const TMap<FName, FRFSupportCallInDef>& GetAllCallIns() const { return CallInDefinitions; }

	/** Seconds of cooldown left for a call-in; 0 when ready. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetCooldownRemainingS(FName CallInId) const;

	/** True when the squad radio is alive; false during the 60 s lockout. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	bool IsRadioOperational() const;

	/** Notifies the subsystem that the squad radio was destroyed. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void NotifyRadioDestroyed();

	/** Notifies the subsystem that a new radio was picked up (ends the lockout early). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void NotifyRadioRestored();

	/** Notifies the subsystem that the comms line broke (delay doubling for queued missions). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void NotifyLineBreak(float DurationS);

	/** True while the line is broken; queued missions run at double delay. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	bool IsLineBroken() const { return LineBreakRemainingS > 0.0f; }

	/** Danger radius of the last confirmed mission in metres (friendly-fire warning). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetLastDangerRadiusM() const { return LastDangerRadiusM; }

protected:
	/** Currently queued / running missions, oldest first. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	TArray<FRFSupportRequest> Queue;

	/** Contract table keyed by call-in id. */
	UPROPERTY() TMap<FName, FRFSupportCallInDef> CallInDefinitions;

	/** CP economy. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FRFCommandPointRules CommandPointRules;

	/** Radio/comms constraints. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FRFCommsRules CommsRules;

	/** Cooldown remaining per call-in id, seconds. */
	UPROPERTY() TMap<FName, float> CooldownRemainingS;

	/** Fractional CP accumulator spent against the game state pool. */
	UPROPERTY() float CpFractionAccumulator = 0.0f;

	/** Call-in type pre-selected by the CallSupport_* inputs. */
	UPROPERTY() ERFSupportCallType PendingCallInType = ERFSupportCallType::Indirect;

	/** Cursor target of the pending request, world centimetres. */
	UPROPERTY() FVector PendingTargetPoint = FVector::ZeroVector;

	/** True when the pending request has a forward observer with line of sight. */
	UPROPERTY() bool bPendingObserverLOS = true;

	/** True when the pending request uses binoculars (-35% spread). */
	UPROPERTY() bool bPendingHasBinoculars = false;

	/** Seconds left of the radio-destroyed lockout. */
	UPROPERTY() float RadioLockoutRemainingS = 0.0f;

	/** Seconds left of the line-break delay penalty. */
	UPROPERTY() float LineBreakRemainingS = 0.0f;

	/** Danger radius of the last mission, metres. */
	UPROPERTY() float LastDangerRadiusM = 0.0f;

	/** Kicks off the concrete effect actor for a mission. */
	void LaunchSupportMission(FRFSupportRequest& Request);

	/** Spread in metres before observer/binocular/flare corrections. */
	float ResolveSpreadM(const FRFSupportCallInDef& CallIn, bool bObserverLOS, bool bHasBinoculars) const;
};
