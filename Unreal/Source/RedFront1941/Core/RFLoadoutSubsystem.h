// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RFLoadoutSubsystem.generated.h"

/** A single slot assignment inside a loadout preset. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFLoadoutEntry
{
	GENERATED_BODY()

	/** Slot kind this entry occupies. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RedFront|Loadout")
	ERFSlotType Slot = ERFSlotType::Primary;

	/** Weapons.json id when bIsWeapon, otherwise the equipment.json id. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RedFront|Loadout")
	FName DefinitionId = NAME_None;

	/** True for weapon entries, false for item entries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RedFront|Loadout")
	bool bIsWeapon = false;

	/** Units taken of an item (1 for weapons). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RedFront|Loadout")
	int32 Quantity = 1;
};

/** Result of validating a loadout against the level contract and class rules. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFLoadoutValidation
{
	GENERATED_BODY()

	/** True when every rule passed and the loadout may be deployed. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Loadout")
	bool bValid = true;

	/** Total carried mass in kilograms. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Loadout")
	float TotalWeightKg = 0.0f;

	/** Budget the loadout was validated against, kilograms. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Loadout")
	float WeightBudgetKg = 24.0f;

	/** Load band the resulting weight falls into. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Loadout")
	ERFLoadBand Band = ERFLoadBand::Standard;

	/** Human-readable reasons the loadout is rejected (empty when valid). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Loadout")
	TArray<FText> Problems;
};

/**
 * Owns loadout validation and preset save/load.
 *
 * Rules enforced (all straight from the contracts):
 *   * Slot counts: at most classes.json slots[slot] entries of each kind.
 *   * Weight budget: total kitchen-scale mass must fit level.weight_budget_kg; the
 *     character may still deploy overloaded, but the mission briefing flags it.
 *   * Weapons must be allowed for the class (signature weapons) or already unlocked.
 *   * Items must belong to the player's faction or be faction "both".
 *
 * Presets persist through GConfig into Saved/Config/Windows/RFLoadouts.ini, so they
 * survive a crash without needing a save-game slot.
 */
UCLASS()
class REDFRONT1941_API URFLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Installs the parsed contract tables used for validation. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Loadout")
	void SetDefinitions(const TMap<FName, FRFWeaponDef>& InWeapons,
		const TMap<FName, FRFItemDef>& InItems,
		const TMap<FName, FRFClassDef>& InClasses);

	/** Default capacity in kilograms for a faction (Soviet 24, US 26). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Loadout")
	static float GetBaseCapacityKg(ERFFaction Faction);

	/** Total mass of a loadout in kilograms (weapons + items, honouring quantities). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Loadout")
	float ComputeLoadoutWeightKg(const TArray<FRFLoadoutEntry>& Entries) const;

	/**
	 * Validates a loadout for a class and a level.
	 * @param ClassDef  Class whose slot rules apply.
	 * @param Level     Level whose weight budget and support list apply.
	 * @return Validation result including the concrete problems when invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Loadout")
	FRFLoadoutValidation ValidateLoadout(const FRFClassDef& ClassDef, const FRFLevelDef& Level,
		const TArray<FRFLoadoutEntry>& Entries) const;

	/** Maps a carried weight to a load band using the contract ratio table. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Loadout")
	static ERFLoadBand ComputeLoadBand(float TotalWeightKg, float CapacityKg);

	/** Saves the working loadout as a named preset. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Loadout")
	bool SavePreset(FName PresetName, const TArray<FRFLoadoutEntry>& Entries);

	/** Loads a named preset; false when it does not exist. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Loadout")
	bool LoadPreset(FName PresetName, TArray<FRFLoadoutEntry>& OutEntries) const;

	/** All saved preset names. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Loadout")
	TArray<FName> GetPresetNames() const;

	/** The loadout last confirmed on the briefing screen. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Loadout")
	const TArray<FRFLoadoutEntry>& GetWorkingLoadout() const { return WorkingLoadout; }

	/** Replaces the working loadout (briefing screen edits this). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Loadout")
	void SetWorkingLoadout(const TArray<FRFLoadoutEntry>& Entries) { WorkingLoadout = Entries; }

protected:
	/** Weapon contracts keyed by id. */
	UPROPERTY() TMap<FName, FRFWeaponDef> Weapons;

	/** Item contracts keyed by id. */
	UPROPERTY() TMap<FName, FRFItemDef> Items;

	/** Class contracts keyed by id. */
	UPROPERTY() TMap<FName, FRFClassDef> Classes;

	/** Loadout currently being edited on the briefing screen. */
	UPROPERTY() TArray<FRFLoadoutEntry> WorkingLoadout;

	/** Section prefix used for every preset key in the ini file. */
	static const TCHAR* PresetSectionPrefix;

	/** Builds an ini key for a slot inside a preset (e.g. "Slot0"). */
	static FString MakeSlotKey(int32 EntryIndex);
};
