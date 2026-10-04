// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RFDataTypes.h"
#include "RFSurvivalComponent.generated.h"

class URFInventoryComponent;

/** Long-running bodily action that locks the soldier in place. */
UENUM(BlueprintType)
enum class ERFUseAction : uint8
{
	None		UMETA(DisplayName = "None"),
	Eating		UMETA(DisplayName = "Eating"),
	Drinking	UMETA(DisplayName = "Drinking"),
	Medical		UMETA(DisplayName = "Applying medical item")
};

/**
 * Food / water / stamina / temperature / fatigue model.
 *
 * Design intent: every drain rate is per second and expressed on a 0-100 scale per
 * meter, so the HUD can display percentages and the designers can tune one number.
 * Reference values (design baseline, "regular" difficulty):
 *   food      0.020 /s  (~83 min from full)
 *   hydration 0.030 /s  (~55 min from full, water is the sharper pressure)
 *   stamina   0.500 /s while sprinting, x band stamina_drain
 * Temperature drifts toward the ambient value; 1941-42 winter levels push it well
 * below freezing, which is why black bread and vodka are morale items in the contract.
 *
 * All timers are seconds; temperature is degrees Celsius; the rest is 0-100.
 */
UCLASS(ClassGroup = (RedFront), meta = (BlueprintSpawnableComponent))
class REDFRONT1941_API URFSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URFSurvivalComponent();

	/** Initializes derived values and pushes the difficulty multiplier default. */
	virtual void BeginPlay() override;

	/** Sets contract health for spawned units; player characters keep the 100-point default. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void InitializeHealth(float NewMaxHealth);

	/** Drains food/water/stamina, advances temperature/fatigue, resolves use actions. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------- Vitals ----------------------------------

	/** Health points remaining. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetHealth() const { return Health; }

	/** Satiety, 0-100; 0 starts starvation damage. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetFood() const { return Food; }

	/** Hydration, 0-100; drains fastest in the heat and on the march. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetHydration() const { return Hydration; }

	/** Stamina, 0-100; gates sprinting and steadies the aim. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetStamina() const { return Stamina; }

	/** Body temperature in degrees Celsius (36.6 is normal, below 35 is hypothermia). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetTemperatureC() const { return TemperatureC; }

	/** Ambient temperature in degrees Celsius for the current level. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival") void SetAmbientTemperatureC(float NewAmbientC);

	/** Fatigue, 0-100; rises with load and distance, recovered only at resupply. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetFatigue() const { return Fatigue; }

	/** Morale, 0-100; vodka and padded rations push it up, bombardments push it down. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetMorale() const { return Morale; }

	/** Blood loss, 0-100; any value above 0 applies damage over time until bandaged. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") float GetBloodLoss() const { return BloodLoss; }

	/** True once health reached 0; the game mode turns this into a mission failure. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival") bool IsDead() const { return Health <= 0.0f; }

	// -------------------------- Movement modifiers ---------------------------

	/** Speed multiplier from exhaustion, pain and hypothermia. Dimensionless. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	float GetMobilityMultiplier() const;

	/** Multiplier applied to the weapon's spread by stamina and pain. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	float GetAimSwayMultiplier() const;

	/** Stamina drain per second at the current load band and activity. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	float GetStaminaDrainPerS() const;

	/** Sets the load-band stamina drain multiplier (from URFWeightComponent). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void SetLoadStaminaMultiplier(float NewMultiplier);

	/** True while sprinting; raises the stamina drain and blocks regeneration. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void SetSprinting(bool bInSprint) { bSprinting = bInSprint; }

	/** Records distance moved this frame in metres; feeds the fatigue model. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void ReportDistanceTravelledM(float DistanceM);

	// ------------------------------ Use actions ------------------------------

	/**
	 * Eats the best available food item (by calories per kg).
	 * @return True when an item was found and the eating action started.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	bool EatBestAvailableFood();

	/** Drinks the best available water item (by hydration per kg). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	bool DrinkBestAvailableWater();

	/** Applies the best available medical item (medkit > bandage > stimulant). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	bool UseBestMedicalItem();

	/** Action currently locking the soldier in place. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	ERFUseAction GetCurrentUseAction() const { return CurrentAction; }

	/** Seconds remaining on the current use action; 0 when idle. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	float GetUseActionRemainingS() const { return ActionRemainingS; }

	/** True when the soldier must stand still (eating/drinking/medical in progress). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Survival")
	bool IsLockedInPlace() const { return CurrentAction != ERFUseAction::None; }

	// -------------------------------- Damage ---------------------------------

	/**
	 * Applies damage on the 0-100 health scale.
	 * @param Amount        Damage to subtract from health.
	 * @param bCauseBleeding True for ballistic/fragment wounds (start blood loss).
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void ApplyDamage(float Amount, bool bCauseBleeding);

	/** Restores health, capped at 100. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void Heal(float Amount);

	/** Adds or removes morale. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void AddMorale(float Delta);

	/** Sets the difficulty drain multiplier from campaigns.json difficulty_scaling. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Survival")
	void SetDifficultyDrainMultiplier(float NewMultiplier);

protected:
	// Vitals (0-100 scale unless noted; enemy health contracts may exceed 100).
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Health = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float MaxHealth = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Food = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Hydration = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Stamina = 100.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Fatigue = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float Morale = 70.0f;
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float BloodLoss = 0.0f;

	/** Core body temperature in degrees Celsius. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float TemperatureC = 36.6f;

	/** Ambient temperature in degrees Celsius (levels set this: -25 in the 1941 winter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float AmbientTemperatureC = 5.0f;

	// ---------------------------- Drain rates (/s) ---------------------------

	/** Food drain per second at rest (0-100 scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float FoodDrainPerS = 0.02f;

	/** Hydration drain per second at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float HydrationDrainPerS = 0.03f;

	/** Stamina drain per second while sprinting (multiplied by the load band). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float SprintStaminaDrainPerS = 0.5f;

	/** Stamina drain per second while walking loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float WalkStaminaDrainPerS = 0.06f;

	/** Stamina recovery per second while standing still and not suppressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float StaminaRegenPerS = 0.14f;

	/** Starvation/hypothermia damage per second once their thresholds are crossed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float StarvationDamagePerS = 0.35f;

	/** Blood-loss damage per second per 100 points of BloodLoss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float BloodLossDamagePerSPer100 = 0.5f;

	/** Fatigue gained per metre travelled (scales with the load band in code). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Survival") float FatiguePerMetre = 0.02f;

	/** Multiplier applied to every drain by the difficulty tier (veteran 1.2, historical 1.5). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float DifficultyDrainMultiplier = 1.0f;

	/** Load-band stamina multiplier supplied by URFWeightComponent. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float LoadStaminaMultiplier = 1.0f;

	// --------------------------- Use-action state ----------------------------

	/** Action locking the soldier in place. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") ERFUseAction CurrentAction = ERFUseAction::None;

	/** Seconds left on the current action. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") float ActionRemainingS = 0.0f;

	/** Inventory index consumed when the action completes (-1 when none). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") int32 PendingItemIndex = -1;

	/** True while the player holds the sprint input. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Survival") bool bSprinting = false;

	/** Distance accumulated this frame in metres, consumed by the fatigue model. */
	float PendingDistanceM = 0.0f;

	/** Resolves the inventory of the owning character. */
	URFInventoryComponent* GetInventory() const;

	/** Starts a use action for the item in the given inventory index. */
	bool BeginUseItem(int32 InventoryIndex, ERFUseAction Action, float DurationS);

	/** Applies an item's effect (food/water/medical) to the vitals. */
	void ApplyItemEffect(const FRFItemDef& Item);

	/** Ticks the current use action and resolves it when the timer elapses. */
	void TickUseAction(float DeltaTime);
};
