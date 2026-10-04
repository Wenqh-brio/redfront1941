// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RFDataTypes.h"
#include "RFWeightComponent.generated.h"

/**
 * Carried-weight model, mirroring classes.json "weight_system".
 *
 * Contract table (ratio = carried kg / capacity kg):
 *   light      ratio <= 0.60  speed x1.08  stamina drain x0.80
 *   standard   ratio <= 0.85  speed x1.00  stamina drain x1.00
 *   heavy      ratio <= 1.00  speed x0.92  stamina drain x1.25
 *   overloaded ratio >  1.00  speed x0.78  stamina drain x1.70, sprint disabled
 *
 * Units: kilograms for weight, multipliers are dimensionless.
 */
UCLASS(ClassGroup = (RedFront), meta = (BlueprintSpawnableComponent))
class REDFRONT1941_API URFWeightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URFWeightComponent();

	/** Total carried weight, kg: base kit + weapons + items + armour + radio. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetTotalWeightKg() const { return TotalWeightKg; }

	/** Recomputes the carried weight from the owning character's inventory. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void RecalculateWeight();

	/** Overrides the total carried weight (used by the loadout subsystem on spawn). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void SetTotalWeightKg(float NewWeightKg);

	/** Capacity in kg: base_capacity_kg (24 Soviet / 26 US) scaled by class/passive mods. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetCapacityKg() const { return CapacityKg; }

	/** Sets the capacity, e.g. 26 kg for a US campaign soldier. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void SetCapacityKg(float NewCapacityKg);

	/** carried / capacity. Above 1.0 is the overloaded band. Dimensionless. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetLoadRatio() const;

	/** Current band, derived from GetLoadRatio() against the contract table. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	ERFLoadBand GetLoadBand() const;

	/** Movement multiplier of the current band (1.08 / 1.00 / 0.92 / 0.78). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetSpeedMultiplier() const;

	/** Stamina drain multiplier of the current band (0.80 / 1.00 / 1.25 / 1.70). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetStaminaDrainMultiplier() const;

	/** True in the overloaded band: sprint is locked out and ladders are slow. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	bool IsSprintLocked() const { return GetLoadBand() == ERFLoadBand::Overloaded; }

	/**
	 * Mobility multiplier of the equipped primary weapon (weapons.json mobility_mod).
	 * The Maxim M1910 is 0.35, the M1 Carbine 1.05.
	 */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	float GetWeaponMobilityModifier() const { return WeaponMobilityMod; }

	/** Updates the weapon mobility contribution when the primary weapon changes. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void SetWeaponMobilityModifier(float NewModifier);

	/** Flat capacity modifiers (anti-tank "heavy carry" +20%, scout penalty -1 band). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void SetCapacityMultiplier(float NewMultiplier);

	/** Replaces the band table (parsed from classes.json load_bands). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weight")
	void SetLoadBands(const TArray<FRFWeightBand>& NewBands);

	/** Band definition matching an id; returns the standard band as a fallback. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	FRFWeightBand GetBandDefinition(ERFLoadBand Band) const;

	/** Maps a contract band id string ("light"/"standard"/"heavy"/"overloaded"). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weight")
	static ERFLoadBand LoadBandFromId(FName BandId);

protected:
	/** Sum of all carried mass in kilograms. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	float TotalWeightKg = 0.0f;

	/** Carry capacity in kilograms (Soviet base 24, US base 26). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Weight")
	float CapacityKg = 24.0f;

	/** Multiplier applied to CapacityKg by class passives or equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Weight")
	float CapacityMultiplier = 1.0f;

	/** Weight of the base kit (uniform, helmet, bayonet) in kilograms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Weight")
	float BaseKitWeightKg = 3.0f;

	/** Muzzle-weight mobility modifier of the currently equipped primary weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	float WeaponMobilityMod = 1.0f;

	/** Load bands from the contract, ordered light -> overloaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Weight")
	TArray<FRFWeightBand> LoadBands;

	/** Ensures LoadBands holds the contract table even before JSON loading runs. */
	void EnsureDefaultLoadBands();
};
