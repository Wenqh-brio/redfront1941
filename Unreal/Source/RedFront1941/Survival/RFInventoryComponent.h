// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RFDataTypes.h"
#include "RFInventoryComponent.generated.h"

/** One occupied inventory slot. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFInventoryEntry
{
	GENERATED_BODY()

	/** Slot index inside the slot kind, 0-based. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	int32 SlotIndex = 0;

	/** Which capacity pool the entry occupies (primary/secondary/grenade/supply). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	ERFSlotType SlotType = ERFSlotType::Supply;

	/** Resolved contract description of the occupying item. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	FRFItemDef Item;

	/** Units left in this slot (grenades/food/medical stack by count_per_slot). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	int32 Quantity = 0;

	/** True when the entry is a weapon rather than a consumable item. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	bool bIsWeapon = false;

	/** Weapon contract id when bIsWeapon is true. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	FName WeaponId = NAME_None;

	/** Remaining ammo in the weapon's magazine when bIsWeapon is true. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	int32 MagazineAmmo = 0;

	/** Mass of this entry in kilograms (weight_kg x Quantity, or the weapon weight). */
	float GetWeightKg() const;
};

/**
 * Slot-based inventory matching classes.json "slots"
 * ({ "primary": 1, "secondary": 1, "grenade": 2, "supply": 4 } for the rifleman).
 *
 * The inventory is the single source of truth for carried mass: URFWeightComponent
 * asks it for GetTotalCarriedWeightKg(), which is why food and water consumption
 * immediately lightens the soldier (equipment.json: "喝完减重 1kg").
 */
UCLASS(ClassGroup = (RedFront), meta = (BlueprintSpawnableComponent))
class REDFRONT1941_API URFInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URFInventoryComponent();

	/** Applies the slot capacity rules of a class definition. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Inventory")
	void ConfigureSlots(const FRFClassDef& ClassDef);

	/** Number of slots of the given kind granted by the current class. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 GetSlotCapacity(ERFSlotType SlotType) const;

	/** Number of occupied slots of the given kind. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 GetUsedSlotCount(ERFSlotType SlotType) const;

	/**
	 * Adds an item stack to a free slot of its natural kind.
	 * @return True when a slot was free and the stack was added.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Inventory")
	bool AddItem(const FRFItemDef& Item, int32 Quantity);

	/** Adds a weapon to a primary/secondary slot. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Inventory")
	bool AddWeapon(FName WeaponId, float WeightKg, int32 MagazineAmmo, ERFSlotType SlotType);

	/**
	 * Consumes one unit of the entry in the given slot.
	 * @return The consumed entry (with Quantity == 1) or a default entry when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Inventory")
	FRFInventoryEntry ConsumeFromSlot(int32 EntryIndex);

	/** Finds the best food item by calories per kilogram; index -1 when none. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 FindBestFoodIndex() const;

	/** Finds the best drinkable item by hydration per kilogram; index -1 when none. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 FindBestWaterIndex() const;

	/** Finds the highest-priority medical item (medkit > bandage > sulfa > morphine). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 FindBestMedicalIndex() const;

	/** Finds an item of the given behaviour kind; index -1 when none. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 FindItemIndexByKind(ERFItemKind Kind) const;

	/** Total carried mass of every entry in kilograms. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	float GetTotalCarriedWeightKg() const;

	/** Ammo-carry capacity multiplier granted by ammo pouches (1.4 when carried). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	float GetAmmoCapacityMultiplier() const;

	/** Reload-speed multiplier granted by ammo pouches (1.1 when carried). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	float GetReloadSpeedMultiplier() const;

	/** Every occupied slot, in stable order (slot kind, then index). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	const TArray<FRFInventoryEntry>& GetEntries() const { return Entries; }

	/** Index of the active primary/secondary weapon entry, or -1. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Inventory")
	int32 GetActiveWeaponIndex() const { return ActiveWeaponIndex; }

	/** Switches between the primary and secondary weapon slots (SwapWeapon). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Inventory")
	void CycleActiveSlot();

protected:
	/** Occupied slots. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	TArray<FRFInventoryEntry> Entries;

	/** Slot capacity per kind, from the class definition. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	TMap<ERFSlotType, int32> SlotCapacities;

	/** Index into Entries of the equipped weapon. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Inventory")
	int32 ActiveWeaponIndex = -1;

	/** Picks the slot kind an item naturally occupies (grenades vs supply). */
	static ERFSlotType GetNaturalSlotForItem(const FRFItemDef& Item);

	/** Lowest free slot index of the given kind, or -1 when the kind is full. */
	int32 FindFreeSlotIndex(ERFSlotType SlotType) const;
};
