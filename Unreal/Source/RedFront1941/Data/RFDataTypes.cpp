// Copyright RedFront1941. All Rights Reserved.

#include "Data/RFDataTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"

namespace RFJson
{
	static const TSharedPtr<FJsonValue>* FindValue(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		if (!Obj.IsValid() || Field == nullptr)
		{
			return nullptr;
		}
		return Obj->Values.Find(Field);
	}

	const TSharedPtr<FJsonObject>* GetObject(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		static const TSharedPtr<FJsonObject> Invalid;
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Object)
		{
			return nullptr;
		}
		return &(*Value)->AsObject();
	}

	int32 ArrayCount(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Array)
		{
			return 0;
		}
		return (*Value)->AsArray().Num();
	}

	float Number(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default)
	{
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid())
		{
			return Default;
		}
		double Out = 0.0;
		return (*Value)->TryGetNumber(Out) ? static_cast<float>(Out) : Default;
	}

	int32 Integer(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Default)
	{
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid())
		{
			return Default;
		}
		double Out = 0.0;
		return (*Value)->TryGetNumber(Out) ? FMath::RoundToInt(Out) : Default;
	}

	FString String(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default)
	{
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type == EJson::Null)
		{
			return Default;
		}
		return (*Value)->AsString();
	}

	FName Name(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const FString Raw = String(Obj, Field);
		return Raw.IsEmpty() ? NAME_None : FName(*Raw);
	}

	FText Text(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const FString Raw = String(Obj, Field);
		return Raw.IsEmpty() ? FText::GetEmpty() : FText::FromString(Raw);
	}

	bool Bool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Default)
	{
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid())
		{
			return Default;
		}
		bool Out = false;
		return (*Value)->TryGetBool(Out) ? Out : Default;
	}

	TArray<FName> NameArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TArray<FName> Result;
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Array)
		{
			return Result;
		}
		for (const TSharedPtr<FJsonValue>& Entry : (*Value)->AsArray())
		{
			if (Entry.IsValid() && Entry->Type != EJson::Null)
			{
				Result.Add(FName(*Entry->AsString()));
			}
		}
		return Result;
	}

	TArray<FText> TextArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TArray<FText> Result;
		const TSharedPtr<FJsonValue>* Value = FindValue(Obj, Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Array)
		{
			return Result;
		}
		for (const TSharedPtr<FJsonValue>& Entry : (*Value)->AsArray())
		{
			if (Entry.IsValid() && Entry->Type != EJson::Null)
			{
				Result.Add(FText::FromString(Entry->AsString()));
			}
		}
		return Result;
	}

	TMap<FName, int32> IntMap(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TMap<FName, int32> Result;
		const TSharedPtr<FJsonObject>* Sub = GetObject(Obj, Field);
		if (Sub == nullptr || !Sub->IsValid())
		{
			return Result;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Sub)->Values)
		{
			double Raw = 0.0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Raw))
			{
				Result.Add(FName(*Pair.Key), FMath::RoundToInt(Raw));
			}
		}
		return Result;
	}
}

namespace
{
	ERFFaction ParseFaction(const FString& Raw)
	{
		if (Raw == TEXT("soviet")) { return ERFFaction::Soviet; }
		if (Raw == TEXT("us")) { return ERFFaction::US; }
		if (Raw == TEXT("germany")) { return ERFFaction::Germany; }
		if (Raw == TEXT("axis_aux")) { return ERFFaction::AxisAux; }
		if (Raw == TEXT("captured")) { return ERFFaction::Captured; }
		if (Raw == TEXT("both")) { return ERFFaction::Both; }
		return ERFFaction::Neutral;
	}

	ERFWeaponCategory ParseWeaponCategory(const FString& Raw)
	{
		if (Raw == TEXT("rifle_bolt")) { return ERFWeaponCategory::RifleBolt; }
		if (Raw == TEXT("rifle_semi")) { return ERFWeaponCategory::RifleSemi; }
		if (Raw == TEXT("carbine")) { return ERFWeaponCategory::Carbine; }
		if (Raw == TEXT("assault_rifle")) { return ERFWeaponCategory::AssaultRifle; }
		if (Raw == TEXT("smg")) { return ERFWeaponCategory::SMG; }
		if (Raw == TEXT("lmg")) { return ERFWeaponCategory::LMG; }
		if (Raw == TEXT("hmg_emplacement")) { return ERFWeaponCategory::HMGEmplacement; }
		if (Raw == TEXT("sniper")) { return ERFWeaponCategory::Sniper; }
		if (Raw == TEXT("shotgun")) { return ERFWeaponCategory::Shotgun; }
		if (Raw == TEXT("sidearm")) { return ERFWeaponCategory::Sidearm; }
		if (Raw == TEXT("at_rifle")) { return ERFWeaponCategory::ATRifle; }
		if (Raw == TEXT("at_rocket")) { return ERFWeaponCategory::ATRocket; }
		if (Raw == TEXT("at_grenade_thrower")) { return ERFWeaponCategory::ATGrenadeThrower; }
		if (Raw == TEXT("flamethrower")) { return ERFWeaponCategory::Flamethrower; }
		if (Raw == TEXT("demolition")) { return ERFWeaponCategory::Demolition; }
		if (Raw == TEXT("mortar")) { return ERFWeaponCategory::Mortar; }
		return ERFWeaponCategory::Unknown;
	}

	ERFItemCategory ParseItemCategory(const FString& Raw)
	{
		if (Raw == TEXT("grenade")) { return ERFItemCategory::Grenade; }
		if (Raw == TEXT("food")) { return ERFItemCategory::Food; }
		if (Raw == TEXT("water")) { return ERFItemCategory::Water; }
		if (Raw == TEXT("medical")) { return ERFItemCategory::Medical; }
		if (Raw == TEXT("tool")) { return ERFItemCategory::Tool; }
		if (Raw == TEXT("armor")) { return ERFItemCategory::Armor; }
		if (Raw == TEXT("ammo")) { return ERFItemCategory::Ammo; }
		return ERFItemCategory::Unknown;
	}

	ERFSupportCallType ParseCallType(const FString& Raw)
	{
		if (Raw == TEXT("indirect")) { return ERFSupportCallType::Indirect; }
		if (Raw == TEXT("area_saturation")) { return ERFSupportCallType::AreaSaturation; }
		if (Raw == TEXT("obscurant")) { return ERFSupportCallType::Obscurant; }
		if (Raw == TEXT("air")) { return ERFSupportCallType::Air; }
		if (Raw == TEXT("armor")) { return ERFSupportCallType::Armor; }
		if (Raw == TEXT("armor_siege")) { return ERFSupportCallType::ArmorSiege; }
		if (Raw == TEXT("infantry")) { return ERFSupportCallType::Infantry; }
		return ERFSupportCallType::Indirect;
	}

	ERFMissionType ParseMissionType(const FString& Raw)
	{
		if (Raw == TEXT("defense")) { return ERFMissionType::Defense; }
		if (Raw == TEXT("delay")) { return ERFMissionType::Delay; }
		if (Raw == TEXT("breakthrough")) { return ERFMissionType::Breakthrough; }
		if (Raw == TEXT("assault")) { return ERFMissionType::Assault; }
		if (Raw == TEXT("urban_clearing")) { return ERFMissionType::UrbanClearing; }
		if (Raw == TEXT("river_crossing")) { return ERFMissionType::RiverCrossing; }
		if (Raw == TEXT("armored")) { return ERFMissionType::Armored; }
		if (Raw == TEXT("ambush")) { return ERFMissionType::Ambush; }
		if (Raw == TEXT("relief")) { return ERFMissionType::Relief; }
		if (Raw == TEXT("sabotage")) { return ERFMissionType::Sabotage; }
		if (Raw == TEXT("siege")) { return ERFMissionType::Siege; }
		if (Raw == TEXT("amphibious")) { return ERFMissionType::Amphibious; }
		return ERFMissionType::Defense;
	}

	ERFTerrain ParseTerrain(const FString& Raw)
	{
		if (Raw == TEXT("city")) { return ERFTerrain::City; }
		if (Raw == TEXT("forest")) { return ERFTerrain::Forest; }
		if (Raw == TEXT("field")) { return ERFTerrain::Field; }
		if (Raw == TEXT("village")) { return ERFTerrain::Village; }
		if (Raw == TEXT("river")) { return ERFTerrain::River; }
		if (Raw == TEXT("rail")) { return ERFTerrain::Rail; }
		if (Raw == TEXT("industrial")) { return ERFTerrain::Industrial; }
		if (Raw == TEXT("fortress")) { return ERFTerrain::Fortress; }
		if (Raw == TEXT("trench")) { return ERFTerrain::Trench; }
		return ERFTerrain::Field;
	}

	ERFWeather ParseWeather(const FString& Raw)
	{
		if (Raw == TEXT("clear")) { return ERFWeather::Clear; }
		if (Raw == TEXT("rain")) { return ERFWeather::Rain; }
		if (Raw == TEXT("snow")) { return ERFWeather::Snow; }
		if (Raw == TEXT("fog")) { return ERFWeather::Fog; }
		if (Raw == TEXT("mud")) { return ERFWeather::Mud; }
		if (Raw == TEXT("storm")) { return ERFWeather::Storm; }
		return ERFWeather::Clear;
	}

	ERFTimeOfDay ParseTimeOfDay(const FString& Raw)
	{
		if (Raw == TEXT("dawn")) { return ERFTimeOfDay::Dawn; }
		if (Raw == TEXT("day")) { return ERFTimeOfDay::Day; }
		if (Raw == TEXT("dusk")) { return ERFTimeOfDay::Dusk; }
		if (Raw == TEXT("night")) { return ERFTimeOfDay::Night; }
		return ERFTimeOfDay::Day;
	}

	ERFSlotType ParseSlotType(const FString& Raw)
	{
		if (Raw == TEXT("primary")) { return ERFSlotType::Primary; }
		if (Raw == TEXT("secondary")) { return ERFSlotType::Secondary; }
		if (Raw == TEXT("grenade")) { return ERFSlotType::Grenade; }
		return ERFSlotType::Supply;
	}

	/**
	 * Behaviour kind from category + the presence of effect keys.
	 * Kept explicit (rather than schema-driven) so a missing key degrades to a
	 * safe generic kind instead of changing the item's runtime behaviour.
	 */
	ERFItemKind DeriveItemKind(const TSharedPtr<FJsonObject>& Effect, ERFItemCategory Category)
	{
		if (Category == ERFItemCategory::Food) { return ERFItemKind::Food; }
		if (Category == ERFItemCategory::Water) { return ERFItemKind::Water; }
		if (Category == ERFItemCategory::Tool) { return ERFItemKind::Tool; }
		if (Category == ERFItemCategory::Armor) { return ERFItemKind::Armor; }
		if (Category == ERFItemCategory::Ammo) { return ERFItemKind::Ammo; }
		if (Category == ERFItemCategory::Medical)
		{
			if (RFJson::Number(Effect, TEXT("pain_ignore_s")) > 0.0f) { return ERFItemKind::Stim; }
			if (RFJson::Bool(Effect, TEXT("revive"))) { return ERFItemKind::Medkit; }
			return ERFItemKind::Bandage;
		}
		if (Category == ERFItemCategory::Grenade)
		{
			if (RFJson::Number(Effect, TEXT("penetration_mm")) > 0.0f) { return ERFItemKind::AntiTank; }
			if (RFJson::Number(Effect, TEXT("fire_duration_s")) > 0.0f) { return ERFItemKind::Fire; }
			if (RFJson::Number(Effect, TEXT("smoke_duration_s")) > 0.0f) { return ERFItemKind::Smoke; }
			if (RFJson::Number(Effect, TEXT("structure_damage")) > 0.0f) { return ERFItemKind::Demolition; }
			return ERFItemKind::Frag;
		}
		return ERFItemKind::None;
	}
}

FRFWeightBand FRFWeightBand::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFWeightBand Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.MaxRatio = RFJson::Number(Obj, TEXT("max_ratio"), 1.0f);
	Result.SpeedMult = RFJson::Number(Obj, TEXT("speed_mult"), 1.0f);
	Result.StaminaDrain = RFJson::Number(Obj, TEXT("stamina_drain"), 1.0f);
	Result.Desc = RFJson::Text(Obj, TEXT("desc"));
	return Result;
}

int32 FRFClassDef::GetSlotCount(ERFSlotType InSlot) const
{
	for (const FRFLoadoutSlot& Slot : Slots)
	{
		if (Slot.Slot == InSlot)
		{
			return Slot.Count;
		}
	}
	return 0;
}

FRFLoadoutSlot FRFLoadoutSlot::Make(ERFSlotType InSlot, int32 InCount)
{
	FRFLoadoutSlot Result;
	Result.Slot = InSlot;
	Result.Count = FMath::Max(0, InCount);
	return Result;
}

FRFClassPassive FRFClassPassive::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFClassPassive Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.Effect = RFJson::Text(Obj, TEXT("effect"));
	return Result;
}

FRFClassActive FRFClassActive::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFClassActive Result;
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.CooldownS = RFJson::Number(Obj, TEXT("cooldown_s"), 60.0f);
	Result.Effect = RFJson::Text(Obj, TEXT("effect"));
	return Result;
}

FRFWeaponDef FRFWeaponDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFWeaponDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.CategoryRaw = RFJson::Name(Obj, TEXT("category"));
	Result.Category = ParseWeaponCategory(Result.CategoryRaw.ToString());
	Result.Caliber = RFJson::Name(Obj, TEXT("caliber"));
	Result.Damage = RFJson::Number(Obj, TEXT("damage"));
	Result.Rpm = RFJson::Number(Obj, TEXT("rpm"));
	Result.Magazine = RFJson::Integer(Obj, TEXT("magazine"));
	Result.ReloadS = RFJson::Number(Obj, TEXT("reload_s"), 3.0f);
	Result.WeightKg = RFJson::Number(Obj, TEXT("weight_kg"));
	Result.Accuracy = FMath::Clamp(RFJson::Number(Obj, TEXT("accuracy"), 0.5f), 0.0f, 1.0f);
	Result.EffectiveRangeM = RFJson::Number(Obj, TEXT("effective_range_m"), 100.0f);
	Result.PenetrationMm = RFJson::Number(Obj, TEXT("penetration_mm"));
	Result.MobilityMod = RFJson::Number(Obj, TEXT("mobility_mod"), 1.0f);
	Result.AmmoType = RFJson::Name(Obj, TEXT("ammo_type"));
	Result.UnlockLevel = RFJson::Integer(Obj, TEXT("unlock_level"), 1);
	Result.bCaptured = (Result.Faction == ERFFaction::Captured);
	Result.RealismNote = RFJson::Text(Obj, TEXT("realism"));
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));
	return Result;
}

FRFClassDef FRFClassDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFClassDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.FactionIds = RFJson::NameArray(Obj, TEXT("factions"));
	for (const FName& FactionId : Result.FactionIds)
	{
		Result.Factions.Add(ParseFaction(FactionId.ToString()));
	}
	Result.SignatureWeapons = RFJson::NameArray(Obj, TEXT("signature_weapons"));

	// classes.json stores slots as an object map { "primary": 1, "supply": 4 }.
	if (const TSharedPtr<FJsonObject>* SlotsObj = RFJson::GetObject(Obj, TEXT("slots")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*SlotsObj)->Values)
		{
			double Count = 0.0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Count))
			{
				Result.Slots.Add(FRFLoadoutSlot::Make(ParseSlotType(Pair.Key), FMath::RoundToInt(Count)));
			}
		}
	}

	if (const TSharedPtr<FJsonValue>* Passives = Obj.IsValid() ? Obj->Values.Find(TEXT("passives")) : nullptr)
	{
		if (Passives->IsValid() && (*Passives)->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Entry : (*Passives)->AsArray())
			{
				if (Entry.IsValid() && Entry->Type == EJson::Object)
				{
					Result.Passives.Add(FRFClassPassive::FromJson(Entry->AsObject()));
				}
			}
		}
	}

	if (const TSharedPtr<FJsonObject>* Active = RFJson::GetObject(Obj, TEXT("active")))
	{
		Result.Active = FRFClassActive::FromJson(*Active);
	}
	Result.UnlockLevel = RFJson::Integer(Obj, TEXT("unlock_level"), 1);
	return Result;
}

FRFItemDef FRFItemDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFItemDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.Category = ParseItemCategory(RFJson::String(Obj, TEXT("category")));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.WeightKg = RFJson::Number(Obj, TEXT("weight_kg"));
	Result.CountPerSlot = FMath::Max(1, RFJson::Integer(Obj, TEXT("count_per_slot"), 1));
	Result.Capacity = RFJson::Number(Obj, TEXT("capacity"));
	Result.RealismNote = RFJson::Text(Obj, TEXT("realism"));
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));

	const TSharedPtr<FJsonObject>* EffectPtr = RFJson::GetObject(Obj, TEXT("effect"));
	TSharedPtr<FJsonObject> Effect = EffectPtr != nullptr ? *EffectPtr : nullptr;
	Result.Kind = DeriveItemKind(Effect, Result.Category);

	Result.Damage = RFJson::Number(Effect, TEXT("damage"));
	Result.RadiusM = RFJson::Number(Effect, TEXT("radius_m"));
	Result.FuseS = RFJson::Number(Effect, TEXT("fuse_s"));
	Result.Fragments = RFJson::Integer(Effect, TEXT("fragments"));
	Result.SmokeDurationS = RFJson::Number(Effect, TEXT("smoke_duration_s"));
	Result.FireDurationS = RFJson::Number(Effect, TEXT("fire_duration_s"));
	Result.PenetrationMm = RFJson::Number(Effect, TEXT("penetration_mm"));
	Result.StructureDamage = RFJson::Number(Effect, TEXT("structure_damage"));
	Result.Calories = RFJson::Number(Effect, TEXT("calories"));
	Result.Hydration = RFJson::Number(Effect, TEXT("hydration"));
	Result.Heal = RFJson::Number(Effect, TEXT("heal"));
	Result.Morale = RFJson::Number(Effect, TEXT("morale"));
	Result.EatTimeS = RFJson::Number(Effect, TEXT("eat_time_s"));
	Result.DrinkTimeS = RFJson::Number(Effect, TEXT("drink_time_s"));
	Result.UseTimeS = RFJson::Number(Effect, TEXT("use_time_s"));
	Result.bStopBleed = RFJson::Bool(Effect, TEXT("stop_bleed"));
	Result.bRevive = RFJson::Bool(Effect, TEXT("revive"));
	Result.PainIgnoreS = RFJson::Number(Effect, TEXT("pain_ignore_s"));
	Result.MeleeDamage = RFJson::Number(Effect, TEXT("melee_damage"));
	Result.MobilityPenalty = RFJson::Number(Effect, TEXT("mobility_penalty"), 1.0f);
	Result.StaminaPenalty = RFJson::Number(Effect, TEXT("stamina_penalty"), 1.0f);
	Result.AmmoCapacityMult = RFJson::Number(Effect, TEXT("ammo_capacity_mult"), 1.0f);
	Result.ReloadSpeed = RFJson::Number(Effect, TEXT("reload_speed"), 1.0f);
	Result.ArtilleryAccuracyBonus = RFJson::Number(Effect, TEXT("artillery_accuracy_bonus"));
	Result.MarkRangeM = RFJson::Number(Effect, TEXT("mark_range_m"));
	Result.bRequiresSettle = RFJson::Bool(Effect, TEXT("requires_settle")) || RFJson::Bool(Effect, TEXT("settle_required"));
	Result.bSharable = RFJson::Bool(Effect, TEXT("sharable"));
	Result.DailyLimit = RFJson::Integer(Effect, TEXT("daily_limit"));
	Result.Debuff = RFJson::Text(Effect, TEXT("debuff"));
	// A short eat animation is the contract's signal that the item is eaten on the move
	// (equipment.json note: "唯一支持移动进食的食物").
	Result.bMobileUse = Result.EatTimeS > 0.0f && Result.EatTimeS <= 2.0f;

	// Water containers refill rather than grant a flat hydration value at pickup time.
	if (Result.Category == ERFItemCategory::Water && Result.Hydration <= 0.0f)
	{
		Result.Hydration = Result.Capacity;
	}
	return Result;
}

FRFEnemyAIProfile FRFEnemyAIProfile::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFEnemyAIProfile Result;
	Result.AwarenessM = RFJson::Number(Obj, TEXT("awareness_m"), 200.0f);
	Result.ReactionS = RFJson::Number(Obj, TEXT("reaction_s"), 1.2f);
	Result.SuppressionResist = RFJson::Number(Obj, TEXT("suppression_resist"), 1.0f);
	Result.MoraleBreakChance = FMath::Clamp(RFJson::Number(Obj, TEXT("morale_break_chance")), 0.0f, 1.0f);
	Result.bCallsSupport = RFJson::Bool(Obj, TEXT("calls_support"));
	Result.CallSupportS = RFJson::Number(Obj, TEXT("call_support_s"));
	Result.bRelocatesAfterShot = RFJson::Bool(Obj, TEXT("relocates_after_shot"));
	Result.bTargetsVehiclesFirst = RFJson::Bool(Obj, TEXT("targets_vehicles_first"));
	Result.AmbushBonus = RFJson::Number(Obj, TEXT("ambush_bonus"), 1.0f);
	Result.EngageRangeM = RFJson::Number(Obj, TEXT("engage_range_m"), 800.0f);
	Result.bEmplaced = RFJson::Bool(Obj, TEXT("emplaced"));
	Result.bTargetsAircraft = RFJson::Bool(Obj, TEXT("targets_aircraft"));
	Result.bClearsBunkers = RFJson::Bool(Obj, TEXT("clears_bunkers"));
	Result.CarriesInfantry = RFJson::Integer(Obj, TEXT("carries_infantry"));
	return Result;
}

FRFEnemyDef FRFEnemyDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFEnemyDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.Fronts = RFJson::NameArray(Obj, TEXT("fronts"));
	Result.Hp = RFJson::Number(Obj, TEXT("hp"), 100.0f);
	Result.ArmorValue = RFJson::Number(Obj, TEXT("armor_value"));
	Result.Weapon = RFJson::Name(Obj, TEXT("weapon"));
	Result.Damage = RFJson::Number(Obj, TEXT("damage"));
	Result.FireRateS = RFJson::Number(Obj, TEXT("fire_rate_s"), 1.0f);
	Result.Accuracy = FMath::Clamp(RFJson::Number(Obj, TEXT("accuracy"), 0.5f), 0.0f, 1.0f);
	Result.Role = RFJson::Name(Obj, TEXT("role"));
	Result.Threat = RFJson::Integer(Obj, TEXT("threat"), 1);
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));
	if (const TSharedPtr<FJsonObject>* AI = RFJson::GetObject(Obj, TEXT("ai")))
	{
		Result.AI = FRFEnemyAIProfile::FromJson(*AI);
	}
	return Result;
}

float FRFVehicleDef::GetArmorForAspect(const FRFVehicleDef& Def, bool bSideOrRear)
{
	// Armour is authored as frontal mm RHA; aspects are scaled by side_rear_ratio, and
	// emplaced guns (ratio 1.0) keep a single value for every aspect.
	const float Ratio = bSideOrRear ? FMath::Clamp(Def.SideRearRatio, 0.05f, 1.0f) : 1.0f;
	return Def.ArmorMm * Ratio;
}

FRFVehicleDef FRFVehicleDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFVehicleDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.Fronts = RFJson::NameArray(Obj, TEXT("fronts"));
	Result.ArmorMm = RFJson::Number(Obj, TEXT("armor_mm"));
	Result.SideRearRatio = RFJson::Number(Obj, TEXT("side_rear_ratio"), 0.5f);
	Result.Hp = RFJson::Number(Obj, TEXT("hp"), 100.0f);
	Result.Weapon = RFJson::Name(Obj, TEXT("weapon"));
	Result.PenetrationMm = RFJson::Number(Obj, TEXT("penetration_mm"));
	Result.SpeedKph = RFJson::Number(Obj, TEXT("speed_kph"));
	Result.Threat = RFJson::Integer(Obj, TEXT("threat"), 1);
	Result.FirstSeen = RFJson::Name(Obj, TEXT("first_seen"));
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));
	if (const TSharedPtr<FJsonObject>* AI = RFJson::GetObject(Obj, TEXT("ai")))
	{
		Result.AI = FRFEnemyAIProfile::FromJson(*AI);
	}
	return Result;
}

FRFAirUnitDef FRFAirUnitDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFAirUnitDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.Fronts = RFJson::NameArray(Obj, TEXT("fronts"));
	Result.Hp = RFJson::Number(Obj, TEXT("hp"), 100.0f);
	Result.Weapon = RFJson::Name(Obj, TEXT("weapon"));
	Result.Threat = RFJson::Integer(Obj, TEXT("threat"), 1);
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));
	if (const TSharedPtr<FJsonObject>* AI = RFJson::GetObject(Obj, TEXT("ai")))
	{
		Result.bStrafes = RFJson::Bool(*AI, TEXT("strafes"));
		Result.bHasSiren = RFJson::Bool(*AI, TEXT("siren"));
		Result.BombRadiusM = RFJson::Number(*AI, TEXT("bomb_radius_m"));
	}
	return Result;
}

FRFSupportCallInDef FRFSupportCallInDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFSupportCallInDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.TypeRaw = RFJson::Name(Obj, TEXT("type"));
	Result.CallType = ParseCallType(Result.TypeRaw.ToString());
	Result.CpCost = RFJson::Integer(Obj, TEXT("cp_cost"), 1);
	Result.CooldownS = RFJson::Number(Obj, TEXT("cooldown_s"));
	Result.DelayS = RFJson::Number(Obj, TEXT("delay_s"), 25.0f);
	Result.RadiusM = RFJson::Number(Obj, TEXT("radius_m"));
	Result.Rounds = RFJson::Integer(Obj, TEXT("rounds"));
	Result.DamagePerRound = RFJson::Number(Obj, TEXT("damage_per_round"));
	Result.Shell = RFJson::Name(Obj, TEXT("shell"));
	Result.SpreadBaseM = RFJson::Number(Obj, TEXT("spread_base_m"));
	Result.Requires = RFJson::NameArray(Obj, TEXT("requires"));
	Result.BestAgainst = RFJson::NameArray(Obj, TEXT("best_against"));
	Result.DurationS = RFJson::Number(Obj, TEXT("duration_s"));
	Result.Passes = RFJson::Integer(Obj, TEXT("passes"));
	Result.DamagePerPass = RFJson::Number(Obj, TEXT("damage_per_pass"));
	Result.ArmorPenetrationMm = RFJson::Number(Obj, TEXT("armor_penetration_mm"));
	Result.Units = RFJson::Integer(Obj, TEXT("units"));
	Result.ArmorMm = RFJson::Number(Obj, TEXT("armor_mm"));
	Result.HpEach = RFJson::Number(Obj, TEXT("hp_each"));
	Result.VehicleWeapon = RFJson::Name(Obj, TEXT("weapon"));
	Result.VehiclePenetrationMm = RFJson::Number(Obj, TEXT("penetration_mm"));
	Result.ArrivesFrom = RFJson::Name(Obj, TEXT("arrives_from"));
	Result.DesantSlots = RFJson::Integer(Obj, TEXT("desant_slots"));
	Result.SquadSize = RFJson::Integer(Obj, TEXT("squad_size"));
	Result.WeaponMix = RFJson::Name(Obj, TEXT("weapon_mix"));
	Result.Behaviour = RFJson::Text(Obj, TEXT("behaviour"));
	Result.RealismNote = RFJson::Text(Obj, TEXT("realism"));
	Result.DesignNote = RFJson::Text(Obj, TEXT("note"));
	return Result;
}

FRFCommandPointRules FRFCommandPointRules::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFCommandPointRules Result;
	Result.Start = RFJson::Integer(Obj, TEXT("start"), 2);
	Result.Max = RFJson::Integer(Obj, TEXT("max"), 10);
	Result.RegenPerS = RFJson::Number(Obj, TEXT("regen_per_s"), 0.035f);
	Result.ObjectiveBonus = RFJson::Integer(Obj, TEXT("objective_bonus"), 2);
	Result.KillStreakBonus = RFJson::Number(Obj, TEXT("kill_streak_bonus"), 0.2f);
	return Result;
}

FRFCommsRules FRFCommsRules::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFCommsRules Result;
	Result.bRadioRequired = RFJson::Bool(Obj, TEXT("radio_required"), true);
	Result.RadioWeightKg = RFJson::Number(Obj, TEXT("radio_weight_kg"), 2.4f);
	Result.RadioSlots = RFJson::Integer(Obj, TEXT("radio_slots"), 1);
	return Result;
}

FRFObjectiveDef FRFObjectiveDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFObjectiveDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.TextZh = RFJson::Text(Obj, TEXT("text_zh"));
	Result.Type = RFJson::Name(Obj, TEXT("type"));
	Result.bRequired = RFJson::Bool(Obj, TEXT("required"));
	Result.TimeLimitS = RFJson::Number(Obj, TEXT("time_limit_s"));
	Result.Condition = RFJson::Name(Obj, TEXT("condition"));
	Result.HoldTimeS = RFJson::Number(Obj, TEXT("hold_time_s"));
	Result.RadiusM = RFJson::Number(Obj, TEXT("radius_m"), 4.0f);
	Result.TargetCount = RFJson::Integer(Obj, TEXT("target_count"));
	Result.TargetEnemyIds = RFJson::NameArray(Obj, TEXT("target_enemy_ids"));
	return Result;
}

FRFHistoricalAccuracy FRFHistoricalAccuracy::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFHistoricalAccuracy Result;
	Result.VerifiedEvents = RFJson::TextArray(Obj, TEXT("verified_events"));
	Result.Dramatized = RFJson::TextArray(Obj, TEXT("dramatized"));
	return Result;
}

FRFUnlockReward FRFUnlockReward::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFUnlockReward Result;
	Result.Xp = RFJson::Integer(Obj, TEXT("xp"));
	Result.Weapon = RFJson::Name(Obj, TEXT("weapon"));
	Result.ClassId = RFJson::Name(Obj, TEXT("class"));
	return Result;
}

FRFEndlessSeed FRFEndlessSeed::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFEndlessSeed Result;
	Result.Era = RFJson::Integer(Obj, TEXT("era"), 1941);
	Result.WeightMult = RFJson::Number(Obj, TEXT("weight_mult"), 1.0f);
	return Result;
}

int32 FRFLevelDef::GetTotalEnemyCount() const
{
	int32 Total = 0;
	for (const TPair<FName, int32>& Pair : EnemyComposition)
	{
		Total += FMath::Max(0, Pair.Value);
	}
	return Total;
}

FRFLevelDef FRFLevelDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFLevelDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.Index = RFJson::Integer(Obj, TEXT("index"));
	Result.NumberInCampaign = RFJson::Integer(Obj, TEXT("number_in_campaign"));
	Result.CampaignId = RFJson::Name(Obj, TEXT("campaign_id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Date = RFJson::Name(Obj, TEXT("date"));
	Result.LocationZh = RFJson::Text(Obj, TEXT("location_zh"));
	Result.LocationEn = RFJson::Text(Obj, TEXT("location_en"));
	Result.Coordinates = RFJson::Name(Obj, TEXT("coordinates"));
	Result.MissionType = ParseMissionType(RFJson::String(Obj, TEXT("mission_type")));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.PlayerRoleZh = RFJson::Text(Obj, TEXT("player_role_zh"));
	Result.PlayerClassRecommended = RFJson::NameArray(Obj, TEXT("player_class_recommended"));
	Result.EnemyComposition = RFJson::IntMap(Obj, TEXT("enemy_composition"));
	Result.EnemyTacticsZh = RFJson::Text(Obj, TEXT("enemy_tactics_zh"));
	Result.FriendlyForcesZh = RFJson::Text(Obj, TEXT("friendly_forces_zh"));
	Result.Terrain = ParseTerrain(RFJson::String(Obj, TEXT("terrain")));
	Result.Weather = ParseWeather(RFJson::String(Obj, TEXT("weather")));
	Result.TimeOfDay = ParseTimeOfDay(RFJson::String(Obj, TEXT("time_of_day")));
	Result.VisibilityM = RFJson::Integer(Obj, TEXT("visibility_m"), 500);
	Result.Difficulty = RFJson::Integer(Obj, TEXT("difficulty"), 1);
	Result.ParTimeS = RFJson::Integer(Obj, TEXT("par_time_s"), 900);
	Result.WeightBudgetKg = RFJson::Number(Obj, TEXT("weight_budget_kg"), 24.0f);
	Result.ResupplyPoints = RFJson::Integer(Obj, TEXT("resupply_points"));
	Result.AvailableSupport = RFJson::NameArray(Obj, TEXT("available_support"));
	Result.VehicleAvailable = RFJson::NameArray(Obj, TEXT("vehicle_available"));
	Result.HistoricalNoteZh = RFJson::Text(Obj, TEXT("historical_note_zh"));
	Result.DesignNoteZh = RFJson::Text(Obj, TEXT("design_note_zh"));
	Result.MusicMood = RFJson::Text(Obj, TEXT("music_mood"));

	if (const TSharedPtr<FJsonValue>* Objectives = Obj.IsValid() ? Obj->Values.Find(TEXT("objectives")) : nullptr)
	{
		if (Objectives->IsValid() && (*Objectives)->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Entry : (*Objectives)->AsArray())
			{
				if (Entry.IsValid() && Entry->Type == EJson::Object)
				{
					Result.Objectives.Add(FRFObjectiveDef::FromJson(Entry->AsObject()));
				}
			}
		}
	}
	if (const TSharedPtr<FJsonObject>* Accuracy = RFJson::GetObject(Obj, TEXT("historical_accuracy")))
	{
		Result.HistoricalAccuracy = FRFHistoricalAccuracy::FromJson(*Accuracy);
	}
	if (const TSharedPtr<FJsonObject>* Reward = RFJson::GetObject(Obj, TEXT("unlock_reward")))
	{
		Result.UnlockReward = FRFUnlockReward::FromJson(*Reward);
	}
	if (const TSharedPtr<FJsonObject>* Seed = RFJson::GetObject(Obj, TEXT("endless_seed_modifier")))
	{
		Result.EndlessSeedModifier = FRFEndlessSeed::FromJson(*Seed);
	}
	return Result;
}

FRFCampaignDef FRFCampaignDef::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFCampaignDef Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.NameZh = RFJson::Text(Obj, TEXT("name_zh"));
	Result.NameEn = RFJson::Text(Obj, TEXT("name_en"));
	Result.Faction = ParseFaction(RFJson::String(Obj, TEXT("faction")));
	Result.Front = RFJson::Name(Obj, TEXT("front"));
	Result.StartDate = RFJson::Name(Obj, TEXT("start_date"));
	Result.EndDate = RFJson::Name(Obj, TEXT("end_date"));
	Result.Tone = RFJson::Text(Obj, TEXT("tone"));
	Result.PlayerArcZh = RFJson::Text(Obj, TEXT("player_arc_zh"));
	Result.MechanicsFocus = RFJson::NameArray(Obj, TEXT("mechanics_focus"));
	Result.UnlockProgression = RFJson::Text(Obj, TEXT("unlock_progression"));

	// campaigns.json stores "levels": [first, last] as a two-element array.
	const TSharedPtr<FJsonValue>* Levels = Obj.IsValid() ? Obj->Values.Find(TEXT("levels")) : nullptr;
	if (Levels != nullptr && Levels->IsValid() && (*Levels)->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Range = (*Levels)->AsArray();
		double First = 1.0;
		double Last = 1.0;
		if (Range.Num() > 0 && Range[0].IsValid()) { Range[0]->TryGetNumber(First); }
		if (Range.Num() > 1 && Range[1].IsValid()) { Range[1]->TryGetNumber(Last); }
		Result.LevelRangeStart = FMath::RoundToInt(First);
		Result.LevelRangeEnd = FMath::RoundToInt(Last);
	}
	return Result;
}

FRFDifficultyScaling FRFDifficultyScaling::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFDifficultyScaling Result;
	Result.Id = RFJson::Name(Obj, TEXT("id"));
	Result.EnemyHpMult = RFJson::Number(Obj, TEXT("enemy_hp_mult"), 1.0f);
	Result.EnemyAccuracyMult = RFJson::Number(Obj, TEXT("enemy_accuracy_mult"), 1.0f);
	Result.PlayerDamageMult = RFJson::Number(Obj, TEXT("player_damage_mult"), 1.0f);
	Result.CpRegenMult = RFJson::Number(Obj, TEXT("cp_regen_mult"), 1.0f);
	Result.SurvivalDrainMult = 1.0f;
	Result.Desc = RFJson::Text(Obj, TEXT("desc"));

	const FString Id = Result.Id.ToString();
	if (Id == TEXT("recruit")) { Result.Difficulty = ERFDifficulty::Recruit; }
	else if (Id == TEXT("regular")) { Result.Difficulty = ERFDifficulty::Regular; }
	else if (Id == TEXT("veteran")) { Result.Difficulty = ERFDifficulty::Veteran; }
	else if (Id == TEXT("historical")) { Result.Difficulty = ERFDifficulty::Historical; }

	// The contracts encode drain scaling inside the description rather than a numeric
	// field ("食物/水消耗 +20%" / "+50%"), so it is normalized here to one place.
	if (Result.Difficulty == ERFDifficulty::Veteran) { Result.SurvivalDrainMult = 1.2f; }
	else if (Result.Difficulty == ERFDifficulty::Historical) { Result.SurvivalDrainMult = 1.5f; }
	return Result;
}

FRFEndlessWave FRFEndlessWave::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	FRFEndlessWave Result;
	Result.Wave = RFJson::Integer(Obj, TEXT("wave"), 1);
	Result.Era = RFJson::Integer(Obj, TEXT("era"), 1941);
	Result.Composition = RFJson::IntMap(Obj, TEXT("composition"));
	Result.bBoss = RFJson::Bool(Obj, TEXT("boss"));
	return Result;
}
