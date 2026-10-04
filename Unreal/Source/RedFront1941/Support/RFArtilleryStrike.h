// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFArtilleryStrike.generated.h"

/**
 * One indirect-fire mission: shells land over time inside an ellipse of dispersion.
 *
 * Contract behaviour (support.json call_ins):
 *   * delay_s      time from confirmation to the first shell (12 s for the 82 mm up to
 *                  50 s for the 152 mm; the observer's corrections do not shorten it).
 *   * rounds       shells delivered, spread evenly across 60% of the mission window.
 *   * spread_base_m dispersion radius before corrections; reduced by 35% with
 *                  binoculars and by 50% with a signal flare, doubled when the
 *                  observer loses line of sight.
 *   * Friendly fire: the danger radius is the effect radius; the subsystem exposes it
 *                  so the HUD can warn when the aim point is inside it.
 *
 * Units: seconds, metres for radii (converted to centimetres for world queries).
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFArtilleryStrike : public AActor
{
	GENERATED_BODY()

public:
	ARFArtilleryStrike();

	/** Ticks the delay, then the shell schedule. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Spawns an artillery mission.
	 * @param WorldContextObject Any world context object (usually the subsystem).
	 * @param CallIn       Contract row for the mission.
	 * @param TargetPoint  Aim point, world centimetres.
	 * @param SpreadM      Effective dispersion radius in metres (already corrected).
	 * @param Caller       Player controller of the requesting player, for messages.
	 * @return The spawned mission actor, or null on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support", meta = (WorldContext = "WorldContextObject"))
	static ARFArtilleryStrike* StartFireMission(const UObject* WorldContextObject, const FRFSupportCallInDef& CallIn,
		const FVector& TargetPoint, float SpreadM, AActor* Caller);

	/** Seconds until the next shell; 0 when the mission is complete. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetTimeToNextRoundS() const { return TimeToNextRoundS; }

	/** Shells already delivered. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	int32 GetRoundsFired() const { return RoundsFired; }

	/** True once every shell has landed and the actor can be destroyed. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	bool IsMissionComplete() const { return bMissionComplete; }

	/** Danger radius in metres: anyone inside is at risk of friendly fire. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetDangerRadiusM() const;

protected:
	/** Mission contract. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FRFSupportCallInDef Mission;

	/** Aim point on the plane, centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	FVector TargetPoint = FVector::ZeroVector;

	/** Effective dispersion radius, metres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float SpreadM = 0.0f;

	/** Seconds left before the first shell. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float DelayRemainingS = 0.0f;

	/** Seconds to the next shell once firing has started. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	float TimeToNextRoundS = 0.0f;

	/** Shells fired so far. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	int32 RoundsFired = 0;

	/** True once the mission has finished. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	bool bMissionComplete = false;

	/** Requesting actor, used as the damage instigator. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support")
	TObjectPtr<AActor> CallerActor;

	/** Delivers one shell at a point sampled inside the dispersion ellipse. */
	void FireOneShell();

	/** Samples a point inside the dispersion ellipse, world centimetres. */
	FVector SampleImpactPoint() const;
};
