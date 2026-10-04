// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFBallistics.h"

#include "Combat/RFDamageTypes.h"
#include "Combat/RFWeaponBase.h"
#include "CollisionQueryParams.h"
#include "Core/RFCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Survival/RFWeightComponent.h"

namespace
{
	/** Damage delivered when a round is stopped by armour but still spalls. */
	constexpr float PartialPenetrationDamageFraction = 0.25f;

	/** Penetration shortfall (as a fraction of the armour) still allowed to spall. */
	constexpr float PartialPenetrationTolerance = 0.25f;

	/** Falloff floor: a round at twice the effective range still does 35% damage. */
	constexpr float FalloffFloor = 0.35f;

	/** Grazing impacts past this angle (degrees) may ricochet off armour. */
	constexpr float RicochetAngleDeg = 70.0f;
}

float RFBallistics::GetEffectiveArmorMm(const FRFVehicleDef& Vehicle, bool bSideOrRear)
{
	return FRFVehicleDef::GetArmorForAspect(Vehicle, bSideOrRear);
}

float RFBallistics::GetDamageFalloffMultiplier(float DistanceM, float EffectiveRangeM)
{
	const float Range = FMath::Max(1.0f, EffectiveRangeM);
	if (DistanceM <= Range)
	{
		return 1.0f;
	}

	// Linear from 1.0 at effective range to FalloffFloor at 2x effective range, then flat.
	// This keeps a Mosin (550 m) dangerous at 700 m while a PPSh-41 (150 m) is a
	// 40 m weapon in practice.
	const float ExcessRatio = FMath::Clamp((DistanceM - Range) / Range, 0.0f, 1.0f);
	return FMath::Lerp(1.0f, FalloffFloor, ExcessRatio);
}

bool RFBallistics::ShouldRicochet(float ImpactAngleDeg, float ArmorMm)
{
	// Unarmoured targets (flesh, sandbags) never ricochet; only hard armour does.
	if (ArmorMm < 5.0f)
	{
		return false;
	}
	return FMath::Abs(ImpactAngleDeg) >= RicochetAngleDeg;
}

ERFPenetrationResult RFBallistics::ResolvePenetration(float PenetrationMm, float ArmorMm, float ImpactAngleDeg)
{
	if (ArmorMm <= 0.0f)
	{
		// Soft target: any round penetrates.
		return ERFPenetrationResult::Penetrated;
	}

	if (ShouldRicochet(ImpactAngleDeg, ArmorMm))
	{
		return ERFPenetrationResult::Ricochet;
	}

	if (PenetrationMm >= ArmorMm)
	{
		return ERFPenetrationResult::Penetrated;
	}

	// Shortfall smaller than 25% of the armour still delivers spall damage; the PTRD-41
	// (35 mm) against a Panzer IV F2 side (27.5 mm) therefore kills reliably, while a
	// Tiger's 80 mm side stops it outright.
	const float Shortfall = (ArmorMm - PenetrationMm) / ArmorMm;
	if (Shortfall <= PartialPenetrationTolerance)
	{
		return ERFPenetrationResult::PartialDamage;
	}

	return ERFPenetrationResult::ArmourImmune;
}

float RFBallistics::GetDamageAfterPenetration(float BaseDamage, ERFPenetrationResult Result, float FalloffMultiplier)
{
	switch (Result)
	{
	case ERFPenetrationResult::Penetrated:
		return BaseDamage * FalloffMultiplier;
	case ERFPenetrationResult::PartialDamage:
		return BaseDamage * FalloffMultiplier * PartialPenetrationDamageFraction;
	default:
		return 0.0f;
	}
}

FVector2D RFBallistics::ApplySpreadToDirection(const FVector2D& Direction, float SpreadDeg)
{
	const FVector2D Base = Direction.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : Direction.GetSafeNormal();

	// Uniform-in-angle jitter inside the cone. A Gaussian-shaped distribution would be
	// more realistic but reads as "unfair misses" to the player at these ranges.
	const float AngleRad = FMath::DegreesToRadians(SpreadDeg);
	const float OffsetRad = FMath::FRandRange(-AngleRad, AngleRad);
	const float Cos = FMath::Cos(OffsetRad);
	const float Sin = FMath::Sin(OffsetRad);

	return FVector2D(Base.X * Cos - Base.Y * Sin, Base.X * Sin + Base.Y * Cos).GetSafeNormal();
}

FRFShotResult RFBallistics::ResolveShot(const FVector& Origin, const FVector2D& AimDirection,
	float SpreadDeg, const FRFWeaponDef& Weapon, const AActor* Shooter, const URFWeaponBase* WeaponComp)
{
	FRFShotResult Result;

	UWorld* World = Shooter != nullptr ? Shooter->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return Result;
	}

	const FVector2D ShotDir2D = ApplySpreadToDirection(AimDirection, SpreadDeg);
	const FVector ShotDir(ShotDir2D.X, ShotDir2D.Y, 0.0f);

	// The weapon's effective range defines the trace length: rounds do not fly forever.
	const float TraceLength = FMath::Max(1000.0f, Weapon.EffectiveRangeM * 200.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RFShot), false, Shooter);
	Params.bTraceComplex = false;
	Params.bReturnPhysicalMaterial = false;
	if (Shooter != nullptr && Shooter->ActorHasTag(FName(TEXT("RF_Enemy"))))
	{
		TArray<AActor*> FriendlyUnits;
		UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("RF_Enemy")), FriendlyUnits);
		for (AActor* FriendlyUnit : FriendlyUnits)
		{
			Params.AddIgnoredActor(FriendlyUnit);
		}
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, Origin, Origin + ShotDir * TraceLength, ECC_Pawn, Params);

	if (!bHit)
	{
		Result.Result = ERFPenetrationResult::NoTarget;
		Result.DistanceM = TraceLength / 100.0f;
		return Result;
	}

	Result.ImpactPoint = Hit.ImpactPoint;
	Result.DistanceM = FVector::Dist(Origin, Hit.ImpactPoint) / 100.0f;
	const float Falloff = GetDamageFalloffMultiplier(Result.DistanceM, Weapon.EffectiveRangeM);

	// Armour of the hit actor: soldiers use their equipment, while vehicle aspect is
	// resolved against the hull's forward axis so side/rear plates use their contract ratio.
	float TargetArmorMm = RFDamage::GetActorArmorMm(Hit.GetActor(), Result.bSideOrRearAspect);
	if (AActor* HitActor = Hit.GetActor())
	{
		if (const URFVehicleArmorComponent* VehicleArmor =
			HitActor->FindComponentByClass<URFVehicleArmorComponent>())
		{
			const float ForwardDot = FMath::Abs(FVector::DotProduct(
				ShotDir.GetSafeNormal(), HitActor->GetActorForwardVector().GetSafeNormal()));
			Result.bSideOrRearAspect = ForwardDot < 0.5f;
			TargetArmorMm = VehicleArmor->GetArmorForAspect(Result.bSideOrRearAspect);
		}
	}
	const float ImpactAngleDeg = RFDamage::GetImpactAngleDeg(ShotDir, Hit.ImpactNormal);

	Result.Result = ResolvePenetration(Weapon.PenetrationMm, TargetArmorMm, ImpactAngleDeg);
	Result.AppliedDamage = GetDamageAfterPenetration(Weapon.Damage, Result.Result, Falloff);

	if (Result.AppliedDamage > 0.0f)
	{
		// Bullet wounds bleed; fragments and HE do not (survival component decides).
		const bool bBleeds = Weapon.Category != ERFWeaponCategory::Flamethrower
			&& Weapon.Category != ERFWeaponCategory::Demolition;
		RFDamage::ApplyBallisticDamage(World, Hit.GetActor(), Result.AppliedDamage, bBleeds,
			Hit.ImpactPoint, ShotDir, Shooter, WeaponComp, Weapon.PenetrationMm);
	}

	return Result;
}
