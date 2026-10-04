// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFVehicleActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class URFVehicleArmorComponent;

/** Runtime 2D armored target driven by an enemies.json vehicle contract. */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFVehicleActor : public AActor
{
	GENERATED_BODY()

public:
	ARFVehicleActor();

	/** Applies the vehicle contract values and creates its readable blockout silhouette. */
	void InitializeFromDefinition(const FRFVehicleDef& InDefinition, ERFFaction PlayerFaction);

	virtual void Tick(float DeltaSeconds) override;

	/** Receives damage after RFBallistics has resolved armor penetration. */
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "RedFront|Vehicle")
	FName GetVehicleId() const { return VehicleId; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Vehicle")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Vehicle")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Vehicle")
	bool IsDestroyed() const { return Health <= 0.0f; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TObjectPtr<UTextRenderComponent> Nameplate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Vehicle")
	TObjectPtr<URFVehicleArmorComponent> ArmorComponent;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	FName VehicleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	ERFFaction Faction = ERFFaction::Germany;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	float Health = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	float MoveSpeedCms = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	float EngageRangeM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	float GunPenetrationMm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	bool bFriendlyToPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Vehicle")
	bool bTargetsAircraft = false;

	float ReactionRemainingS = 0.0f;
	float ReactionDurationS = 1.2f;
	float ShotCooldownS = 0.0f;
	float GunDamage = 75.0f;
	float GunAccuracy = 0.72f;

	void TickCombat(float DeltaSeconds);
	AActor* FindCombatTarget() const;
	void FireAt(AActor* Target);
};
