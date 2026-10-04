// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "RFDamageTypes.h"

// Reflected types produced by this system (ERFPenetrationResult, FRFShotResult) live in
// RFDamageTypes.h, which owns this module's *.generated.h. This header is a pure
// declaration namespace and therefore has no generated file of its own.

class ARFCharacter;
class URFWeaponBase;

/**
 * Stateless ballistics maths for the 2D plane.
 *
 * Rules implemented (from the contracts):
 *   * Penetration: a round penetrates when weapon penetration_mm >= target armour_mm.
 *     Vehicles use side_rear_ratio to scale frontal armour for side/rear aspects
 *     (Tiger I: 100 mm front, 80 mm side; Pak 40: ratio 1.0, one value everywhere).
 *   * Partial damage: when penetration falls short by less than 25%, the round still
 *     delivers a fraction (spall) instead of nothing.
 *   * Damage falloff: full damage out to effective_range_m, then a linear falloff to
 *     35% at 2x effective range, after which the round only wounds.
 *   * Ricochet: shallow impacts (past ricochet_angle_deg) on armour deflect instead of
 *     penetrating, with a difficulty-independent probability from RFDamageTypes.
 */
namespace RFBallistics
{
	/** Effective armour in mm RHA for an impact aspect; uses side_rear_ratio. */
	REDFRONT1941_API float GetEffectiveArmorMm(const FRFVehicleDef& Vehicle, bool bSideOrRear);

	/** Damage multiplier for a travel distance in metres (1.0 inside effective range). */
	REDFRONT1941_API float GetDamageFalloffMultiplier(float DistanceM, float EffectiveRangeM);

	/**
	 * Resolves penetration and damage against an armour value in mm RHA.
	 * @param PenetrationMm  Round penetration (weapons.json penetration_mm).
	 * @param ArmorMm        Effective armour of the target in mm RHA.
	 * @param ImpactAngleDeg 0 = perpendicular, 90 = grazing.
	 * @return Result code; AppliedDamage is filled by the caller through GetDamageAfterPenetration.
	 */
	REDFRONT1941_API ERFPenetrationResult ResolvePenetration(float PenetrationMm, float ArmorMm, float ImpactAngleDeg);

	/** Damage actually delivered after penetration outcome and falloff. */
	REDFRONT1941_API float GetDamageAfterPenetration(float BaseDamage, ERFPenetrationResult Result, float FalloffMultiplier);

	/** True when the shallow impact angle should ricochet; angle in degrees from the plane. */
	REDFRONT1941_API bool ShouldRicochet(float ImpactAngleDeg, float ArmorMm);

	/**
	 * Fires one round from a soldier along the plane.
	 *
	 * The shot is a line trace on the gameplay plane with a random cone of half-angle
	 * SpreadDeg around the aim direction. Hit resolution applies falloff, penetration
	 * and ricochet, then routes damage through UGameplayStatics::ApplyDamage with the
	 * RF damage types so armour and bleeding behave consistently.
	 *
	 * @param Origin       Muzzle position, world centimetres.
	 * @param AimDirection Planar unit direction (X = East, Y = North).
	 * @param SpreadDeg    Half-angle of the dispersion cone, degrees.
	 * @param Weapon       Weapon contract providing damage/range/penetration.
	 * @param Shooter      Firing soldier (damage instigator).
	 * @param WeaponComp   Firing component (damage causer), may be null.
	 * @return What happened to the round.
	 */
	REDFRONT1941_API FRFShotResult ResolveShot(const FVector& Origin, const FVector2D& AimDirection,
		float SpreadDeg, const FRFWeaponDef& Weapon, const AActor* Shooter, const URFWeaponBase* WeaponComp);

	/** Applies spread to a direction and returns a unit planar vector. */
	REDFRONT1941_API FVector2D ApplySpreadToDirection(const FVector2D& Direction, float SpreadDeg);
}
