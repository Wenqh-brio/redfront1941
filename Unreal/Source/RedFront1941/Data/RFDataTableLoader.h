// Copyright RedFront1941. All Rights Reserved.
//
// DATA PIPELINE
// -------------
// The shared game data lives in E:\GAMES\RedFront1941\data\*.json and is owned by the
// data author, not by this module. Two supported routes bring it into the engine:
//
//   1. EDITOR IMPORTER (optional asset-backed path)
//        a. tools/ regenerates CSVs into ../data/generated/.
//        b. In the editor: Tools > Import DataTable, pick the CSV, and choose the row
//           struct that matches (FRFWeaponDef, FRFClassDef, FRFItemDef, FRFEnemyDef,
//           FRFVehicleDef, FRFAirUnitDef, FRFSupportCallInDef, FRFCampaignDef,
//           FRFDifficultyScaling, FRFLevelDef).
//        c. Save the resulting DT_* assets under Content/RedFront/Data/.
//      Every USTRUCT in RFDataTypes.h derives from FTableRowBase precisely so this works.
//
//   2. RUNTIME JSON LOADER
//      LoadAllContracts() reads Content/RedFront/Data/*.json at runtime and builds the
//      same rows into transient UDataTables. The JSON directory is staged as NonUFS so
//      it remains directly readable in Development and Shipping builds.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "UObject/Object.h"
#include "RFDataTableLoader.generated.h"

class UDataTable;

/**
 * Builds and owns the RedFront DataTables from the shared JSON contracts.
 *
 * The loader is created by ARFGameMode and lives for the session; every other system
 * reads the parsed maps (weapons/classes/items/support/campaigns/levels) rather than the
 * DataTables directly, so a missing DataTable row can never crash gameplay code.
 */
UCLASS(BlueprintType)
class REDFRONT1941_API URFDataTableLoader : public UObject
{
	GENERATED_BODY()

public:
	/** Loads every contract file under Content/RedFront/Data. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data")
	bool LoadAllContracts();

	/**
	 * Loads one contract file.
	 * @param FileName  Bare file name including extension, e.g. "weapons.json".
	 * @param OutRoot   Parsed root object of the file.
	 * @return True when the file existed and parsed.
	 *
	 * NOTE: deliberately NOT a UFUNCTION — UHT cannot parse TSharedPtr<FJsonObject>
	 * in a reflected signature ("Unable to find ... with name 'TSharedPtr'").
	 * Blueprint callers should use LoadAllContracts() instead.
	 */
	bool LoadJsonFile(const FString& FileName, TSharedPtr<FJsonObject>& OutRoot) const;

	/** Parses weapons.json into Weapons and builds DT_Weapons. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildWeaponTable();
	/** Parses classes.json into Classes and builds DT_Classes. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildClassTable();
	/** Parses equipment.json into Items and builds DT_Items. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildItemTable();
	/** Parses enemies.json infantry into Enemies and builds DT_Enemies. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildEnemyTable();
	/** Parses enemies.json vehicles into Vehicles and builds DT_Vehicles. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildVehicleTable();
	/** Parses enemies.json air into AirUnits and builds DT_AirUnits. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildAirUnitTable();
	/** Parses support.json into SupportCallIns, CP rules and comms rules. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildSupportTables();
	/** Parses campaigns.json into Campaigns and DifficultyRows. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildCampaignTables();
	/** Parses every data/levels/*.json into Levels keyed by contract id. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data") bool BuildLevelTables();

	/** Looks up a level contract by id across every loaded level table. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Data")
	bool FindLevel(FName LevelId, FRFLevelDef& OutLevel) const;

	/** A weapon contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindWeapon(FName Id, FRFWeaponDef& Out) const;
	/** An item contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindItem(FName Id, FRFItemDef& Out) const;
	/** A class contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindClass(FName Id, FRFClassDef& Out) const;
	/** An infantry enemy contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindEnemy(FName Id, FRFEnemyDef& Out) const;
	/** A vehicle contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindVehicle(FName Id, FRFVehicleDef& Out) const;
	/** A support call-in contract by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") bool FindSupportCallIn(FName Id, FRFSupportCallInDef& Out) const;

	/** Every parsed weapon, keyed by id (consumed by the loadout subsystem). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFWeaponDef>& GetWeapons() const { return Weapons; }
	/** Every parsed item, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFItemDef>& GetItems() const { return Items; }
	/** Every parsed class, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFClassDef>& GetClasses() const { return Classes; }
	/** Every parsed infantry enemy, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFEnemyDef>& GetEnemies() const { return Enemies; }
	/** Every parsed vehicle, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFVehicleDef>& GetVehicles() const { return Vehicles; }
	/** Every parsed air unit, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFAirUnitDef>& GetAirUnits() const { return AirUnits; }
	/** Every parsed support call-in, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TArray<FRFSupportCallInDef>& GetSupportCallIns() const { return SupportCallIns; }
	/** Every parsed campaign, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFCampaignDef>& GetCampaigns() const { return Campaigns; }
	/** Every parsed level, keyed by id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFLevelDef>& GetLevels() const { return Levels; }

	/** Command point economy from support.json. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const FRFCommandPointRules& GetCommandPointRules() const { return CommandPointRules; }
	/** Comms constraints from support.json. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const FRFCommsRules& GetCommsRules() const { return CommsRules; }
	/** Difficulty scaling table from campaigns.json, keyed by tier id. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TMap<FName, FRFDifficultyScaling>& GetDifficultyScaling() const { return DifficultyScaling; }
	/** Endless wave table from support.json. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TArray<FRFEndlessWave>& GetEndlessWaves() const { return EndlessWaves; }
	/** Load bands from classes.json weight_system. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") const TArray<FRFWeightBand>& GetLoadBands() const { return LoadBands; }

	/** Base carry capacity in kilograms (Soviet 24, US 26). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") float GetBaseCapacityKg() const { return BaseCapacityKg; }
	/** US carry capacity in kilograms. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Data") float GetUsCapacityKg() const { return UsCapacityKg; }

protected:
	/** Parsed contracts, keyed by contract id. */
	UPROPERTY(Transient) TMap<FName, FRFWeaponDef> Weapons;
	UPROPERTY(Transient) TMap<FName, FRFItemDef> Items;
	UPROPERTY(Transient) TMap<FName, FRFClassDef> Classes;
	UPROPERTY(Transient) TMap<FName, FRFEnemyDef> Enemies;
	UPROPERTY(Transient) TMap<FName, FRFVehicleDef> Vehicles;
	UPROPERTY(Transient) TMap<FName, FRFAirUnitDef> AirUnits;
	UPROPERTY(Transient) TMap<FName, FRFSupportCallInDef> SupportCallInsById;
	UPROPERTY(Transient) TMap<FName, FRFCampaignDef> Campaigns;
	UPROPERTY(Transient) TMap<FName, FRFLevelDef> Levels;
	UPROPERTY(Transient) TMap<FName, FRFDifficultyScaling> DifficultyScaling;

	/** Support call-ins in contract order (the map above is the id lookup). */
	UPROPERTY(Transient) TArray<FRFSupportCallInDef> SupportCallIns;

	/** Endless wave table. */
	UPROPERTY(Transient) TArray<FRFEndlessWave> EndlessWaves;

	/** Weight bands from classes.json weight_system.load_bands. */
	UPROPERTY(Transient) TArray<FRFWeightBand> LoadBands;

	/** CP economy from support.json command_points. */
	UPROPERTY(Transient) FRFCommandPointRules CommandPointRules;

	/** Comms constraints from support.json comms. */
	UPROPERTY(Transient) FRFCommsRules CommsRules;

	/** Base (Soviet) carry capacity in kilograms. */
	UPROPERTY(Transient) float BaseCapacityKg = 24.0f;

	/** US carry capacity in kilograms. */
	UPROPERTY(Transient) float UsCapacityKg = 26.0f;

	/** Runtime DataTables built from the JSON (editor/iteration only). */
	UPROPERTY(Transient) TObjectPtr<UDataTable> WeaponTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> ClassTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> ItemTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> EnemyTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> VehicleTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> SupportTable;

	/** Absolute folder the JSON contracts are read from (Content/RedFront/Data). */
	FString GetContractFolder() const;

	/** Absolute folder the generated CSVs live in, for the editor importer note. */
	FString GetGeneratedCsvFolder() const;

	/**
	 * Rebuilds a transient UDataTable from parsed rows.
	 * Implemented in the .cpp for each concrete row type (no member template: UHT does
	 * not parse templates inside a UCLASS body).
	 */
	void RebuildWeaponTable();
	void RebuildClassTable();
	void RebuildItemTable();
	void RebuildEnemyTable();
	void RebuildVehicleTable();
	void RebuildSupportTable();
};
