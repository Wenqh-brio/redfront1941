// Copyright RedFront1941. All Rights Reserved.

#include "Survival/RFWeightComponent.h"

#include "Core/RFCharacter.h"
#include "Survival/RFInventoryComponent.h"

URFWeightComponent::URFWeightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	EnsureDefaultLoadBands();
}

void URFWeightComponent::EnsureDefaultLoadBands()
{
	if (LoadBands.Num() > 0)
	{
		return;
	}

	// Contract table, transcribed verbatim from classes.json weight_system.load_bands.
	// Kept as a literal so the component is correct before any JSON has been parsed.
	FRFWeightBand Light;
	Light.Id = FName(TEXT("light"));
	Light.MaxRatio = 0.6f;
	Light.SpeedMult = 1.08f;
	Light.StaminaDrain = 0.8f;

	FRFWeightBand Standard;
	Standard.Id = FName(TEXT("standard"));
	Standard.MaxRatio = 0.85f;
	Standard.SpeedMult = 1.0f;
	Standard.StaminaDrain = 1.0f;

	FRFWeightBand Heavy;
	Heavy.Id = FName(TEXT("heavy"));
	Heavy.MaxRatio = 1.0f;
	Heavy.SpeedMult = 0.92f;
	Heavy.StaminaDrain = 1.25f;

	FRFWeightBand Overloaded;
	Overloaded.Id = FName(TEXT("overloaded"));
	Overloaded.MaxRatio = 99.0f;
	Overloaded.SpeedMult = 0.78f;
	Overloaded.StaminaDrain = 1.7f;

	LoadBands = { Light, Standard, Heavy, Overloaded };
}

void URFWeightComponent::SetTotalWeightKg(float NewWeightKg)
{
	TotalWeightKg = FMath::Max(0.0f, NewWeightKg);
}

void URFWeightComponent::SetCapacityKg(float NewCapacityKg)
{
	CapacityKg = FMath::Max(1.0f, NewCapacityKg);
}

void URFWeightComponent::SetCapacityMultiplier(float NewMultiplier)
{
	// Passives such as scout "light_foot" or anti-tank "heavy_carry" land here.
	CapacityMultiplier = FMath::Clamp(NewMultiplier, 0.5f, 2.0f);
}

void URFWeightComponent::SetWeaponMobilityModifier(float NewModifier)
{
	WeaponMobilityMod = FMath::Clamp(NewModifier, 0.1f, 1.5f);
}

void URFWeightComponent::SetLoadBands(const TArray<FRFWeightBand>& NewBands)
{
	if (NewBands.Num() > 0)
	{
		LoadBands = NewBands;
		// Keep the table ordered by ratio so band lookup can walk it front to back.
		LoadBands.Sort([](const FRFWeightBand& A, const FRFWeightBand& B) { return A.MaxRatio < B.MaxRatio; });
	}
}

void URFWeightComponent::RecalculateWeight()
{
	// The inventory owns the concrete slot contents, so the mass always comes from it;
	// the component keeps the base kit and the derived banding.
	float Sum = BaseKitWeightKg;

	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(GetOwner()))
	{
		if (const URFInventoryComponent* Inventory = Soldier->GetInventoryComponent())
		{
			Sum += Inventory->GetTotalCarriedWeightKg();
		}
	}

	TotalWeightKg = FMath::Max(0.0f, Sum);
}

float URFWeightComponent::GetLoadRatio() const
{
	const float EffectiveCapacity = CapacityKg * CapacityMultiplier;
	return EffectiveCapacity > KINDA_SMALL_NUMBER ? TotalWeightKg / EffectiveCapacity : 0.0f;
}

ERFLoadBand URFWeightComponent::GetLoadBand() const
{
	const float Ratio = GetLoadRatio();

	if (LoadBands.IsEmpty())
	{
		if (Ratio <= 0.6f) { return ERFLoadBand::Light; }
		if (Ratio <= 0.85f) { return ERFLoadBand::Standard; }
		if (Ratio <= 1.0f) { return ERFLoadBand::Heavy; }
		return ERFLoadBand::Overloaded;
	}

	// Walk the contract table in ascending ratio order and take the first match.
	for (const FRFWeightBand& Band : LoadBands)
	{
		if (Ratio <= Band.MaxRatio)
		{
			return LoadBandFromId(Band.Id);
		}
	}
	return ERFLoadBand::Overloaded;
}

float URFWeightComponent::GetSpeedMultiplier() const
{
	return GetBandDefinition(GetLoadBand()).SpeedMult;
}

float URFWeightComponent::GetStaminaDrainMultiplier() const
{
	return GetBandDefinition(GetLoadBand()).StaminaDrain;
}

FRFWeightBand URFWeightComponent::GetBandDefinition(ERFLoadBand Band) const
{
	for (const FRFWeightBand& Candidate : LoadBands)
	{
		if (LoadBandFromId(Candidate.Id) == Band)
		{
			return Candidate;
		}
	}

	// Fallback: standard combat load (no penalty) rather than a zero multiplier.
	FRFWeightBand Standard;
	Standard.Id = FName(TEXT("standard"));
	Standard.MaxRatio = 0.85f;
	Standard.SpeedMult = 1.0f;
	Standard.StaminaDrain = 1.0f;
	return Standard;
}

ERFLoadBand URFWeightComponent::LoadBandFromId(FName BandId)
{
	if (BandId == FName(TEXT("light"))) { return ERFLoadBand::Light; }
	if (BandId == FName(TEXT("heavy"))) { return ERFLoadBand::Heavy; }
	if (BandId == FName(TEXT("overloaded"))) { return ERFLoadBand::Overloaded; }
	return ERFLoadBand::Standard;
}
