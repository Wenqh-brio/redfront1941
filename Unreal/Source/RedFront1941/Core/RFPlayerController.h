// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/RFDataTypes.h"
#include "RFPlayerController.generated.h"

class ARFCharacter;
class ARFGameState;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Player controller for the 2D shooter.
 *
 * Responsibilities:
 *   * Enhanced Input setup (IMC_RF_Gameplay + Input Actions) with a legacy
 *     axis/action fallback declared in Config/DefaultInput.ini.
 *   * Mouse-to-world aim: the cursor is deprojected onto the gameplay plane at
 *     Z = 0 so the soldier aims where the player points, at any camera zoom.
 *   * Hold-to-cook grenades: press starts the contract fuse, release throws.
 *
 * Units: aim points are world centimetres on the XY plane; delays/cooldowns are seconds.
 */
UCLASS()
class REDFRONT1941_API ARFPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARFPlayerController();

	/** Pushes the gameplay mapping context and caches the pawn. */
	virtual void BeginPlay() override;

	/** Binds the Enhanced Input actions (or the legacy fallback). */
	virtual void SetupInputComponent() override;

	/** Updates the aim point from the cursor each frame. */
	virtual void PlayerTick(float DeltaTime) override;

	// -------------------------------- Aim -----------------------------------

	/**
	 * Deprojects a viewport-space position onto the gameplay plane (Z = PlaneZ).
	 * @param ScreenPosition Cursor position in pixels (viewport space).
	 * @return World position on the plane, cm; falls back to the pawn's location when
	 *         the view ray is parallel to the plane (camera looking straight down).
	 */
	UFUNCTION(BlueprintCallable, Category = "RedFront|Aim")
	FVector DeprojectScreenToPlane(const FVector2D& ScreenPosition) const;

	/** Latest aim point on the gameplay plane, cm. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Aim")
	FVector GetAimWorldLocation() const { return AimWorldLocation; }

	/** True while the grenade fuse is cooking (Grenade_Throw held). */
	UFUNCTION(BlueprintPure, Category = "RedFront|Combat")
	bool IsCookingGrenade() const { return bCookingGrenade; }

	/** Seconds remaining on the cooked fuse; 0 when no grenade is cooking. */
	UFUNCTION(BlueprintPure, Category = "RedFront|Combat")
	float GetCookTimeRemainingS() const { return CookTimeRemainingS; }

protected:
	// --------------------------- Enhanced Input ------------------------------

	/** IMC_RF_Gameplay: pushed on possess, popped while a full-screen UI is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	/** IMC_RF_Support: radio/support wheel, pushed only while the wheel is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input")
	TObjectPtr<UInputMappingContext> SupportMappingContext;

	/** IMC_RF_Map: map board and squad-order panel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input")
	TObjectPtr<UInputMappingContext> MapMappingContext;

	/** Mapping context priority; the map context wins over gameplay while open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input")
	int32 BaseMappingPriority = 0;

	// Combat / interaction actions (asset references, one per DefaultInput action name).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Fire;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_AimDownSights;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Reload;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_SwapWeapon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_GrenadeThrow;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Melee;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Interact;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Binoculars;

	// Movement actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Move;
	/** Optional gamepad aim action; the mouse cursor is the primary aim source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Aim;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Sprint;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Crouch;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Prone;

	// Survival actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Eat;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_Drink;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_UseMedical;

	// Support / squad / map actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_CallSupportArtillery;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_CallSupportArmor;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_CallSupportAir;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_SupportConfirm;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_OpenMap;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_SquadAdvance;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_SquadHold;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input|Actions") TObjectPtr<UInputAction> IA_EndlessNextWave;

	/** Plane height in cm (Z) that the cursor is deprojected onto; matches RFCharacter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Aim")
	float PlaneZ = 44.0f;

	/** Maximum planar distance for securing an objective marker, centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Objectives", meta = (ClampMin = "1"))
	float ObjectiveInteractRangeCm = 220.0f;

	/** Idle fuse used when no grenade specific fuse has been resolved yet, seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Combat")
	float DefaultGrenadeFuseS = 3.5f;

	/** True when the legacy axis/action bindings should also be installed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RedFront|Input")
	bool bInstallLegacyFallbackBindings = true;

	// --------------------------- Enhanced handlers ---------------------------

	virtual void OnMove(const FInputActionValue& Value);
	virtual void OnMoveCompleted(const FInputActionValue& Value);
	virtual void OnAim(const FInputActionValue& Value);
	virtual void OnFirePressed(const FInputActionValue& Value);
	virtual void OnFireReleased(const FInputActionValue& Value);
	virtual void OnAimDownSights(const FInputActionValue& Value);
	virtual void OnReload(const FInputActionValue& Value);
	virtual void OnSwapWeapon(const FInputActionValue& Value);
	virtual void OnGrenadeThrowPressed(const FInputActionValue& Value);
	virtual void OnGrenadeThrowReleased(const FInputActionValue& Value);
	virtual void OnMelee(const FInputActionValue& Value);
	virtual void OnInteract(const FInputActionValue& Value);
	virtual void OnBinoculars(const FInputActionValue& Value);
	virtual void OnSprintPressed(const FInputActionValue& Value);
	virtual void OnSprintReleased(const FInputActionValue& Value);
	virtual void OnCrouch(const FInputActionValue& Value);
	virtual void OnProne(const FInputActionValue& Value);
	virtual void OnEat(const FInputActionValue& Value);
	virtual void OnDrink(const FInputActionValue& Value);
	virtual void OnUseMedical(const FInputActionValue& Value);
	virtual void OnCallArtillery(const FInputActionValue& Value);
	virtual void OnCallArmor(const FInputActionValue& Value);
	virtual void OnCallAir(const FInputActionValue& Value);
	virtual void OnCallSupport(const FInputActionValue& Value, ERFSupportCallType CallType);
	virtual void OnSupportConfirm(const FInputActionValue& Value);
	virtual void OnOpenMap(const FInputActionValue& Value);
	virtual void OnSquadAdvance(const FInputActionValue& Value);
	virtual void OnSquadHold(const FInputActionValue& Value);
	virtual void OnEndlessNextWave(const FInputActionValue& Value);

	// ------------------------- Legacy axis/action fallback -------------------

	void LegacyMoveForward(float Value);
	void LegacyMoveRight(float Value);
	void LegacyFirePressed();
	void LegacyFireReleased();
	void LegacyInteractPressed();
	void LegacyEndlessNextWavePressed();

	/** Cached controlled soldier; refreshed when possession changes. */
	UPROPERTY(Transient)
	TObjectPtr<ARFCharacter> RFPawn;

	/** Cached game state pointer, used by the squad-order and endless bindings. */
	UPROPERTY(Transient)
	TObjectPtr<ARFGameState> RFGameState;

	/** Current aim point on the plane, cm. */
	FVector AimWorldLocation = FVector::ZeroVector;

	/** True while the fire input is held. */
	bool bFiring = false;

	/** True while the ADS input is held. */
	bool bAimingDownSights = false;

	/** True while the grenade fuse is cooking. */
	bool bCookingGrenade = false;

	/** Legacy-path cache: planar move vector split across the Move/MoveRight axes. */
	FVector2D LegacyMoveAxis = FVector2D::ZeroVector;

	/** Support call-in type selected by the last CallSupport_* press. */
	ERFSupportCallType PendingSupportType = ERFSupportCallType::Indirect;

	/** True when a forward observer has line of sight to the pending support target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Support")
	bool bObserverHasLineOfSight = true;

	/** True when the observer carries binoculars (artillery spread -35%). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RedFront|Support")
	bool bObserverHasBinoculars = false;

	/** Fuse time of the grenade currently being cooked, seconds. */
	float CookFuseS = 3.5f;

	/** Remaining cook time before the grenade must be thrown, seconds. */
	float CookTimeRemainingS = 0.0f;

	/** Recomputes AimWorldLocation from the current cursor position. */
	void UpdateAimFromCursor();

	/** Returns the controlled RF character, or null. */
	ARFCharacter* GetRFPawn() const;
};
