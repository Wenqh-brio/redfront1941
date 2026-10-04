// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RFObjectiveTargetActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Damageable contract target used by destroy_target objectives. */
UCLASS(Blueprintable)
class REDFRONT1941_API ARFObjectiveTargetActor : public AActor
{
	GENERATED_BODY()

public:
	ARFObjectiveTargetActor();

	UFUNCTION(BlueprintCallable, Category = "RedFront|Objective")
	void InitializeTarget(FName InObjectiveId, float InMaxHealth, FName InDisplayName);

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "RedFront|Objective")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Objective")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Objective")
	FName GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "RedFront|Objective")
	bool IsDestroyed() const { return Health <= 0.0f; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RedFront|Objective")
	TObjectPtr<UTextRenderComponent> Nameplate;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objective")
	FName ObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objective")
	FName DisplayName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objective")
	float Health = 300.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RedFront|Objective")
	float MaxHealth = 300.0f;
};
