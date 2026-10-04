// Copyright RedFront1941. All Rights Reserved.

#include "Survival/RFInventoryComponent.h"

URFInventoryComponent::URFInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Default rifleman layout so the component is usable before a class is applied.
	SlotCapacities.Add(ERFSlotType::Primary, 1);
	SlotCapacities.Add(ERFSlotType::Secondary, 1);
	SlotCapacities.Add(ERFSlotType::Grenade, 2);
	SlotCapacities.Add(ERFSlotType::Supply, 4);
}

float FRFInventoryEntry::GetWeightKg() const
{
	if (bIsWeapon)
	{
		// Weapon mass is authored per weapon, not per magazine; ammo mass is folded into
		// the weight_kg contract value for simplicity.
		return Item.WeightKg;
	}
	return Item.WeightKg * static_cast<float>(FMath::Max(0, Quantity));
}

void URFInventoryComponent::ConfigureSlots(const FRFClassDef& ClassDef)
{
	SlotCapacities.Reset();
	for (const FRFLoadoutSlot& Slot : ClassDef.Slots)
	{
		SlotCapacities.Add(Slot.Slot, FMath::Max(0, Slot.Count));
	}

	// Drop entries that no longer fit (class swap during the briefing screen).
	Entries.RemoveAll([this](const FRFInventoryEntry& Entry)
	{
		return Entry.SlotIndex >= GetSlotCapacity(Entry.SlotType);
	});
	ActiveWeaponIndex = -1;
}

int32 URFInventoryComponent::GetSlotCapacity(ERFSlotType SlotType) const
{
	const int32* Found = SlotCapacities.Find(SlotType);
	return Found != nullptr ? *Found : 0;
}

int32 URFInventoryComponent::GetUsedSlotCount(ERFSlotType SlotType) const
{
	int32 Count = 0;
	for (const FRFInventoryEntry& Entry : Entries)
	{
		if (Entry.SlotType == SlotType)
		{
			++Count;
		}
	}
	return Count;
}

int32 URFInventoryComponent::FindFreeSlotIndex(ERFSlotType SlotType) const
{
	const int32 Capacity = GetSlotCapacity(SlotType);
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		const bool bTaken = Entries.ContainsByPredicate([SlotType, Index](const FRFInventoryEntry& Entry)
		{
			return Entry.SlotType == SlotType && Entry.SlotIndex == Index;
		});
		if (!bTaken)
		{
			return Index;
		}
	}
	return -1;
}

ERFSlotType URFInventoryComponent::GetNaturalSlotForItem(const FRFItemDef& Item)
{
	// equipment.json categories map straight onto slot kinds: grenades (including
	// satchel charges and AT grenades) go to grenade slots, everything else to supply.
	return Item.Category == ERFItemCategory::Grenade ? ERFSlotType::Grenade : ERFSlotType::Supply;
}

bool URFInventoryComponent::AddItem(const FRFItemDef& Item, int32 Quantity)
{
	if (Quantity <= 0)
	{
		return false;
	}

	const ERFSlotType TargetSlot = GetNaturalSlotForItem(Item);

	// Top up an existing stack first; stacks never exceed count_per_slot.
	for (FRFInventoryEntry& Entry : Entries)
	{
		if (!Entry.bIsWeapon && Entry.Item.Id == Item.Id && Entry.SlotType == TargetSlot)
		{
			const int32 MaxStack = FMath::Max(1, Item.CountPerSlot);
			const int32 Space = MaxStack - Entry.Quantity;
			if (Space > 0)
			{
				const int32 Added = FMath::Min(Space, Quantity);
				Entry.Quantity += Added;
				Quantity -= Added;
				if (Quantity <= 0)
				{
					return true;
				}
			}
		}
	}

	// Then fill free slots with full stacks.
	while (Quantity > 0)
	{
		const int32 FreeIndex = FindFreeSlotIndex(TargetSlot);
		if (FreeIndex < 0)
		{
			return false;
		}

		FRFInventoryEntry Entry;
		Entry.SlotIndex = FreeIndex;
		Entry.SlotType = TargetSlot;
		Entry.Item = Item;
		Entry.Quantity = FMath::Min(Quantity, FMath::Max(1, Item.CountPerSlot));
		Quantity -= Entry.Quantity;
		Entries.Add(Entry);
	}
	return true;
}

bool URFInventoryComponent::AddWeapon(FName WeaponId, float WeightKg, int32 MagazineAmmo, ERFSlotType SlotType)
{
	const int32 FreeIndex = FindFreeSlotIndex(SlotType);
	if (FreeIndex < 0)
	{
		return false;
	}

	FRFInventoryEntry Entry;
	Entry.SlotIndex = FreeIndex;
	Entry.SlotType = SlotType;
	Entry.bIsWeapon = true;
	Entry.WeaponId = WeaponId;
	Entry.MagazineAmmo = FMath::Max(0, MagazineAmmo);
	// Weapon mass is carried in the item weight field so GetWeightKg() has one path.
	Entry.Item.WeightKg = WeightKg;
	Entry.Quantity = 1;
	Entries.Add(Entry);

	if (ActiveWeaponIndex < 0 && SlotType == ERFSlotType::Primary)
	{
		ActiveWeaponIndex = Entries.Num() - 1;
	}
	return true;
}

FRFInventoryEntry URFInventoryComponent::ConsumeFromSlot(int32 EntryIndex)
{
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return FRFInventoryEntry();
	}

	FRFInventoryEntry Consumed = Entries[EntryIndex];
	Consumed.Quantity = 1;

	Entries[EntryIndex].Quantity -= 1;
	if (Entries[EntryIndex].Quantity <= 0)
	{
		Entries.RemoveAt(EntryIndex);
		if (ActiveWeaponIndex == EntryIndex)
		{
			ActiveWeaponIndex = -1;
		}
		else if (ActiveWeaponIndex > EntryIndex)
		{
			--ActiveWeaponIndex;
		}
	}
	return Consumed;
}

int32 URFInventoryComponent::FindBestFoodIndex() const
{
	int32 Best = -1;
	float BestValue = 0.0f;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FRFInventoryEntry& Entry = Entries[Index];
		if (Entry.bIsWeapon || Entry.Item.Kind != ERFItemKind::Food)
		{
			continue;
		}
		// calories per kilogram, so a D ration (0.1 kg / 30 kcal) beats black bread.
		const float Value = Entry.Item.Calories / FMath::Max(0.05f, Entry.Item.WeightKg);
		if (Value > BestValue)
		{
			BestValue = Value;
			Best = Index;
		}
	}
	return Best;
}

int32 URFInventoryComponent::FindBestWaterIndex() const
{
	int32 Best = -1;
	float BestValue = 0.0f;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FRFInventoryEntry& Entry = Entries[Index];
		if (Entry.bIsWeapon || Entry.Item.Kind != ERFItemKind::Water)
		{
			continue;
		}
		const float Value = Entry.Item.Hydration / FMath::Max(0.05f, Entry.Item.WeightKg);
		if (Value > BestValue)
		{
			BestValue = Value;
			Best = Index;
		}
	}
	return Best;
}

int32 URFInventoryComponent::FindBestMedicalIndex() const
{
	// Priority order matters tactically: a medkit also revives, a bandage only stops
	// bleeding, sulfa prevents infection, morphine is the last resort.
	const ERFItemKind Priority[] = { ERFItemKind::Medkit, ERFItemKind::Bandage, ERFItemKind::Stim };

	for (const ERFItemKind Wanted : Priority)
	{
		const int32 Index = FindItemIndexByKind(Wanted);
		if (Index >= 0)
		{
			return Index;
		}
	}
	return -1;
}

int32 URFInventoryComponent::FindItemIndexByKind(ERFItemKind Kind) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!Entries[Index].bIsWeapon && Entries[Index].Item.Kind == Kind)
		{
			return Index;
		}
	}
	return -1;
}

float URFInventoryComponent::GetTotalCarriedWeightKg() const
{
	float Sum = 0.0f;
	for (const FRFInventoryEntry& Entry : Entries)
	{
		Sum += Entry.GetWeightKg();
	}
	return Sum;
}

float URFInventoryComponent::GetAmmoCapacityMultiplier() const
{
	for (const FRFInventoryEntry& Entry : Entries)
	{
		if (!Entry.bIsWeapon && Entry.Item.AmmoCapacityMult > 1.0f)
		{
			return Entry.Item.AmmoCapacityMult;
		}
	}
	return 1.0f;
}

float URFInventoryComponent::GetReloadSpeedMultiplier() const
{
	for (const FRFInventoryEntry& Entry : Entries)
	{
		if (!Entry.bIsWeapon && Entry.Item.ReloadSpeed > 1.0f)
		{
			return Entry.Item.ReloadSpeed;
		}
	}
	return 1.0f;
}

void URFInventoryComponent::CycleActiveSlot()
{
	// Only primary <-> secondary are cyclable; grenades and supplies are thrown/used.
	TArray<int32> WeaponIndices;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].bIsWeapon)
		{
			WeaponIndices.Add(Index);
		}
	}
	if (WeaponIndices.Num() == 0)
	{
		ActiveWeaponIndex = -1;
		return;
	}

	const int32 CurrentPosition = WeaponIndices.IndexOfByKey(ActiveWeaponIndex);
	ActiveWeaponIndex = WeaponIndices[(CurrentPosition + 1) % WeaponIndices.Num()];
}
