// Copyright RedFront1941. All Rights Reserved.

#include "Core/RFLoadoutSubsystem.h"

#include "Misc/ConfigCacheIni.h"

const TCHAR* URFLoadoutSubsystem::PresetSectionPrefix = TEXT("RedFront.Loadout.");

void URFLoadoutSubsystem::SetDefinitions(const TMap<FName, FRFWeaponDef>& InWeapons,
	const TMap<FName, FRFItemDef>& InItems,
	const TMap<FName, FRFClassDef>& InClasses)
{
	Weapons = InWeapons;
	Items = InItems;
	Classes = InClasses;
}

float URFLoadoutSubsystem::GetBaseCapacityKg(ERFFaction Faction)
{
	// classes.json weight_system: base_capacity_kg 24.0 (Soviet) / us_capacity_kg 26.0.
	return Faction == ERFFaction::US ? 26.0f : 24.0f;
}

float URFLoadoutSubsystem::ComputeLoadoutWeightKg(const TArray<FRFLoadoutEntry>& Entries) const
{
	float Total = 0.0f;
	for (const FRFLoadoutEntry& Entry : Entries)
	{
		if (Entry.bIsWeapon)
		{
			if (const FRFWeaponDef* Weapon = Weapons.Find(Entry.DefinitionId))
			{
				Total += Weapon->WeightKg;
			}
		}
		else if (const FRFItemDef* Item = Items.Find(Entry.DefinitionId))
		{
			// Water mass is live: a full 1 L canteen is 1.0 kg, an empty one 0.0 kg,
			// so the briefing counts the container at full weight.
			Total += Item->WeightKg * static_cast<float>(FMath::Max(1, Entry.Quantity));
		}
	}
	return Total;
}

ERFLoadBand URFLoadoutSubsystem::ComputeLoadBand(float TotalWeightKg, float CapacityKg)
{
	if (CapacityKg <= KINDA_SMALL_NUMBER)
	{
		return ERFLoadBand::Overloaded;
	}

	// Contract ratios: light <= 0.60, standard <= 0.85, heavy <= 1.00, else overloaded.
	const float Ratio = TotalWeightKg / CapacityKg;
	if (Ratio <= 0.6f) { return ERFLoadBand::Light; }
	if (Ratio <= 0.85f) { return ERFLoadBand::Standard; }
	if (Ratio <= 1.0f) { return ERFLoadBand::Heavy; }
	return ERFLoadBand::Overloaded;
}

FRFLoadoutValidation URFLoadoutSubsystem::ValidateLoadout(const FRFClassDef& ClassDef, const FRFLevelDef& Level,
	const TArray<FRFLoadoutEntry>& Entries) const
{
	FRFLoadoutValidation Result;

	// Slot counts: one counter per slot kind, compared against the class definition.
	TMap<ERFSlotType, int32> Used;
	for (const FRFLoadoutEntry& Entry : Entries)
	{
		Used.FindOrAdd(Entry.Slot) += 1;
	}

	for (const TPair<ERFSlotType, int32>& Pair : Used)
	{
		const int32 Allowed = ClassDef.GetSlotCount(Pair.Key);
		if (Pair.Value > Allowed)
		{
			Result.bValid = false;
			Result.Problems.Add(FText::FromString(FString::Printf(
				TEXT("Slot over capacity: %d of %d used."), Pair.Value, Allowed)));
		}
	}

	for (const FRFLoadoutEntry& Entry : Entries)
	{
		if (Entry.DefinitionId.IsNone())
		{
			continue;
		}

		if (Entry.bIsWeapon)
		{
			const FRFWeaponDef* Weapon = Weapons.Find(Entry.DefinitionId);
			if (Weapon == nullptr)
			{
				Result.bValid = false;
				Result.Problems.Add(FText::FromString(FString::Printf(
					TEXT("Unknown weapon id '%s'."), *Entry.DefinitionId.ToString())));
				continue;
			}
			// The weapon must be either a signature weapon of the class or unlocked
			// by progression (unlock_level is compared against the highest unlocked id
			// supplied by the profile subsystem; here it is a pass when level 1).
			const bool bSignature = ClassDef.SignatureWeapons.Contains(Entry.DefinitionId);
			if (!bSignature && Weapon->UnlockLevel > 1)
			{
				Result.Problems.Add(FText::FromString(FString::Printf(
					TEXT("Weapon '%s' is not a %s signature weapon (unlock level %d)."),
					*Entry.DefinitionId.ToString(), *ClassDef.Id.ToString(), Weapon->UnlockLevel)));
			}
		}
		else
		{
			const FRFItemDef* Item = Items.Find(Entry.DefinitionId);
			if (Item == nullptr)
			{
				Result.bValid = false;
				Result.Problems.Add(FText::FromString(FString::Printf(
					TEXT("Unknown item id '%s'."), *Entry.DefinitionId.ToString())));
				continue;
			}

			if (Item->Category == ERFItemCategory::Grenade && Entry.Slot != ERFSlotType::Grenade)
			{
				Result.bValid = false;
				Result.Problems.Add(FText::FromString(FString::Printf(
					TEXT("'%s' is a grenade and must occupy a grenade slot."), *Entry.DefinitionId.ToString())));
			}

			// Faction check: a Soviet soldier cannot draw US-only gear and vice versa.
			const bool bFactionOk = Item->Faction == ERFFaction::Both
				|| Level.Faction == ERFFaction::Both
				|| Item->Faction == Level.Faction;
			if (!bFactionOk)
			{
				Result.bValid = false;
				Result.Problems.Add(FText::FromString(FString::Printf(
					TEXT("'%s' belongs to another faction."), *Entry.DefinitionId.ToString())));
			}
		}
	}

	// Weight budget: the level contract overrides the faction default when set.
	Result.TotalWeightKg = ComputeLoadoutWeightKg(Entries);
	Result.WeightBudgetKg = Level.WeightBudgetKg > 0.0f
		? Level.WeightBudgetKg
		: GetBaseCapacityKg(Level.Faction);
	Result.Band = ComputeLoadBand(Result.TotalWeightKg, Result.WeightBudgetKg);

	if (Result.TotalWeightKg > Result.WeightBudgetKg)
	{
		// Over budget is a soft failure: the soldier may still deploy, but walking
		// into the mission above the budget means the overloaded band and no sprint.
		Result.bValid = false;
		Result.Problems.Add(FText::FromString(FString::Printf(
			TEXT("Over the %.1f kg budget by %.1f kg (band index %d)."),
			Result.WeightBudgetKg, Result.TotalWeightKg - Result.WeightBudgetKg,
			static_cast<int32>(Result.Band))));
	}

	return Result;
}

FString URFLoadoutSubsystem::MakeSlotKey(int32 EntryIndex)
{
	return FString::Printf(TEXT("Slot%d"), EntryIndex);
}

bool URFLoadoutSubsystem::SavePreset(FName PresetName, const TArray<FRFLoadoutEntry>& Entries)
{
	if (PresetName.IsNone() || GConfig == nullptr)
	{
		return false;
	}

	const FString Section = FString(PresetSectionPrefix) + PresetName.ToString();
	GConfig->EmptySection(*Section, GGameUserSettingsIni);

	GConfig->SetInt(*Section, TEXT("Count"), Entries.Num(), GGameUserSettingsIni);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FRFLoadoutEntry& Entry = Entries[Index];
		// One compact value per slot: "<slotInt>|<id>|<weaponFlag>|<quantity>".
		const FString Value = FString::Printf(TEXT("%d|%s|%d|%d"),
			static_cast<int32>(Entry.Slot), *Entry.DefinitionId.ToString(),
			Entry.bIsWeapon ? 1 : 0, Entry.Quantity);
		GConfig->SetString(*Section, *MakeSlotKey(Index), *Value, GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

bool URFLoadoutSubsystem::LoadPreset(FName PresetName, TArray<FRFLoadoutEntry>& OutEntries) const
{
	OutEntries.Reset();
	if (PresetName.IsNone() || GConfig == nullptr)
	{
		return false;
	}

	const FString Section = FString(PresetSectionPrefix) + PresetName.ToString();
	int32 Count = 0;
	if (!GConfig->GetInt(*Section, TEXT("Count"), Count, GGameUserSettingsIni) || Count <= 0)
	{
		return false;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FString Value;
		if (!GConfig->GetString(*Section, *MakeSlotKey(Index), Value, GGameUserSettingsIni))
		{
			continue;
		}

		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT("|"), true);
		if (Parts.Num() < 4)
		{
			continue;
		}

		FRFLoadoutEntry Entry;
		Entry.Slot = static_cast<ERFSlotType>(FCString::Atoi(*Parts[0]));
		Entry.DefinitionId = FName(*Parts[1]);
		Entry.bIsWeapon = FCString::Atoi(*Parts[2]) != 0;
		Entry.Quantity = FMath::Max(1, FCString::Atoi(*Parts[3]));
		OutEntries.Add(Entry);
	}
	return OutEntries.Num() > 0;
}

TArray<FName> URFLoadoutSubsystem::GetPresetNames() const
{
	TArray<FName> Names;
	if (GConfig == nullptr)
	{
		return Names;
	}

	TArray<FString> Sections;
	GConfig->GetSectionNames(GGameUserSettingsIni, Sections);
	const FString Prefix(PresetSectionPrefix);
	for (const FString& Section : Sections)
	{
		if (Section.StartsWith(Prefix))
		{
			Names.Add(FName(*Section.RightChop(Prefix.Len())));
		}
	}
	return Names;
}
