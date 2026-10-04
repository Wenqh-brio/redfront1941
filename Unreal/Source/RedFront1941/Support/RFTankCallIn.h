// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFTankCallIn.generated.h"

class URFVehicleArmorComponent;

/** Lifecycle of an armour call-in. */
UENUM(BlueprintType)
enum class ERFArmorCallState : uint8
{
	AwaitingArrival	UMETA(DisplayName = "Awaiting arrival (rolling up the road)"),
	OnStation		UMETA(DisplayName = "On station"),
	Withdrawing		UMETA(DisplayName = "Withdrawing (fuel/ammo spent)"),
	Destroyed		UMETA(DisplayName = "Destroyed")
};

/**
 * Tank / self-propelled gun support call-in.
 *
 * Contract behaviour:
 *   * arrives_from "rear_road": vehicles enter at the level's rear road entry spline and
 *     drive toward the objective; they cannot cross rubble or dense forest, so a level
 *     with no road to the target yields an aborted call-in rather than teleporting tanks.
 *   * units / armor_mm / hp_each / weapon / penetration_mm describe the delivered platoon
 *     (T-34/76: 3 units, 45 mm, 340 hp, 76 mm F-34 with 78 mm penetration).
 *   * desant_slots: the player may load that many infantry onto the tanks; the tank-desant
 *     mechanic is the reason the T-34 call-in costs 4 CP instead of 3.
 *   * Duty cycle: the contract states armour withdraws after 8 minutes of fuel.
 *   * Anti-air interception: a called-in vehicle column can be strafed by enemy air;
 *     the interception chance grows with difficulty and with the number of units.
 *
 * Units: seconds, metres for distances, mm RHA for armour and penetration, km/h for speed.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFTankCallIn : public AActor
{
	GENERATED_BODY()

public:
	ARFTankCallIn();

	/** Caches the arrival path and starts the withdrawal timer. */
	virtual void BeginPlay() override;

	/** Advances the arrival phase and the duty timer. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Requests an armour platoon.
	 * @param WorldContextObject Any world context object.
	 * @param CallIn        Contract row (sov_tank_t34_76 / sov_tank_isu152 / us_sherman_platoon).
	 * @param TargetPoint   Objective the platoon should advance to, world centimetres.
	 * @param ArrivalPath   Ordered points along the rear road, world centimetres; empty
	 *                      means the level has no road and the request is aborted.
	 * @param Caller        Requesting actor.
	 * @return The call-in actor, or null when the request cannot be honoured.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support", meta = (WorldContext = "WorldContextObject"))
	static ARFTankCallIn* RequestTankSupport(const UObject* WorldContextObject, const FRFSupportCallInDef& CallIn,
		const FVector& TargetPoint, const TArray<FVector>& ArrivalPath, AActor* Caller);

	/** Current lifecycle state. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	ERFArmorCallState GetCallState() const { return CallState; }

	/** Vehicles still operational (starts at the contract "units" value). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	int32 GetOperationalVehicles() const { return OperationalVehicles; }

	/** Seconds of fuel/ammo left before the platoon withdraws (contract: 480 s). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	float GetDutyRemainingS() const { return DutyRemainingS; }

	/** Seats still free for infantry riding the tanks (tank desant). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Support")
	int32 GetFreeDesantSlots() const { return FreeDesantSlots; }

	/**
	 * Loads a soldier onto the nearest operational tank (tank desant).
	 * @return True when a seat was available.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	bool TryLoadDesant(AActor* Soldier);

	/** Unloads every passenger (called when the platoon dismounts or is destroyed). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void UnloadAllDesant();

	/** Registers a vehicle loss (called by the vehicle actors on destruction). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Support")
	void ReportVehicleLost();

protected:
	/** Contract row of the platoon. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FRFSupportCallInDef Platoon;

	/** Objective point, world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") FVector ObjectivePoint = FVector::ZeroVector;

	/** Ordered rear-road waypoints; empty means "no route". */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") TArray<FVector> RoutePoints;

	/** Lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") ERFArmorCallState CallState = ERFArmorCallState::AwaitingArrival;

	/** Vehicles still running. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") int32 OperationalVehicles = 0;

	/** Seconds left before withdrawal; 480 s per the contract note. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") float DutyRemainingS = 480.0f;

	/** Seats still free on the delivered vehicles. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") int32 FreeDesantSlots = 0;

	/** Seconds until the head of the column reaches the objective. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") float ArrivalRemainingS = 0.0f;

	/** Requesting actor. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Support") TObjectPtr<AActor> CallerActor;

	/** Angle tolerated between the road and the objective before the route is rejected, degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Support") float MaxRouteDetourDeg = 75.0f;

	/** Rolls the air-interception dice once, on arrival. */
	void ResolveAirInterception();

	/** Total route length in metres (used to derive the arrival delay from speed_kph). */
	float GetRouteLengthM() const;
};
