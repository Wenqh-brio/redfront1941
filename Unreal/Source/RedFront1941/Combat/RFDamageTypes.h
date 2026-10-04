// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/DamageType.h"
#include "RFDamageTypes.generated.h"

class URFWeaponBase;
class UWorld;

/** Why a projectile failed to damage its target; drives HUD feedback and audio. */
UENUM(BlueprintType)
enum class ERFPenetrationResult : uint8
{
	NoTarget		UMETA(DisplayName = "No target"),
	Penetrated		UMETA(DisplayName = "Penetrated"),
	PartialDamage	UMETA(DisplayName = "Stopped by armour (partial)"),
	Ricochet		UMETA(DisplayName = "Ricochet"),
	ArmourImmune	UMETA(DisplayName = "No effect (armour too thick)")
};

/** Full description of one resolved shot, produced by RFBallistics::ResolveShot. */
USTRUCT(BlueprintType)
struct REDFRONT1941_API FRFShotResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Ballistics")
	ERFPenetrationResult Result = ERFPenetrationResult::NoTarget;

	/** Damage actually applied on the 0-100 health scale. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Ballistics")
	float AppliedDamage = 0.0f;

	/** Impact point in world centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Ballistics")
	FVector ImpactPoint = FVector::ZeroVector;

	/** Travel distance in metres (used for falloff and for the after-action report). */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Ballistics")
	float DistanceM = 0.0f;

	/** True when the round hit a vehicle's side or rear aspect. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Ballistics")
	bool bSideOrRearAspect = false;
};

/**
 * Armour of a vehicle or emplacement actor.
 *
 * Vehicles are 2D sprite actors that never derive from ARFCharacter, so their armour
 * is exposed through this component (front mm RHA plus the side_rear_ratio from
 * enemies.json). Adding it to a vehicle Blueprint makes it penetrable by
 * RFBallistics without any further code.
 */
UCLASS(ClassGroup = (RedFront), meta = (BlueprintSpawnableComponent))
class REDFRONT1941_API URFVehicleArmorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URFVehicleArmorComponent();

	/** Installs the vehicle contract row (armor_mm + side_rear_ratio). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Armor")
	void InitializeFromDefinition(const FRFVehicleDef& InDefinition);

	/** Contract id of the vehicle this component belongs to. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Armor")
	FName GetVehicleId() const { return VehicleId; }

	/** Frontal armour in mm RHA. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Armor")
	float GetFrontArmorMm() const { return FrontArmorMm; }

	/** Side/rear armour in mm RHA (front x side_rear_ratio). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Armor")
	float GetSideRearArmorMm() const;

	/** Effective armour for an impact aspect. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Armor")
	float GetArmorForAspect(bool bSideOrRear) const;

protected:
	/** Contract id, e.g. "pzkpfw_iv_f2". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RedFront|Armor")
	FName VehicleId = NAME_None;

	/** Frontal effective armour in mm RHA. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Armor")
	float FrontArmorMm = 0.0f;

	/** Side/rear ratio from the contract (Tiger I: 0.8, StuG III G: 0.45). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RedFront|Armor")
	float SideRearRatio = 0.5f;
};

/** Rifle, SMG, MG and pistol fire; causes bleeding. */
UCLASS()
class REDFRONT1941_API URFFirearmDamage : public UDamageType
{
	GENERATED_BODY()

public:
	URFFirearmDamage();
};

/** Fragmentation from grenades, mines and artillery; area falloff, causes bleeding. */
UCLASS()
class REDFRONT1941_API URFExplosiveDamage : public UDamageType
{
	GENERATED_BODY()

public:
	URFExplosiveDamage();
};

/** Incendiary damage from flamethrowers, molotovs and burning ground. */
UCLASS()
class REDFRONT1941_API URFIncendiaryDamage : public UDamageType
{
	GENERATED_BODY()

public:
	URFIncendiaryDamage();
};

/** Melee damage from bayonets, entrenching tools and rifle butts. */
UCLASS()
class REDFRONT1941_API URFMeleeDamage : public UDamageType
{
	GENERATED_BODY()

public:
	URFMeleeDamage();
};

/**
 * Shared damage plumbing for RedFront.
 *
 * Design intent: every system that hurts something goes through here, so armour
 * aspects, bleeding flags and the damage-type-to-causer mapping exist in one place
 * and the survival component receives a consistent call.
 */
namespace RFDamage
{
	/**
	 * Armour of an actor in mm RHA.
	 * Soldiers use armor_value from enemies.json; vehicles use armor_mm scaled by
	 * side_rear_ratio (never below 1 mm for a real armour plate).
	 * @param bOutSideOrRear Set to true when the impact resolved onto a side/rear aspect.
	 */
	REDFRONT1941_API float GetActorArmorMm(const AActor* Target, bool& bOutSideOrRear);

	/** Impact angle in degrees; 0 = perpendicular to the surface, 90 = grazing. */
	REDFRONT1941_API float GetImpactAngleDeg(const FVector& ShotDirection, const FVector& ImpactNormal);

	/**
	 * Applies damage through the engine so damage types, death and HUD feedback agree.
	 * @param Bleeding True for wounds that keep bleeding until bandaged.
	 * @return Damage actually applied.
	 */
	REDFRONT1941_API float ApplyBallisticDamage(UWorld* World, AActor* Target, float Amount, bool bBleeding,
		const FVector& ImpactPoint, const FVector& ShotDirection, const AActor* Shooter,
		const UObject* DamageCauser, float PenetrationMm);

	/**
	 * Applies explosion damage with linear falloff from the blast centre.
	 * @param RadiusM      Effect radius in metres.
	 * @param FullDamageM  Radius in metres within which full damage applies (default 35%).
	 * @return Number of actors damaged.
	 */
	REDFRONT1941_API int32 ApplyExplosionDamage(UWorld* World, const FVector& Center, float RadiusM,
		float Damage, float PenetrationMm, const AActor* Shooter, const UObject* DamageCauser,
		float FullDamageM = -1.0f);
}
