// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFGrenade.generated.h"

class UPaperFlipbookComponent;
class USphereComponent;

/** Grenade archetypes driven by equipment.json "effect" keys. */
UENUM(BlueprintType)
enum class ERFGrenadeKind : uint8
{
	Fragmentation	UMETA(DisplayName = "Fragmentation (RGD-33 / F-1 / Mk 2)"),
	Smoke			UMETA(DisplayName = "Smoke (RDG-42 / AN-M8)"),
	Incendiary		UMETA(DisplayName = "Incendiary (Molotov)"),
	AntiTank		UMETA(DisplayName = "Anti-tank HEAT (RPG-43)"),
	Demolition		UMETA(DisplayName = "Demolition charge (satchel)")
};

/**
 * Thrown explosive / obscurant / incendiary device.
 *
 * Contract behaviour:
 *   * fuse_s is the real fuse length (RGD-33 3.5 s, F-1 4.0 s, Mk 2 4.5 s, satchel 6 s).
 *   * Hold-to-cook is modelled by subtracting the already-burnt fuse from FuseRemainingS
 *     when the player releases the throw (RFPlayerController does the hold).
 *   * Fragmentation damage falls off linearly from the centre to radius_m; the fragment
 *     count is used for audio/VFX density and for multi-hit rolls on clustered targets.
 *   * Molotov (fuse 0) detonates on impact and leaves burning ground for fire_duration_s.
 *   * RPG-43 requires a near-normal plate hit; it carries penetration_mm 75.
 *
 * Units: seconds, metres for radius (converted to cm internally), mm RHA for penetration.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFGrenade : public AActor
{
	GENERATED_BODY()

public:
	ARFGrenade();

	/** Sets up collision and the fuse flipbook. */
	virtual void BeginPlay() override;

	/** Advances the fuse, the throw arc and the smoke/fire volume lifetimes. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Spawns and throws a grenade from a soldier.
	 * @param Thrower       Soldier performing the throw (used as damage instigator).
	 * @param TargetPoint   Aim point on the plane, world centimetres.
	 * @param CookedTimeS   Fuse already burnt while held; subtracted from the fuse.
	 * @return The spawned grenade, or null when spawn/collision failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Grenade", meta = (WorldContext = "Thrower"))
	static ARFGrenade* ThrowCookedGrenade(AActor* Thrower, const FVector& TargetPoint, float CookedTimeS);

	/** Spawns a grenade actor already resting at a location (mines, dropped charges). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Grenade", meta = (WorldContext = "WorldContextObject"))
	static ARFGrenade* SpawnPlacedCharge(const UObject* WorldContextObject, const FVector& Location,
		const FRFItemDef& Item, AActor* Placer);

	/** Installs the item contract row this grenade was spawned from. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Grenade")
	void InitializeFromItem(const FRFItemDef& InItem);

	/** Remaining fuse in seconds; 0 means it detonates this frame. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Grenade")
	float GetFuseRemainingS() const { return FuseRemainingS; }

	/** Grenade archetype resolved from the item contract. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Grenade")
	ERFGrenadeKind GetGrenadeKind() const { return GrenadeKind; }

	/** Detonates immediately (fuse expiry, impact fuse, or scripted objective爆破). */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Grenade")
	void Detonate();

protected:
	/** Contract row of the thrown item. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	FRFItemDef ItemDefinition;

	/** Resolved archetype. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	ERFGrenadeKind GrenadeKind = ERFGrenadeKind::Fragmentation;

	/** Collision sphere used for cooking overlaps while the grenade is live. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Grenade")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** Sprite for the thrown device. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Grenade")
	TObjectPtr<UPaperFlipbookComponent> Flipbook;

	/** Seconds left before detonation. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	float FuseRemainingS = 3.5f;

	/** True for impact-fused devices (molotov) and for a fully cooked grenade. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	bool bImpactFuse = false;

	/** Distance between the plate normal and the flight direction that still fuses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Grenade")
	float ThrowSpeedCms = 1400.0f;

	/** Burn time left for incendiary ground fire, seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	float FireRemainingS = 0.0f;

	/** Smoke cloud life left, seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	float SmokeRemainingS = 0.0f;

	/** Instigator for the damage call. */
	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Grenade")
	TObjectPtr<AActor> ThrowerActor;

	/** Called when the device touches the ground or a target. */
	UFUNCTION()
	void OnGrenadeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Applies fragmentation damage in the blast radius. */
	void ApplyFragmentationEffect();

	/** Spawns the smoke volume (no damage, blocks AI awareness and MG fire). */
	void ApplySmokeEffect();

	/** Spawns burning ground (damage over time to anything standing in it). */
	void ApplyIncendiaryEffect();

	/** Anti-tank effect: HEAT penetration against vehicles only. */
	void ApplyAntiTankEffect();
};
