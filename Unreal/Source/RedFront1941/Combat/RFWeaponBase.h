// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFWeaponBase.generated.h"

class ARFCharacter;
class URFSurvivalComponent;
class URFWeightComponent;

/**
 * A carried weapon. One instance exists per carried weapon; it is spawned by the
 * loadout subsystem and attached to the owning soldier.
 *
 * Firing model (values straight from weapons.json):
 *   rate      = 60 / rpm seconds between rounds (PPSh-41 900 rpm = 0.067 s)
 *   damage    = damage, 0-100 health scale, before RFBallistics falloff
 *   spread    = f(accuracy, movement, load band, bipod, ADS, stamina)
 *   reload    = reload_s, reduced by the fast_reload passive and ammo pouches
 *
 * Units: seconds for all timers, kilograms for mass, mm RHA for penetration,
 * metres for effective range, and centimetres for anything world-space.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API URFWeaponBase : public UActorComponent
{
	GENERATED_BODY()

public:
	URFWeaponBase();

	/** Resolves the owning soldier and applies the contract defaults. */
	virtual void BeginPlay() override;

	/** Advances the fire loop, the reload timer and the spread recovery. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Installs the parsed weapon contract row. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	void InitializeFromDefinition(const FRFWeaponDef& InDefinition);

	/** Contract data of this weapon. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	const FRFWeaponDef& GetDefinition() const { return Definition; }

	// ------------------------------- Firing ----------------------------------

	/** Primary fire edge: starts the fire loop (respects the selected fire mode). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual void StartFire();

	/** Primary fire release: stops automatic fire. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual void StopFire();

	/** True while the weapon is cycling rounds. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	bool IsFiring() const { return bFiring; }

	/** True while a reload is in progress. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	bool IsReloading() const { return bReloading; }

	/** Begins a reload; returns false when the magazine is full or ammo is out. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual bool StartReload();

	/** Cancels a reload (weapon swap, melee, death). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	void CancelReload();

	/** Rounds currently in the magazine/belt. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }

	/** Sets the magazine contents (used when re-equipping a stowed weapon). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	void SetMagazineAmmo(int32 NewAmmo);

	/** Rounds held in reserve for this weapon's ammo_type. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	/** Sets the reserve pool (resupply points top it up). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	void SetReserveAmmo(int32 NewReserve);

	/** Fills reserve ammunition to the carried-load capacity; returns rounds added. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	int32 ReplenishReserveAmmo();

	/** Switches between single and full auto when the weapon supports both. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	void CycleFireMode();

	/** Current fire mode. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	ERFFireMode GetFireMode() const { return FireMode; }

	/** Seconds between rounds for the contract rate of fire. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	float GetShotIntervalS() const;

	// -------------------------------- States ---------------------------------

	/** Enters or leaves the aimed-down-sights state (slower movement, tighter spread). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual void SetAimingDownSights(bool bInAiming);

	/** True while aiming down sights. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	bool IsAimingDownSights() const { return bAimingDownSights; }

	/**
	 * Deploys or stows the bipod. Deploying takes bipod_deploy_s and roots the soldier;
	 * while deployed the spread is multiplied by bipod_spread_mult.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual bool SetBipodDeployed(bool bDeployed);

	/** True while the bipod is deployed. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	bool IsBipodDeployed() const { return bBipodDeployed; }

	/** True when the actor is a soldier that can fire/hold this weapon. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	ARFCharacter* GetOwningSoldier() const;

	// -------------------------------- Spread ---------------------------------

	/**
	 * Current shot dispersion half-angle in degrees.
	 * Composed from the contract accuracy, movement, load band, stance, ADS, bipod
	 * and the soldier's stamina/suppression sway.
	 */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	float ComputeCurrentSpreadDeg() const;

	/** Accuracy value 0-1 converted to a base cone half-angle in degrees. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	static float AccuracyToBaseSpreadDeg(float Accuracy, float EffectiveRangeM);

protected:
	/** Contract row for this weapon (weapons.json). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	FRFWeaponDef Definition;

	/** Rounds in the magazine/belt/drum. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	int32 MagazineAmmo = 0;

	/** Rounds in reserve for this ammo_type. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	int32 ReserveAmmo = 0;

	/** True while the fire loop is running. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	bool bFiring = false;

	/** True while a reload is in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	bool bReloading = false;

	/** True while aiming down sights. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	bool bAimingDownSights = false;

	/** True while the bipod is deployed. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	bool bBipodDeployed = false;

	/** Selected fire mode. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	ERFFireMode FireMode = ERFFireMode::Single;

	/** Seconds until the next round may be fired. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	float TimeUntilNextShotS = 0.0f;

	/** Seconds left on the reload. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Weapon")
	float ReloadRemainingS = 0.0f;

	/** Spread multiplier while aiming down sights (design: 0.45). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Weapon")
	float AdsSpreadMultiplier = 0.45f;

	/** Spread multiplier while the bipod is deployed (MG passives: -50%). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Weapon")
	float BipodSpreadMultiplier = 0.5f;

	/** Seconds required to deploy the bipod (machine gunner passive: 1.2 s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Weapon")
	float BipodDeployS = 1.2f;

	/** Reload speed multiplier from the class passive fast_reload. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Weapon")
	float ReloadSpeedMultiplier = 1.0f;

	/** Fires exactly one round; consumed by the fire loop and by single-shot weapons. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual void FireOneRound();

	/** True when the weapon can currently fire (ammo, timers, stance, reload). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Weapon")
	bool CanFireNow() const;

	/** Applies class/equipment modifiers (fast reload, bipod discipline, ...). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Weapon")
	virtual void ApplyOwnerModifiers();
};
