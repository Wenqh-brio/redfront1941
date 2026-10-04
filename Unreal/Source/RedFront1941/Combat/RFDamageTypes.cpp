// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFDamageTypes.h"

#include "Core/RFCharacter.h"
#include "Core/RFGameMode.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Survival/RFInventoryComponent.h"
#include "Survival/RFSurvivalComponent.h"

URFFirearmDamage::URFFirearmDamage()
{
	bCausedByWorld = false;
	bScaleMomentumByMass = false;
}

URFExplosiveDamage::URFExplosiveDamage()
{
	bCausedByWorld = true;
	bScaleMomentumByMass = false;
	// Artillery does not discriminate: the support subsystem still computes a danger
	// radius for friendly-fire warnings, but the damage call itself is impartial.
	bRadialDamageVelChange = false;
}

URFIncendiaryDamage::URFIncendiaryDamage()
{
	bCausedByWorld = false;
}

URFMeleeDamage::URFMeleeDamage()
{
	bCausedByWorld = false;
}

URFVehicleArmorComponent::URFVehicleArmorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URFVehicleArmorComponent::InitializeFromDefinition(const FRFVehicleDef& InDefinition)
{
	VehicleId = InDefinition.Id;
	FrontArmorMm = InDefinition.ArmorMm;
	SideRearRatio = FMath::Clamp(InDefinition.SideRearRatio, 0.05f, 1.0f);
}

float URFVehicleArmorComponent::GetSideRearArmorMm() const
{
	return FrontArmorMm * FMath::Clamp(SideRearRatio, 0.05f, 1.0f);
}

float URFVehicleArmorComponent::GetArmorForAspect(bool bSideOrRear) const
{
	return bSideOrRear ? GetSideRearArmorMm() : FrontArmorMm;
}

float RFDamage::GetActorArmorMm(const AActor* Target, bool& bOutSideOrRear)
{
	bOutSideOrRear = false;
	if (Target == nullptr)
	{
		return 0.0f;
	}

	// Vehicles and emplaced guns: armour comes from the contract-backed component.
	if (const URFVehicleArmorComponent* Armor = Target->FindComponentByClass<URFVehicleArmorComponent>())
	{
		// The aspect is decided by the caller through the impact point; here we report
		// the frontal value and flag that a side/rear hit needs the scaled lookup.
		bOutSideOrRear = Armor->GetSideRearArmorMm() < Armor->GetFrontArmorMm();
		return Armor->GetFrontArmorMm();
	}

	// Soldiers: body armour from equipment.json is thin plate, converted to an RHA
	// equivalent. The SN-42 breastplate (torso_protection 0.55) is worth about 14 mm,
	// enough to stop 7.62x25 at range but not a rifle round.
	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(Target))
	{
		if (const URFInventoryComponent* Inventory = Soldier->GetInventoryComponent())
		{
			float ProtectionScore = 0.0f;
			for (const FRFInventoryEntry& Entry : Inventory->GetEntries())
			{
				if (Entry.bIsWeapon || Entry.Item.Category != ERFItemCategory::Armor)
				{
					continue;
				}
				// Head and torso protection are both folded in at a 25 mm-per-unit scale.
				ProtectionScore += Entry.Item.MobilityPenalty < 1.0f ? 0.55f : 0.2f;
			}
			return ProtectionScore * 25.0f;
		}
	}

	// Soft target (sandbags, wooden cover, unarmoured trucks).
	return 0.0f;
}

float RFDamage::GetImpactAngleDeg(const FVector& ShotDirection, const FVector& ImpactNormal)
{
	if (ShotDirection.IsNearlyZero() || ImpactNormal.IsNearlyZero())
	{
		return 0.0f;
	}

	// Angle between the surface normal and the incoming direction: 0 degrees means the
	// round arrives perpendicular to the plate (best case for penetration).
	const float Dot = FMath::Clamp(FVector::DotProduct(ShotDirection.GetSafeNormal(), -ImpactNormal.GetSafeNormal()), -1.0f, 1.0f);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

float RFDamage::ApplyBallisticDamage(UWorld* World, AActor* Target, float Amount, bool bBleeding,
	const FVector& ImpactPoint, const FVector& ShotDirection, const AActor* Shooter,
	const UObject* DamageCauser, float PenetrationMm)
{
	if (World == nullptr || Target == nullptr || Amount <= 0.0f)
	{
		return 0.0f;
	}

	// Characters route through the survival component so bleeding, morale and stamina
	// react to the wound; everything else takes engine damage directly.
	if (ARFCharacter* Soldier = Cast<ARFCharacter>(Target))
	{
		if (URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
		{
			const bool bWasDead = Survival->IsDead();
			Survival->ApplyDamage(Amount, bBleeding);
			if (!bWasDead && Survival->IsDead() && Soldier->ActorHasTag(FName(TEXT("RF_Enemy"))))
			{
				for (const FName& Tag : Soldier->Tags)
				{
					const FString TagString = Tag.ToString();
					if (TagString.StartsWith(TEXT("RF_EnemyId:")))
					{
						if (ARFGameMode* GameMode = World->GetAuthGameMode<ARFGameMode>())
						{
							GameMode->NotifyEnemyEliminated(FName(*TagString.RightChop(11)));
						}
						break;
					}
				}
			}
			return Amount;
		}
	}

	TSubclassOf<UDamageType> DamageType = bBleeding
		? URFFirearmDamage::StaticClass()
		: URFExplosiveDamage::StaticClass();

	// ApplyDamage needs a non-const victim; the trace hands back a mutable actor.
	AController* InstigatorController = nullptr;
	if (const APawn* ShooterPawn = Cast<APawn>(Shooter))
	{
		InstigatorController = ShooterPawn->GetController();
	}

	// ApplyPointDamage requires an actor as its damage causer; weapon components are
	// valid damage sources too, so use the instigating pawn for consistent attribution.
	AActor* DamageCauserActor = InstigatorController != nullptr ? InstigatorController->GetPawn() : nullptr;
	return UGameplayStatics::ApplyPointDamage(Target, Amount, ShotDirection.GetSafeNormal(),
		FHitResult(), InstigatorController, DamageCauserActor, DamageType);
}

int32 RFDamage::ApplyExplosionDamage(UWorld* World, const FVector& Center, float RadiusM,
	float Damage, float PenetrationMm, const AActor* Shooter, const UObject* DamageCauser,
	float FullDamageM)
{
	if (World == nullptr || RadiusM <= 0.0f || Damage <= 0.0f)
	{
		return 0;
	}

	// Default model: full damage inside 35% of the radius, linear falloff to 15% at the
	// edge. This matches the contract's "fragment radius with falloff" description for
	// F-1 (9 m / 22 fragments) and the 82 mm mortar (18 m).
	const float FullRadius = FullDamageM > 0.0f ? FullDamageM : RadiusM * 0.35f;
	const float RadiusCm = RadiusM * 100.0f;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RFExplosion), false, Shooter);
	Params.bReturnPhysicalMaterial = false;

	World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(RadiusCm), Params);

	int32 DamagedCount = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (Victim == nullptr)
		{
			continue;
		}

		const float DistanceCm = FVector::Dist(Center, Victim->GetActorLocation());
		const float DistanceM = DistanceCm / 100.0f;

		bool bSideOrRear = false;
		const float ArmorMm = GetActorArmorMm(Victim, bSideOrRear);

		// Fragments are stopped by armour the same way bullets are, but the blast itself
		// still concusses: 15% of the nominal damage always lands inside the radius.
		const float Falloff = DistanceM <= FullRadius
			? 1.0f
			: FMath::Lerp(1.0f, 0.15f, FMath::Clamp((DistanceM - FullRadius) / FMath::Max(0.01f, RadiusM - FullRadius), 0.0f, 1.0f));

		float Applied = Damage * Falloff;
		if (ArmorMm > PenetrationMm)
		{
			Applied *= 0.2f;
		}

		ApplyBallisticDamage(World, Victim, Applied, true, Victim->GetActorLocation(),
			(Victim->GetActorLocation() - Center).GetSafeNormal(), Shooter, DamageCauser, PenetrationMm);
		++DamagedCount;
	}
	return DamagedCount;
}
