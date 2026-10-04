// Copyright RedFront1941. All Rights Reserved.

#include "Survival/RFSurvivalComponent.h"

#include "Core/RFCharacter.h"
#include "Survival/RFInventoryComponent.h"
#include "Survival/RFWeightComponent.h"

URFSurvivalComponent::URFSurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

void URFSurvivalComponent::BeginPlay()
{
	Super::BeginPlay();

	// Men spawn fed and watered, and the load band multiplier is pushed in from the
	// weight component so stamina drain and movement agree on the same number.
	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(GetOwner()))
	{
		if (const URFWeightComponent* Weight = Soldier->GetWeightComponent())
		{
			SetLoadStaminaMultiplier(Weight->GetStaminaDrainMultiplier());
		}
	}
}

void URFSurvivalComponent::InitializeHealth(float NewMaxHealth)
{
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	Health = MaxHealth;
}

void URFSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsDead() || DeltaTime <= 0.0f)
	{
		return;
	}

	// Movement and use actions never stack: an eating soldier is stationary.
	TickUseAction(DeltaTime);

	const bool bMoving = PendingDistanceM > KINDA_SMALL_NUMBER;
	const float DrainScale = DifficultyDrainMultiplier;

	// --- Food and water: continuous, difficulty-scaled, and slightly faster on the move.
	const float ActivityFoodMult = bMoving ? 1.4f : 1.0f;
	Food = FMath::Clamp(Food - FoodDrainPerS * ActivityFoodMult * DrainScale * DeltaTime, 0.0f, 100.0f);
	Hydration = FMath::Clamp(Hydration - HydrationDrainPerS * ActivityFoodMult * DrainScale * DeltaTime, 0.0f, 100.0f);

	// --- Stamina: drains while moving (weighted by the load band), regenerates at rest.
	if (bSprinting && bMoving)
	{
		Stamina = FMath::Clamp(Stamina - SprintStaminaDrainPerS * LoadStaminaMultiplier * DeltaTime, 0.0f, 100.0f);
	}
	else if (bMoving)
	{
		Stamina = FMath::Clamp(Stamina - WalkStaminaDrainPerS * LoadStaminaMultiplier * DeltaTime, 0.0f, 100.0f);
	}
	else
	{
		// Fully rested men recover faster than exhausted ones (convex recovery curve).
		const float RecoveryScale = FMath::Lerp(0.5f, 1.0f, Stamina / 100.0f);
		Stamina = FMath::Clamp(Stamina + StaminaRegenPerS * RecoveryScale * DeltaTime, 0.0f, 100.0f);
	}

	// --- Fatigue: accumulates with distance, never recovers in the field.
	if (PendingDistanceM > 0.0f)
	{
		Fatigue = FMath::Clamp(Fatigue + PendingDistanceM * FatiguePerMetre, 0.0f, 100.0f);
	}
	PendingDistanceM = 0.0f;

	// --- Temperature: drifts toward ambient. 1941-42 winter levels sit below -20 C,
	//     where unwarmed men lose health through hypothermia damage.
	const float Insulation = 1.0f - FMath::Clamp(Fatigue / 200.0f, 0.0f, 0.4f);
	const float DriftPerS = 0.012f * Insulation;
	TemperatureC += (AmbientTemperatureC - TemperatureC) * DriftPerS * DeltaTime;

	// --- Consequences.
	if (Food <= 0.0f || Hydration <= 0.0f || TemperatureC < 34.0f)
	{
		ApplyDamage(StarvationDamagePerS * DeltaTime, false);
	}

	if (BloodLoss > 0.0f)
	{
		ApplyDamage(BloodLossDamagePerSPer100 * (BloodLoss / 100.0f) * DeltaTime, false);
	}
}

URFInventoryComponent* URFSurvivalComponent::GetInventory() const
{
	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(GetOwner()))
	{
		return Soldier->GetInventoryComponent();
	}
	return nullptr;
}

bool URFSurvivalComponent::BeginUseItem(int32 InventoryIndex, ERFUseAction Action, float DurationS)
{
	if (CurrentAction != ERFUseAction::None || InventoryIndex < 0 || DurationS <= 0.0f)
	{
		return false;
	}
	CurrentAction = Action;
	PendingItemIndex = InventoryIndex;
	ActionRemainingS = DurationS;
	return true;
}

void URFSurvivalComponent::TickUseAction(float DeltaTime)
{
	if (CurrentAction == ERFUseAction::None)
	{
		return;
	}

	ActionRemainingS -= DeltaTime;
	if (ActionRemainingS > 0.0f)
	{
		return;
	}

	// The action completes: consume the item and apply its effect exactly once.
	URFInventoryComponent* Inventory = GetInventory();
	if (Inventory != nullptr && PendingItemIndex >= 0)
	{
		const FRFInventoryEntry Consumed = Inventory->ConsumeFromSlot(PendingItemIndex);
		ApplyItemEffect(Consumed.Item);
	}

	CurrentAction = ERFUseAction::None;
	PendingItemIndex = -1;
	ActionRemainingS = 0.0f;

	// Consuming an item changes the carried mass (water especially: -1 kg per litre).
	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(GetOwner()))
	{
		if (URFWeightComponent* Weight = Soldier->GetWeightComponent())
		{
			Weight->RecalculateWeight();
			SetLoadStaminaMultiplier(Weight->GetStaminaDrainMultiplier());
		}
	}
}

void URFSurvivalComponent::ApplyItemEffect(const FRFItemDef& Item)
{
	switch (Item.Kind)
	{
	case ERFItemKind::Food:
		Food = FMath::Clamp(Food + Item.Calories, 0.0f, 100.0f);
		Morale = FMath::Clamp(Morale + Item.Morale, 0.0f, 100.0f);
		break;

	case ERFItemKind::Water:
		Hydration = FMath::Clamp(Hydration + (Item.Hydration > 0.0f ? Item.Hydration : Item.Capacity), 0.0f, 100.0f);
		break;

	case ERFItemKind::Bandage:
	case ERFItemKind::Medkit:
	case ERFItemKind::Stim:
		Heal(Item.Heal);
		if (Item.bStopBleed)
		{
			BloodLoss = 0.0f;
		}
		// A stimulant (morphine) suppresses pain: stamina is topped up for its duration.
		if (Item.PainIgnoreS > 0.0f)
		{
			Stamina = FMath::Clamp(Stamina + 25.0f, 0.0f, 100.0f);
		}
		break;

	default:
		break;
	}
}

bool URFSurvivalComponent::EatBestAvailableFood()
{
	URFInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	const int32 Index = Inventory->FindBestFoodIndex();
	if (Index < 0)
	{
		return false;
	}

	const FRFInventoryEntry& Entry = Inventory->GetEntries()[Index];
	// D rations (eat_time_s 2.0) are the only mobile food; everything else roots the man.
	const float Duration = Entry.Item.EatTimeS > 0.0f ? Entry.Item.EatTimeS : 5.0f;
	return BeginUseItem(Index, ERFUseAction::Eating, Duration);
}

bool URFSurvivalComponent::DrinkBestAvailableWater()
{
	URFInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	const int32 Index = Inventory->FindBestWaterIndex();
	if (Index < 0)
	{
		return false;
	}

	const FRFInventoryEntry& Entry = Inventory->GetEntries()[Index];
	const float Duration = Entry.Item.DrinkTimeS > 0.0f ? Entry.Item.DrinkTimeS : 3.5f;
	return BeginUseItem(Index, ERFUseAction::Drinking, Duration);
}

bool URFSurvivalComponent::UseBestMedicalItem()
{
	URFInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	const int32 Index = Inventory->FindBestMedicalIndex();
	if (Index < 0)
	{
		return false;
	}

	const FRFInventoryEntry& Entry = Inventory->GetEntries()[Index];
	const float Duration = Entry.Item.UseTimeS > 0.0f ? Entry.Item.UseTimeS : 7.0f;
	return BeginUseItem(Index, ERFUseAction::Medical, Duration);
}

float URFSurvivalComponent::GetStaminaDrainPerS() const
{
	const float Base = bSprinting ? SprintStaminaDrainPerS : WalkStaminaDrainPerS;
	return Base * LoadStaminaMultiplier;
}

float URFSurvivalComponent::GetMobilityMultiplier() const
{
	// Exhaustion, heavy fatigue, pain (blood loss) and cold all slow a man down.
	float Multiplier = 1.0f;
	Multiplier *= FMath::Lerp(0.72f, 1.0f, FMath::Clamp(Stamina / 100.0f, 0.0f, 1.0f));
	Multiplier *= FMath::Lerp(0.88f, 1.0f, 1.0f - FMath::Clamp(Fatigue / 100.0f, 0.0f, 1.0f) * 0.5f);
	Multiplier *= FMath::Lerp(0.80f, 1.0f, 1.0f - FMath::Clamp(BloodLoss / 100.0f, 0.0f, 1.0f) * 0.5f);

	if (TemperatureC < 35.0f)
	{
		Multiplier *= FMath::GetMappedRangeValueClamped(FVector2D(30.0f, 35.0f), FVector2D(0.7f, 1.0f), TemperatureC);
	}
	return FMath::Clamp(Multiplier, 0.4f, 1.0f);
}

float URFSurvivalComponent::GetAimSwayMultiplier() const
{
	// Weapon sway grows when the man is winded, hurt or freezing.
	float Sway = 1.0f;
	Sway *= 1.0f + (1.0f - FMath::Clamp(Stamina / 100.0f, 0.0f, 1.0f)) * 0.6f;
	Sway *= 1.0f + FMath::Clamp(BloodLoss / 100.0f, 0.0f, 1.0f) * 0.4f;
	if (TemperatureC < 35.0f)
	{
		Sway *= 1.25f;
	}
	return Sway;
}

void URFSurvivalComponent::SetLoadStaminaMultiplier(float NewMultiplier)
{
	LoadStaminaMultiplier = FMath::Clamp(NewMultiplier, 0.5f, 3.0f);
}

void URFSurvivalComponent::SetDifficultyDrainMultiplier(float NewMultiplier)
{
	DifficultyDrainMultiplier = FMath::Clamp(NewMultiplier, 0.5f, 3.0f);
}

void URFSurvivalComponent::SetAmbientTemperatureC(float NewAmbientC)
{
	AmbientTemperatureC = FMath::Clamp(NewAmbientC, -45.0f, 45.0f);
}

void URFSurvivalComponent::ReportDistanceTravelledM(float DistanceM)
{
	PendingDistanceM += FMath::Max(0.0f, DistanceM);
}

void URFSurvivalComponent::ApplyDamage(float Amount, bool bCauseBleeding)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health - Amount, 0.0f, MaxHealth);

	if (bCauseBleeding)
	{
		// Every wound leaves a bleed that must be bandaged (bandage 20 hp + stop_bleed,
		// medkit 60 hp + stop_bleed + revive).
		BloodLoss = FMath::Clamp(BloodLoss + FMath::Min(40.0f, Amount * 0.5f), 0.0f, 100.0f);
	}

	// Pain and shock cost morale and stamina immediately.
	Morale = FMath::Clamp(Morale - Amount * 0.25f, 0.0f, 100.0f);
	Stamina = FMath::Clamp(Stamina - Amount * 0.2f, 0.0f, 100.0f);
}

void URFSurvivalComponent::Heal(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}
	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
}

void URFSurvivalComponent::AddMorale(float Delta)
{
	Morale = FMath::Clamp(Morale + Delta, 0.0f, 100.0f);
}
