// Copyright RedFront1941. All Rights Reserved.
//
// Runtime mirror of the shared JSON contracts in E:\GAMES\RedFront1941\data\.
// Field names deliberately match the JSON keys so RFDataTableLoader and the
// CSV importer (Tools > Import DataTable) stay 1:1 with the source of truth.
// Units are stated per field: kg, m, s, mm RHA @100m, 0-100 health scale.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RFDataTypes.generated.h"

/** Which army a definition belongs to (contracts: faction). */
UENUM(BlueprintType)
enum class ERFFaction : uint8
{
	Soviet		UMETA(DisplayName = "Soviet"),
	US			UMETA(DisplayName = "US"),
	Germany		UMETA(DisplayName = "Germany"),
	AxisAux		UMETA(DisplayName = "Axis Auxiliary"),
	Captured	UMETA(DisplayName = "Captured"),
	Both		UMETA(DisplayName = "Both"),
	Neutral		UMETA(DisplayName = "Neutral")
};

/** Load banding from classes.json weight_system.load_bands. Serialized by id string. */
UENUM(BlueprintType)
enum class ERFLoadBand : uint8
{
	/** ratio <= 0.60: speed x1.08, stamina drain x0.80. */
	Light		UMETA(DisplayName = "Light"),
	/** ratio <= 0.85: speed x1.00, stamina drain x1.00. */
	Standard	UMETA(DisplayName = "Standard"),
	/** ratio <= 1.00: speed x0.92, stamina drain x1.25. */
	Heavy		UMETA(DisplayName = "Heavy"),
	/** ratio above 1.00: speed x0.78, stamina drain x1.70, sprint disabled. */
	Overloaded	UMETA(DisplayName = "Overloaded")
};

/** Mission archetype from data/levels/SCHEMA.md. */
UENUM(BlueprintType)
enum class ERFMissionType : uint8
{
	Defense			UMETA(DisplayName = "Defense"),
	Delay			UMETA(DisplayName = "Delay"),
	Breakthrough	UMETA(DisplayName = "Breakthrough"),
	Assault			UMETA(DisplayName = "Assault"),
	UrbanClearing	UMETA(DisplayName = "Urban Clearing"),
	RiverCrossing	UMETA(DisplayName = "River Crossing"),
	Armored			UMETA(DisplayName = "Armored"),
	Ambush			UMETA(DisplayName = "Ambush"),
	Relief			UMETA(DisplayName = "Relief"),
	Sabotage		UMETA(DisplayName = "Sabotage"),
	Siege			UMETA(DisplayName = "Siege"),
	Amphibious		UMETA(DisplayName = "Amphibious")
};

/** Weather state; drives visibility, artillery spread and air-support availability. */
UENUM(BlueprintType)
enum class ERFWeather : uint8
{
	Clear	UMETA(DisplayName = "Clear"),
	Rain	UMETA(DisplayName = "Rain"),
	Snow	UMETA(DisplayName = "Snow"),
	Fog		UMETA(DisplayName = "Fog"),
	Mud		UMETA(DisplayName = "Mud"),
	Storm	UMETA(DisplayName = "Storm")
};

/** Time of day; night forces signal flares for support call-ins. */
UENUM(BlueprintType)
enum class ERFTimeOfDay : uint8
{
	Dawn	UMETA(DisplayName = "Dawn"),
	Day		UMETA(DisplayName = "Day"),
	Dusk	UMETA(DisplayName = "Dusk"),
	Night	UMETA(DisplayName = "Night")
};

/** Ground/terrain archetype used by the 2D level layouts. */
UENUM(BlueprintType)
enum class ERFTerrain : uint8
{
	City		UMETA(DisplayName = "City"),
	Forest		UMETA(DisplayName = "Forest"),
	Field		UMETA(DisplayName = "Field"),
	Village		UMETA(DisplayName = "Village"),
	River		UMETA(DisplayName = "River"),
	Rail		UMETA(DisplayName = "Rail"),
	Industrial	UMETA(DisplayName = "Industrial"),
	Fortress	UMETA(DisplayName = "Fortress"),
	Trench		UMETA(DisplayName = "Trench")
};

/** Inventory slot kinds from classes.json "slots". */
UENUM(BlueprintType)
enum class ERFSlotType : uint8
{
	Primary		UMETA(DisplayName = "Primary"),
	Secondary	UMETA(DisplayName = "Secondary"),
	Grenade		UMETA(DisplayName = "Grenade"),
	Supply		UMETA(DisplayName = "Supply")
};

/** Item categories from equipment.json "categories". */
UENUM(BlueprintType)
enum class ERFItemCategory : uint8
{
	Grenade		UMETA(DisplayName = "Grenade"),
	Food		UMETA(DisplayName = "Food"),
	Water		UMETA(DisplayName = "Water"),
	Medical		UMETA(DisplayName = "Medical"),
	Tool		UMETA(DisplayName = "Tool"),
	Armor		UMETA(DisplayName = "Armor"),
	Ammo		UMETA(DisplayName = "Ammo"),
	Unknown		UMETA(DisplayName = "Unknown")
};

/** Item behaviour category, derived from equipment.json category + effect keys. */
UENUM(BlueprintType)
enum class ERFItemKind : uint8
{
	None		UMETA(DisplayName = "None"),
	Frag		UMETA(DisplayName = "Fragmentation"),
	Smoke		UMETA(DisplayName = "Smoke"),
	Fire		UMETA(DisplayName = "Incendiary"),
	AntiTank	UMETA(DisplayName = "Anti-Tank"),
	Demolition	UMETA(DisplayName = "Demolition"),
	Food		UMETA(DisplayName = "Food"),
	Water		UMETA(DisplayName = "Water"),
	Bandage		UMETA(DisplayName = "Bandage"),
	Medkit		UMETA(DisplayName = "Medkit"),
	Stim		UMETA(DisplayName = "Stimulant"),
	Tool		UMETA(DisplayName = "Tool"),
	Armor		UMETA(DisplayName = "Armor"),
	Ammo		UMETA(DisplayName = "Ammo")
};

/** Primary/secondary weapon classification (weapons.json "category"). */
UENUM(BlueprintType)
enum class ERFWeaponCategory : uint8
{
	RifleBolt		UMETA(DisplayName = "Bolt-Action Rifle"),
	RifleSemi		UMETA(DisplayName = "Semi-Auto Rifle"),
	Carbine			UMETA(DisplayName = "Carbine"),
	AssaultRifle	UMETA(DisplayName = "Assault Rifle"),
	SMG				UMETA(DisplayName = "SMG"),
	LMG				UMETA(DisplayName = "LMG"),
	HMGEmplacement	UMETA(DisplayName = "HMG Emplacement"),
	Sniper			UMETA(DisplayName = "Sniper"),
	Shotgun			UMETA(DisplayName = "Shotgun"),
	Sidearm			UMETA(DisplayName = "Sidearm"),
	ATRifle			UMETA(DisplayName = "AT Rifle"),
	ATRocket		UMETA(DisplayName = "AT Rocket"),
	ATGrenadeThrower UMETA(DisplayName = "AT Grenade Thrower"),
	Flamethrower	UMETA(DisplayName = "Flamethrower"),
	Demolition		UMETA(DisplayName = "Demolition"),
	Mortar			UMETA(DisplayName = "Mortar"),
	Unknown			UMETA(DisplayName = "Unknown")
};

/** Selector fire modes used by RFWeaponBase. */
UENUM(BlueprintType)
enum class ERFFireMode : uint8
{
	Safe		UMETA(DisplayName = "Safe"),
	Single		UMETA(DisplayName = "Single"),
	Burst		UMETA(DisplayName = "Burst"),
	FullAuto	UMETA(DisplayName = "Full Auto"),
	BoltAction	UMETA(DisplayName = "Bolt Action"),
	Pump		UMETA(DisplayName = "Pump")
};

/** Support call-in archetype (support.json "type"). */
UENUM(BlueprintType)
enum class ERFSupportCallType : uint8
{
	Indirect			UMETA(DisplayName = "Indirect Fire"),
	AreaSaturation		UMETA(DisplayName = "Area Saturation"),
	Obscurant			UMETA(DisplayName = "Smoke / Obscurant"),
	Air					UMETA(DisplayName = "Air"),
	Armor				UMETA(DisplayName = "Armor"),
	ArmorSiege			UMETA(DisplayName = "Armor Siege"),
	Infantry			UMETA(DisplayName = "Infantry")
};

/** Lifecycle of a queued support mission. */
UENUM(BlueprintType)
enum class ERFSupportState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Queued			UMETA(DisplayName = "Queued (comms delay)"),
	Inbound			UMETA(DisplayName = "Inbound"),
	Firing			UMETA(DisplayName = "Firing"),
	Complete		UMETA(DisplayName = "Complete"),
	Aborted			UMETA(DisplayName = "Aborted")
};

/** Player-facing difficulty tier (campaigns.json difficulty_scaling). */
UENUM(BlueprintType)
enum class ERFDifficulty : uint8
{
	Recruit		UMETA(DisplayName = "Recruit"),
	Regular		UMETA(DisplayName = "Regular"),
	Veteran		UMETA(DisplayName = "Veteran"),
	Historical	UMETA(DisplayName = "Historical")
};

/** 8-direction sprite facing baked into every character/vehicle flipbook. */
UENUM(BlueprintType)
enum class ERFCharacterFacing : uint8
{
	South		UMETA(DisplayName = "South (screen down)"),
	SouthWest	UMETA(DisplayName = "South-West"),
	West		UMETA(DisplayName = "West (screen left)"),
	NorthWest	UMETA(DisplayName = "North-West"),
	North		UMETA(DisplayName = "North (screen up)"),
	NorthEast	UMETA(DisplayName = "North-East"),
	East		UMETA(DisplayName = "East (screen right)"),
	SouthEast	UMETA(DisplayName = "South-East")
};

/**
 * Weight band entry from classes.json weight_system.load_bands.
 * max_ratio is a fraction of base_capacity_kg (dimensionless).
 */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFWeightBand
{
	GENERATED_BODY()

	/** Band id as written in JSON: light / standard / heavy / overloaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	FName Id = NAME_None;

	/** Bag ratio (carried kg / capacity kg) up to which this band applies. Dimensionless. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	float MaxRatio = 1.0f;

	/** Movement speed multiplier applied in this band (1.0 = baseline). Dimensionless. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	float SpeedMult = 1.0f;

	/** Stamina drain multiplier applied in this band (1.0 = baseline). Dimensionless. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	float StaminaDrain = 1.0f;

	/** Localized design description (zh) straight from the contract. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weight")
	FText Desc;

	/** True when stamina drain is high enough that sprint must be locked out. */
	bool IsSprintLocked() const { return StaminaDrain >= 1.5f; }

	/** Builds one band from a load_bands entry. */
	static FRFWeightBand FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One weapon row from data/weapons.json. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFWeaponDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable contract id, e.g. "mosin_m1891_30". Also the DataTable row name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FName Id = NAME_None;

	/** Chinese display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FText NameZh;

	/** English display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FText NameEn;

	/** Owning faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	ERFFaction Faction = ERFFaction::Neutral;

	/** Raw category string, kept verbatim for tooling and CSV round-trips. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FName CategoryRaw = NAME_None;

	/** Parsed category enum used by gameplay code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	ERFWeaponCategory Category = ERFWeaponCategory::Unknown;

	/** Chambering, informational only (e.g. "7.62x54mmR"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FName Caliber = NAME_None;

	/** Base damage per round against unarmoured personnel, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float Damage = 0.0f;

	/** Cyclic rate of fire in rounds per minute; 0 means single-shot/melee only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float Rpm = 0.0f;

	/** Rounds per magazine/belt/drum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	int32 Magazine = 0;

	/** Reload time in seconds (full magazine change). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float ReloadS = 3.0f;

	/** Carried weight in kilograms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float WeightKg = 0.0f;

	/** Abstract accuracy 0-1 (higher is tighter); converted to a cone in RFWeaponBase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float Accuracy = 0.5f;

	/** Range in metres at which full damage still applies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float EffectiveRangeM = 100.0f;

	/** Armour penetration in mm RHA at 100 m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float PenetrationMm = 0.0f;

	/** Movement speed multiplier while this weapon is carried (1.0 = baseline). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	float MobilityMod = 1.0f;

	/** Ammo pool id shared with resupply logic (e.g. "rifle_762x54"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FName AmmoType = NAME_None;

	/** Progression level at which the weapon unlocks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	int32 UnlockLevel = 1;

	/** True when the weapon was captured from the enemy (scarce ammo, higher resupply weight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	bool bCaptured = false;

	/** Historical/realism blurb (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FText RealismNote;

	/** Gameplay note (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Weapon")
	FText DesignNote;

	/** Seconds between rounds for the cyclic rate; 0 when the weapon has no auto fire. */
	float GetShotIntervalS() const { return Rpm > KINDA_SMALL_NUMBER ? 60.0f / Rpm : 0.0f; }

	/** Parses one entry of weapons.json "weapons". */
	static FRFWeaponDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One inventory slot rule from classes.json "slots" (e.g. primary:1, supply:4). */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFLoadoutSlot
{
	GENERATED_BODY()

	/** Slot kind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Loadout")
	ERFSlotType Slot = ERFSlotType::Primary;

	/** Number of physical slots of this kind granted by the class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Loadout")
	int32 Count = 0;

	/** Builds a slot rule directly (the contract stores slots as an object map). */
	static FRFLoadoutSlot Make(ERFSlotType InSlot, int32 InCount);
};

/** A class passive ability (classes.json passives[]). Effects are authored data, not code yet. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFClassPassive
{
	GENERATED_BODY()

	/** Passive id, e.g. "steady_aim". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FName Id = NAME_None;

	/** Localized name (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText NameZh;

	/** Localized effect description (zh); gameplay wiring happens per-passive in Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText Effect;

	static FRFClassPassive FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Active ability of a class (classes.json classes[].active). */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFClassActive
{
	GENERATED_BODY()

	/** Localized name (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText NameZh;

	/** Cooldown in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	float CooldownS = 60.0f;

	/** Localized effect description (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText Effect;

	static FRFClassActive FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One player class/kit row from data/classes.json. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFClassDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id, e.g. "machine_gunner". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FName Id = NAME_None;

	/** Chinese display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText NameZh;

	/** English display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FText NameEn;

	/** Factions allowed to pick this class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	TArray<ERFFaction> Factions;

	/** Raw faction strings as authored (kept for CSV fidelity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	TArray<FName> FactionIds;

	/** Signature weapon ids from weapons.json that the class is expected to field. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	TArray<FName> SignatureWeapons;

	/** Slot capacity rules (primary/secondary/grenade/supply). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	TArray<FRFLoadoutSlot> Slots;

	/** Three passive abilities. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	TArray<FRFClassPassive> Passives;

	/** Single active ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	FRFClassActive Active;

	/** Progression level at which the class unlocks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Class")
	int32 UnlockLevel = 1;

	/** Number of slots of the requested kind, 0 when the class does not grant any. */
	int32 GetSlotCount(ERFSlotType InSlot) const;

	/** Parses one entry of classes.json "classes". */
	static FRFClassDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One row of equipment.json "items" (grenade, food, water, medical, tool, armor, ammo). */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFItemDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id, e.g. "black_bread". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FName Id = NAME_None;

	/** Raw category key from equipment.json categories. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	ERFItemCategory Category = ERFItemCategory::Unknown;

	/** Behaviour kind derived from the effect keys (frag/smoke/food/water/...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	ERFItemKind Kind = ERFItemKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	ERFFaction Faction = ERFFaction::Neutral;

	/** Weight of one unit in kilograms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float WeightKg = 0.0f;

	/** Units granted per inventory slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	int32 CountPerSlot = 1;

	/** Container capacity for water items (contract "capacity", in hydration points). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Capacity = 0.0f;

	/** Explosive/incendiary damage, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Damage = 0.0f;

	/** Effect radius in metres (fragmentation, smoke, fire, splash). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float RadiusM = 0.0f;

	/** Fuse time in seconds; 0 means impact/direct activation (molotov). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float FuseS = 0.0f;

	/** Number of fragments used by the fragmentation falloff model. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	int32 Fragments = 0;

	/** Smoke cloud lifetime in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float SmokeDurationS = 0.0f;

	/** Burning-ground lifetime in seconds for incendiary items. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float FireDurationS = 0.0f;

	/** HEAT penetration in mm RHA for AT grenades and mines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float PenetrationMm = 0.0f;

	/** Damage scale against fortifications (contract "structure_damage"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float StructureDamage = 0.0f;

	/** Satiety points restored by food (contract "calories"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Calories = 0.0f;

	/** Hydration points restored by drink; -1 means "refill the container instead". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Hydration = 0.0f;

	/** Health restored by medical items, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Heal = 0.0f;

	/** Morale delta applied on use (volodka ration is +40, at an accuracy cost). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float Morale = 0.0f;

	/** Seconds of uninterrupted animation required to eat this item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float EatTimeS = 0.0f;

	/** Seconds of uninterrupted animation required to drink this item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float DrinkTimeS = 0.0f;

	/** Seconds of uninterrupted animation required to apply this item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float UseTimeS = 0.0f;

	/** True when the item stops bleeding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	bool bStopBleed = false;

	/** True when the item can revive a downed squadmate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	bool bRevive = false;

	/** Duration in seconds of pain-ignore (morphine). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float PainIgnoreS = 0.0f;

	/** Melee damage when the item doubles as a weapon (entrenching tool). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float MeleeDamage = 0.0f;

	/** Movement speed multiplier penalty while worn (SN-42 breastplate 0.93). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float MobilityPenalty = 1.0f;

	/** Stamina drain multiplier while worn (SN-42 breastplate 1.20). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float StaminaPenalty = 1.0f;

	/** Multiplier applied to ammo carry capacity (ammo pouch 1.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float AmmoCapacityMult = 1.0f;

	/** Reload speed multiplier while carried (ammo pouch 1.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float ReloadSpeed = 1.0f;

	/** Artillery accuracy bonus granted when used as an observation device (binoculars 0.35). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float ArtilleryAccuracyBonus = 0.0f;

	/** Marking range in metres for optics/binoculars. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	float MarkRangeM = 0.0f;

	/** True when the item must be put down before use (water can, ammo box). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	bool bRequiresSettle = false;

	/** True when the item can be shared with the squad. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	bool bSharable = false;

	/** True when the item can be eaten while moving (D ration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	bool bMobileUse = false;

	/** Per-level use limit; 0 = unlimited (vodka is 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	int32 DailyLimit = 0;

	/** Raw realism blurb (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FText RealismNote;

	/** Raw gameplay note (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FText DesignNote;

	/** Raw debuff string when the contract defines one (vodka, morphine). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Item")
	FText Debuff;

	/** Parses one entry of equipment.json "items". */
	static FRFItemDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** AI behaviour block shared by infantry and vehicles (enemies.json "ai"). */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFEnemyAIProfile
{
	GENERATED_BODY()

	/** Awareness radius in metres at which the AI can first notice the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float AwarenessM = 200.0f;

	/** Reaction time in seconds between noticing and the first aimed shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float ReactionS = 1.2f;

	/** Suppression resistance multiplier; 1.0 is baseline, higher means harder to pin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float SuppressionResist = 1.0f;

	/** Probability 0-1 that the unit breaks morale and retreats when suppressed/casualty-hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float MoraleBreakChance = 0.0f;

	/** True when the unit can request its own support fire mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bCallsSupport = false;

	/** Seconds of contact before the unit calls support (only when bCallsSupport). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float CallSupportS = 0.0f;

	/** True when the sniper relocates to a new firing position after every shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bRelocatesAfterShot = false;

	/** True when the unit prefers vehicles over infantry targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bTargetsVehiclesFirst = false;

	/** Multiplier applied to damage when the unit fires from ambush (Fallschirmjager 1.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float AmbushBonus = 1.0f;

	/** Vehicle engagement envelope in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	float EngageRangeM = 800.0f;

	/** True when the unit is emplaced and cannot move (Pak 40, Flak 36). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bEmplaced = false;

	/** True when the unit will engage aircraft (Flak 36). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bTargetsAircraft = false;

	/** True when the unit clears bunkers/tunnels (Pionier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	bool bClearsBunkers = false;

	/** Passenger capacity for half-tracks (Sd.Kfz.251 carries 6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|AI")
	int32 CarriesInfantry = 0;

	static FRFEnemyAIProfile FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One infantry row from data/enemies.json "infantry". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFEnemyDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id, e.g. "ger_mg42_team". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	ERFFaction Faction = ERFFaction::Germany;

	/** Fronts the unit appears on: "east" / "west". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	TArray<FName> Fronts;

	/** Hit points, 0-100+ health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	float Hp = 100.0f;

	/** Flat armour value in mm RHA (body armour / plate). 0 for soft targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	float ArmorValue = 0.0f;

	/** Weapon id used by the unit (may be a compound contract token such as "stg44_or_kar98k"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FName Weapon = NAME_None;

	/** Damage per shot against the player, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	float Damage = 0.0f;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	float FireRateS = 1.0f;

	/** Abstract accuracy 0-1 used to size the AI firing cone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	float Accuracy = 0.5f;

	/** Behaviour profile from the "ai" block. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FRFEnemyAIProfile AI;

	/** Tactical role token: line / leader / support / sniper / anti_armor / assault / militia / elite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FName Role = NAME_None;

	/** Threat rating 1-6 used by the squad coordinator to allocate fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	int32 Threat = 1;

	/** Designer note (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Enemy")
	FText DesignNote;

	/** Air / soft-target classification helper for AI target selection. */
	bool IsVehicleThreat() const { return Role == FName(TEXT("anti_armor")); }

	static FRFEnemyDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One vehicle or emplacement row from data/enemies.json "vehicles". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFVehicleDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id, e.g. "pzkpfw_iv_f2". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	ERFFaction Faction = ERFFaction::Germany;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TArray<FName> Fronts;

	/** Frontal effective armour thickness in mm RHA. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	float ArmorMm = 0.0f;

	/** Side/rear armour ratio applied to ArmorMm (contract side_rear_ratio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	float SideRearRatio = 0.5f;

	/** Structural hit points, 0-100+ scale (tanks reach 520). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	float Hp = 100.0f;

	/** Main armament name (informational). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FName Weapon = NAME_None;

	/** Main armament penetration in mm RHA at 100 m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	float PenetrationMm = 0.0f;

	/** Road speed in km/h; 0 for towed or emplaced guns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	float SpeedKph = 0.0f;

	/** Behaviour profile (engage range, reaction, emplaced flag, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FRFEnemyAIProfile AI;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	int32 Threat = 1;

	/** First month the vehicle can appear, "YYYY-MM"; drives campaign gating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FName FirstSeen = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	FText DesignNote;

	/** Armour in mm RHA for the given impact aspect; side/rear scaled by SideRearRatio. */
	static float GetArmorForAspect(const FRFVehicleDef& Def, bool bSideOrRear);

	static FRFVehicleDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One air unit row from data/enemies.json "air" (AI air support / enemy air). */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFAirUnitDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	ERFFaction Faction = ERFFaction::Germany;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	TArray<FName> Fronts;

	/** Structural hit points, 0-100+ scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	float Hp = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	FName Weapon = NAME_None;

	/** Bomb/rocket blast radius in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	float BombRadiusM = 0.0f;

	/** True when the aircraft strafes ground targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	bool bStrafes = false;

	/** True when the aircraft's siren suppresses unshielded infantry (Stuka). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	bool bHasSiren = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	int32 Threat = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	FText DesignNote;

	static FRFAirUnitDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One support call-in row from data/support.json "call_ins". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFSupportCallInDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id, e.g. "sov_howitzer_122". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	ERFFaction Faction = ERFFaction::Soviet;

	/** Raw type token from the contract. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName TypeRaw = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	ERFSupportCallType CallType = ERFSupportCallType::Indirect;

	/** Command point cost paid on request. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 CpCost = 1;

	/** Cooldown in seconds before the same call-in can be requested again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float CooldownS = 0.0f;

	/** Comms delay in seconds between confirmation and the first effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float DelayS = 25.0f;

	/** Effect radius in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float RadiusM = 0.0f;

	/** Number of shells/rockets in the mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 Rounds = 0;

	/** Damage per shell, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float DamagePerRound = 0.0f;

	/** Ammunition designation (informational). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName Shell = NAME_None;

	/** Base dispersion radius in metres before observer/binocular corrections. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float SpreadBaseM = 0.0f;

	/** Requirements tokens, e.g. "radio", "observer_line_of_sight", "marked_target". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	TArray<FName> Requires;

	/** Target classes this mission is best against. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	TArray<FName> BestAgainst;

	/** Smoke/obscurant duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float DurationS = 0.0f;

	/** Aircraft passes over the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 Passes = 0;

	/** Damage per aircraft pass, 0-100 health scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float DamagePerPass = 0.0f;

	/** Air weapon penetration in mm RHA. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float ArmorPenetrationMm = 0.0f;

	/** Number of vehicles delivered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 Units = 0;

	/** Armour of the delivered vehicles in mm RHA. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float ArmorMm = 0.0f;

	/** Hit points of each delivered vehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float HpEach = 0.0f;

	/** Main armament of the delivered vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName VehicleWeapon = NAME_None;

	/** Penetration of the delivered vehicles' guns in mm RHA. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float VehiclePenetrationMm = 0.0f;

	/** Entry road token; vehicles path in along roads ("rear_road"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName ArrivesFrom = NAME_None;

	/** Tank-desant seats available on the delivered vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 DesantSlots = 0;

	/** Number of squads delivered for infantry call-ins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 SquadSize = 0;

	/** Weapon mix description for infantry call-ins (informational). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FName WeaponMix = NAME_None;

	/** Scripted behaviour description for infantry call-ins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FText Behaviour;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FText RealismNote;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	FText DesignNote;

	/** True when the mission needs an active radio in the squad. */
	bool RequiresRadio() const { return Requires.Contains(FName(TEXT("radio"))); }

	/** True when the mission needs line of sight from an observer. */
	bool RequiresObserverLOS() const { return Requires.Contains(FName(TEXT("observer_line_of_sight"))); }

	/** True when the mission needs a marked target (night/smoke flare or designator). */
	bool RequiresMarkedTarget() const { return Requires.Contains(FName(TEXT("marked_target"))); }

	static FRFSupportCallInDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Command point economy from support.json "command_points". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFCommandPointRules
{
	GENERATED_BODY()

	/** CP available at mission start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 Start = 2;

	/** Hard CP ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 Max = 10;

	/** Passive CP regeneration per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float RegenPerS = 0.035f;

	/** CP granted per completed objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 ObjectiveBonus = 2;

	/** CP granted per kill-streak step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float KillStreakBonus = 0.2f;

	static FRFCommandPointRules FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Radio/comms constraints from support.json "comms". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFCommsRules
{
	GENERATED_BODY()

	/** True when at least one radio must be alive in the squad to call support. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	bool bRadioRequired = true;

	/** Radio set weight in kilograms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float RadioWeightKg = 2.4f;

	/** Inventory slots the radio occupies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	int32 RadioSlots = 1;

	/** Seconds the squad is radio-silent after the set is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float DestroyedLockoutS = 60.0f;

	/** Delay multiplier applied to already-queued missions when the line breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float LineBreakDelayMult = 2.0f;

	/** Extra delay in seconds when a signal flare must be fired (night/smoke). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float SignalFlareDelayS = 6.0f;

	/** Dispersion multiplier when a signal flare marks the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float SignalFlareSpreadMult = 0.5f;

	/** Seconds after a call during which enemy counter-battery may trigger on hard difficulties. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Support")
	float CounterIntelWindowS = 20.0f;

	static FRFCommsRules FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One objective from a level definition. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFObjectiveDef
{
	GENERATED_BODY()

	/** Id unique inside the level, e.g. "obj1". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	FName Id = NAME_None;

	/** Localized objective text (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	FText TextZh;

	/** "primary" or "secondary". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	FName Type = NAME_None;

	/** True when failing the objective fails the mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	bool bRequired = false;

	/** Time limit in seconds; <= 0 means untimed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	float TimeLimitS = 0.0f;

	/** Optional runtime condition token; empty objectives use proximity interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	FName Condition = NAME_None;

	/** Required uninterrupted hold duration for "hold_position". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	float HoldTimeS = 0.0f;

	/** Radius in metres for spatial objective conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	float RadiusM = 4.0f;

	/** Required eliminations for the "eliminate_count" condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	int32 TargetCount = 0;

	/** Optional set of enemy contract ids counted by an elimination objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	TArray<FName> TargetEnemyIds;

	/** True when the objective is a hard fail condition. */
	bool IsPrimary() const { return Type == FName(TEXT("primary")); }

	static FRFObjectiveDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Historical-accuracy split required by data/levels/SCHEMA.md. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFHistoricalAccuracy
{
	GENERATED_BODY()

	/** Events/units/dates that are documented and must stay factual. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|History")
	TArray<FText> VerifiedEvents;

	/** Deliberate gameplay dramatizations; never presented as history in-game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|History")
	TArray<FText> Dramatized;

	static FRFHistoricalAccuracy FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Unlock reward granted on level completion. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFUnlockReward
{
	GENERATED_BODY()

	/** Experience granted, 800-3000 per the schema. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 Xp = 0;

	/** Weapon id unlocked, or NAME_None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName Weapon = NAME_None;

	/** Class id unlocked, or NAME_None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName ClassId = NAME_None;

	static FRFUnlockReward FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** Endless-mode seeding block from the level schema. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFEndlessSeed
{
	GENERATED_BODY()

	/** Era token: 1941 / 1942 / 1943 / 1945. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 Era = 1941;

	/** Enemy weight multiplier, 0.9-1.2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	float WeightMult = 1.0f;

	static FRFEndlessSeed FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One level row per data/levels/*.json entries. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFLevelDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Contract id in the form <faction>_<two-digit index>_<short name>. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName Id = NAME_None;

	/** Global index 1..56. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 Index = 0;

	/** Index inside the campaign, starting at 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 NumberInCampaign = 0;

	/** Owning campaign id from campaigns.json. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName CampaignId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText NameEn;

	/** In-fiction date, "YYYY-MM-DD". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName Date = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText LocationZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText LocationEn;

	/** Real-world coordinates, "48.71N 37.53E". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FName Coordinates = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	ERFMissionType MissionType = ERFMissionType::Defense;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	ERFFaction Faction = ERFFaction::Soviet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText PlayerRoleZh;

	/** 1-3 recommended class ids from classes.json. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	TArray<FName> PlayerClassRecommended;

	/** 2-5 objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	TArray<FRFObjectiveDef> Objectives;

	/** Enemy id -> count, 3-8 entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	TMap<FName, int32> EnemyComposition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText EnemyTacticsZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText FriendlyForcesZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	ERFTerrain Terrain = ERFTerrain::Field;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	ERFWeather Weather = ERFWeather::Clear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	ERFTimeOfDay TimeOfDay = ERFTimeOfDay::Day;

	/** Visibility in metres, 50-1200; scales AI awareness and artillery accuracy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 VisibilityM = 500;

	/** Difficulty rating 1-10. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 Difficulty = 1;

	/** Par time in seconds, 600-2400. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 ParTimeS = 900;

	/** Loadout budget in kilograms (Soviet 24, US 26 baseline). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	float WeightBudgetKg = 24.0f;

	/** Resupply points available, 0-4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	int32 ResupplyPoints = 0;

	/** Support call-in ids allowed in this level, 0-4. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	TArray<FName> AvailableSupport;

	/** Vehicle call-in ids or scene vehicle ids usable in this level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	TArray<FName> VehicleAvailable;

	/** 150-260 character historical note (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText HistoricalNoteZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FRFHistoricalAccuracy HistoricalAccuracy;

	/** 80-150 character design intent note (zh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText DesignNoteZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FText MusicMood;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FRFUnlockReward UnlockReward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Level")
	FRFEndlessSeed EndlessSeedModifier;

	/** Total enemy strength summed over EnemyComposition, used for difficulty validation. */
	int32 GetTotalEnemyCount() const;

	static FRFLevelDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One campaign row from data/campaigns.json "campaigns". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFCampaignDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FText NameZh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FText NameEn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	ERFFaction Faction = ERFFaction::Soviet;

	/** "east" or "west". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FName Front = NAME_None;

	/** Inclusive global level index range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	int32 LevelRangeStart = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	int32 LevelRangeEnd = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FName StartDate = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FName EndDate = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FText Tone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FText PlayerArcZh;

	/** Mechanics taught across the campaign ("weight", "food_water", ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	TArray<FName> MechanicsFocus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Campaign")
	FText UnlockProgression;

	static FRFCampaignDef FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One difficulty tier from campaigns.json "difficulty_scaling". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFDifficultyScaling : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	ERFDifficulty Difficulty = ERFDifficulty::Regular;

	/** Multiplier on enemy hit points (1.0 = design baseline). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	float EnemyHpMult = 1.0f;

	/** Multiplier on enemy accuracy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	float EnemyAccuracyMult = 1.0f;

	/** Multiplier on damage dealt by the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	float PlayerDamageMult = 1.0f;

	/** Multiplier on command point regeneration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	float CpRegenMult = 1.0f;

	/** Multiplier on food/water drain (veteran 1.2, historical 1.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	float SurvivalDrainMult = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Difficulty")
	FText Desc;

	static FRFDifficultyScaling FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/** One wave entry from support.json "endless_mode.wave_table". */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFEndlessWave
{
	GENERATED_BODY()

	/** Wave number (the table is sparse: 1,2,3,4,5,6,10,15). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Endless")
	int32 Wave = 1;

	/** Composed enemy era: 1941 / 1942 / 1943 / 1945. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Endless")
	int32 Era = 1941;

	/** Enemy id -> count for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Endless")
	TMap<FName, int32> Composition;

	/** True for boss waves (5, 10, 15). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Endless")
	bool bBoss = false;

	static FRFEndlessWave FromJson(const TSharedPtr<FJsonObject>& Obj);
};

/**
 * Shared JSON helpers for every FRF*Def::FromJson implementation.
 * All accessors are null-safe and fall back to the supplied default so that a
 * partially-authored contract row never crashes a shipping build.
 */
namespace RFJson
{
	/** Validates the shared pointer and returns the object or nullptr. */
	const TSharedPtr<FJsonObject>* GetObject(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** Number of entries in an array field, 0 when missing. */
	int32 ArrayCount(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** Numeric field as float, or Default. */
	float Number(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default = 0.0f);

	/** Numeric field as int32, or Default. */
	int32 Integer(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Default = 0);

	/** String field as FString, or Default. */
	FString String(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default = FString());

	/** String field as FName; NAME_None when the key is absent or the string is empty. */
	FName Name(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** String field as localized FText (authored content is Chinese). */
	FText Text(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** Boolean field, or Default. */
	bool Bool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Default = false);

	/** String array -> FName array. */
	TArray<FName> NameArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** String array -> FText array. */
	TArray<FText> TextArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);

	/** Object field -> object map of int32 (enemy_composition, wave composition). */
	TMap<FName, int32> IntMap(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field);
}
