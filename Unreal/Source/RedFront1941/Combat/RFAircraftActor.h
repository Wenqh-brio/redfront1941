// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RFDataTypes.h"
#include "GameFramework/Actor.h"
#include "RFAircraftActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Contract-driven 2D air unit that makes a strafing or bomb run across the map. */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFAircraftActor : public AActor
{
	GENERATED_BODY()

public:
	ARFAircraftActor();

	void InitializeFromDefinition(const FRFAirUnitDef& InDefinition, ERFFaction PlayerFaction,
		float InitialAttackDelayS = 4.0f);
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "RedFront|Air")
	FName GetAircraftId() const { return AircraftId; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Air")
	float GetHealth() const { return Health; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	TObjectPtr<UStaticMeshComponent> FuselageMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	TObjectPtr<UStaticMeshComponent> WingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Air")
	TObjectPtr<UTextRenderComponent> Nameplate;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Air")
	FName AircraftId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Air")
	float Health = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Air")
	float BombRadiusM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Air")
	bool bFriendlyToPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Air")
	bool bStrafes = false;
	bool bHasSiren = false;

	bool bBombReleased = false;
	bool bFlightStarted = false;
	bool bHasAttacked = false;
	bool bWarningSent = false;
	FVector2D FlightDirection = FVector2D::ZeroVector;
	float AttackCooldownS = 4.0f;

	AActor* FindTarget() const;
	void MakeAttackPass(AActor* Target);
};
