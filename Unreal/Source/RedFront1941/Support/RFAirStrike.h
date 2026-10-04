// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFAirStrike.generated.h"

/** Outcome of an air-support request. */
UENUM(BlueprintType)
enum class ERFAirStrikeState : uint8
{
	Inbound			UMETA(DisplayName = "Inbound"),
	OnTarget		UMETA(DisplayName = "Strafing / bombing"),
	Egressing		UMETA(DisplayName = "Egressing"),
	Intercepted		UMETA(DisplayName = "Shot down by flak"),
	Aborted			UMETA(DisplayName = "Aborted (weather / no mark)")
};

/**
 * Air support call-in (IL-2 Sturmovik, P-47 Thunderbolt).
 *
 * Contract behaviour:
 *   * delay_s is the flight time from the airfield: 45 s for the IL-2, 50 s for the P-47.
 *   * passes / damage_per_pass: the aircraft makes two runs, each delivering its damage
 *     in a radius around the marked point (IL-2 25 m, P-47 30 m).
 *   * armor_penetration_mm: 45 mm for the IL-2's PTAB/rockets, 70 mm for the P-47.
 *   * Flak: Flak 36 (targets_aircraft) and quad 20 mm guns can shoot the aircraft down
 *     before it delivers its ordnance; the interception chance grows with the number of
 *     anti-aircraft units inside the approach corridor.
 *   * Weather: clear_weather is required for the P-47, and low cloud/storm aborts any
 *     air mission; night requires a marked target (flare/designator).
 *
 * Units: seconds, metres for radii, mm RHA for penetration.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFAirStrike : public AActor
{
	GENERATED_BODY()

public:
	ARFAirStrike();

	/** Advances the ingress, the two passes, and the egress. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Requests air support.
	 * @param WorldContextObject Any world context object.
	 * @param CallIn          Contract row (sov_il2_strafe / us_p47_strike).
	 * @param TargetPoint     Marked target on the plane, world centimetres.
	 * @param bTargetMarked   True when a flare/designator marks the target.
	 * @param AAUnitsInCorridor Number of enemy anti-aircraft units near the approach.
	 * @param Caller          Requesting actor.
	 * @return The strike actor, or null when weather/marking rules abort the request.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support", meta = (WorldContext = "WorldContextObject"))
	static ARFAirStrike* RequestAirSupport(const UObject* WorldContextObject, const FRFSupportCallInDef& CallIn,
		const FVector& TargetPoint, bool bTargetMarked, int32 AAUnitsInCorridor, AActor* Caller);

	/** Current state. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	ERFAirStrikeState GetStrikeState() const { return StrikeState; }

	/** Passes delivered so far. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	int32 GetPassesFlown() const { return PassesFlown; }

	/** True when the aircraft was shot down before delivering ordnance. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	bool WasIntercepted() const { return StrikeState == ERFAirStrikeState::Intercepted; }

protected:
	/** Contract row. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FRFSupportCallInDef Mission;

	/** Marked target point, world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FVector TargetPoint = FVector::ZeroVector;

	/** State machine value. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") ERFAirStrikeState StrikeState = ERFAirStrikeState::Inbound;

	/** Seconds until the next event (arrival or next pass). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") float PhaseRemainingS = 0.0f;

	/** Passes already flown. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") int32 PassesFlown = 0;

	/** Requesting actor. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") TObjectPtr<AActor> CallerActor;

	/** Anti-aircraft units counted along the ingress corridor. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") int32 AAThreatCount = 0;

	/** Rolls the flak-interception dice at the moment of the first pass. */
	void ResolveFlakInterception();

	/** Delivers one pass of ordnance on the marked point. */
	void DeliverOnePass();
};
