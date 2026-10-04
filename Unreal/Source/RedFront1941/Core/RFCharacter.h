// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "Data/RFDataTypes.h"
#include "RFCharacter.generated.h"

class UBoxComponent;
class UCameraComponent;
class URFWeightComponent;
class URFInventoryComponent;
class URFSurvivalComponent;
class URFWeaponBase;
class UPaperFlipbook;
class UPaperFlipbookComponent;

/** Ground movement state of the 2D soldier. */
UENUM(BlueprintType)
enum class ERFStance : uint8
{
	Stand		UMETA(DisplayName = "Standing"),
	Crouch		UMETA(DisplayName = "Crouching"),
	Prone		UMETA(DisplayName = "Prone"),
	Sprint		UMETA(DisplayName = "Sprinting")
};

/**
 * The player / squad soldier.
 *
 * 2D contract: the character is a PaperFlipbook on the XY gameplay plane.
 * There is deliberately no 3D capsule locomotion, no gravity-driven movement and
 * no root-motion rotation - facing is a discrete 8-direction value
 * (ERFCharacterFacing) chosen from the aim vector, and every animation set ships
 * one flipbook per facing. Collision is a thin box on the plane.
 */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFCharacter : public APaperCharacter
{
	GENERATED_BODY()

public:
	ARFCharacter();

	/** Wires the survival/inventory/weight components to this character. */
	virtual void BeginPlay() override;

	/** Advances stance, stamina and facing each frame (frame-rate independent). */
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------ Movement --------------------------------

	/**
	 * Sets the desired movement from a screen-space 2D vector (X = East, Y = North).
	 * @param WorldDirection2D Normalized or raw planar direction; the weight band
	 *        and stance multipliers are applied internally.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Movement")
	void SetMoveInput2D(const FVector2D& WorldDirection2D);

	/** True while the sprint input is held and the load band allows sprinting. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Movement")
	void SetSprinting(bool bInSprint);

	/** Changes stance; sprinting is ignored while prone. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Movement")
	void SetStance(ERFStance NewStance);

	/** Current stance. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Movement")
	ERFStance GetStance() const { return Stance; }

	/** Authoritative base walk speed in cm/s before load-band and stance multipliers. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Movement")
	float GetBaseWalkSpeed() const { return BaseWalkSpeedCms; }

	// -------------------------------- Aim ------------------------------------

	/**
	 * Updates the aim point on the gameplay plane (Z = 0), world units (cm).
	 * The controller computes it from the mouse cursor; AI code sets it directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Aim")
	void SetAimWorldLocation(const FVector& WorldLocation);

	/** Last aim point on the plane, world units (cm), Z forced to the plane height. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Aim")
	FVector GetAimWorldLocation() const { return AimWorldLocation; }

	/** Planar unit vector from the soldier to the aim point. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Aim")
	FVector2D GetAimDirection2D() const;

	/** Picks the 8-direction facing bucket that best matches a planar direction. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Aim")
	static ERFCharacterFacing FacingFromDirection2D(const FVector2D& Direction2D);

	/** Unit vector in world space for a facing bucket. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Aim")
	static FVector2D Direction2DFromFacing(ERFCharacterFacing Facing);

	/** Swaps the flipbook to the current stance/facing combination. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Art")
	void RefreshFlipbookForState();

	// ------------------------------ Components -------------------------------

	/** Subsystem accessors used by UI and the AI. */
	URFWeightComponent* GetWeightComponent() const { return WeightComponent; }
	URFInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	URFSurvivalComponent* GetSurvivalComponent() const { return SurvivalComponent; }

	/** Equips a weapon actor and hides the weapon sprite when it is dropped. */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Combat")
	void EquipWeapon(URFWeaponBase* NewWeapon);

	/** Currently held weapon, may be null (unarmed / melee only). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Combat")
	URFWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

protected:
	/** Thin planar collision box; the soldier occupies a strip of the plane only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Components")
	TObjectPtr<UBoxComponent> PlaneCollision;

	/** Orthographic XY-plane camera used by the 2D campaign maps. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Components")
	TObjectPtr<UCameraComponent> Camera;

	/** Carried-weight bookkeeping and load banding. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URFWeightComponent> WeightComponent;

	/** Slot-based inventory matching classes.json slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URFInventoryComponent> InventoryComponent;

	/** Food/hydration/stamina/temperature/fatigue model. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URFSurvivalComponent> SurvivalComponent;

	/** Base walk speed in cm/s (600 cm/s = 6 m/s, a loaded infantry jog). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Movement")
	float BaseWalkSpeedCms = 220.0f;

	/** Sprint multiplier applied on top of the load band (contract-free design value). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Movement")
	float SprintMultiplier = 1.9f;

	/** Speed multiplier while crouched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Movement")
	float CrouchSpeedMultiplier = 0.55f;

	/** Speed multiplier while prone (crawl). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Movement")
	float ProneSpeedMultiplier = 0.28f;

	/** Current stance, including the transient Sprint overlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Movement")
	ERFStance Stance = ERFStance::Stand;

	/** Stance the soldier returns to when the sprint key is released. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Movement")
	ERFStance BaseStance = ERFStance::Stand;

	/** Current 8-direction facing bucket. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Art")
	ERFCharacterFacing Facing = ERFCharacterFacing::East;

	/** Flipbooks indexed by facing bucket; the animation set supplies 8 entries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Art")
	TArray<TObjectPtr<UPaperFlipbook>> FacingFlipbooks;

private:
	/** Aim point on the gameplay plane, world units (cm). */
	FVector AimWorldLocation = FVector::ZeroVector;

	/** Cached planar move direction from the last input frame. */
	FVector2D MoveInput2D = FVector2D::ZeroVector;

	/** True while the sprint input is held. */
	bool bSprintRequested = false;

	/** Held weapon, owned by the inventory component. */
	UPROPERTY(Transient)
	TObjectPtr<URFWeaponBase> CurrentWeapon;

	/** Applies load band + stance + mobility modifiers to MaxWalkSpeed. */
	void ApplyMovementSpeed();
};
