// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFWeaponBase.h"

#include "Core/RFCharacter.h"
#include "Combat/RFBallistics.h"
#include "Survival/RFInventoryComponent.h"
#include "Survival/RFSurvivalComponent.h"
#include "Survival/RFWeightComponent.h"

namespace
{
int32 CalculateResupplyRounds(int32 CurrentReserve, int32 MagazineSize,
	float CapacityMultiplier)
{
	if (MagazineSize <= 0 || !FMath::IsFinite(CapacityMultiplier) || CapacityMultiplier <= 0.0f)
	{
		return 0;
	}
	const int32 MaximumReserve = FMath::RoundToInt(
		static_cast<float>(MagazineSize * 2) * CapacityMultiplier);
	return FMath::Max(0, MaximumReserve - CurrentReserve);
}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFWeaponResupplyCapacityTest,
	"RedFront.Supply.AmmoCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFWeaponResupplyCapacityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Resupply fills a two-magazine reserve"),
		CalculateResupplyRounds(10, 30, 1.0f), 50);
	TestEqual(TEXT("An ammo pouch extends reserve capacity"),
		CalculateResupplyRounds(60, 30, 1.4f), 24);
	TestEqual(TEXT("A full reserve does not create ammunition"),
		CalculateResupplyRounds(60, 30, 1.0f), 0);
	TestEqual(TEXT("Invalid weapon capacities yield no ammunition"),
		CalculateResupplyRounds(0, 0, 1.0f), 0);
	return true;
}
#endif

URFWeaponBase::URFWeaponBase()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URFWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyOwnerModifiers();
}

void URFWeaponBase::InitializeFromDefinition(const FRFWeaponDef& InDefinition)
{
	Definition = InDefinition;

	// A fresh weapon ships with a full magazine and a two-magazine reserve basis;
	// resupply points and the ammo_pouch item extend the reserve at mission start.
	MagazineAmmo = FMath::Max(0, Definition.Magazine);
	ReserveAmmo = FMath::Max(0, Definition.Magazine * 2);

	// Default fire mode follows the contract rate of fire: cyclic weapons are full auto,
	// bolt/pump/single-shot weapons fire one round per input.
	if (Definition.Category == ERFWeaponCategory::RifleBolt || Definition.Category == ERFWeaponCategory::Sniper)
	{
		FireMode = ERFFireMode::BoltAction;
	}
	else if (Definition.Category == ERFWeaponCategory::Shotgun)
	{
		FireMode = ERFFireMode::Pump;
	}
	else if (Definition.Rpm >= 400.0f)
	{
		FireMode = ERFFireMode::FullAuto;
	}
	else
	{
		FireMode = ERFFireMode::Single;
	}

	ApplyOwnerModifiers();
}

void URFWeaponBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Reload timer first: a reload blocks firing for its whole duration.
	if (bReloading)
	{
		ReloadRemainingS -= DeltaTime;
		if (ReloadRemainingS <= 0.0f)
		{
			bReloading = false;
			ReloadRemainingS = 0.0f;

			// Magazine change: rounds come from the reserve pool of the same ammo_type.
			const int32 Needed = FMath::Max(0, Definition.Magazine - MagazineAmmo);
			const int32 Taken = FMath::Min(Needed, ReserveAmmo);
			MagazineAmmo += Taken;
			ReserveAmmo -= Taken;
		}
		return;
	}

	TimeUntilNextShotS = FMath::Max(0.0f, TimeUntilNextShotS - DeltaTime);

	if (bFiring && CanFireNow())
	{
		if (FireMode == ERFFireMode::FullAuto)
		{
			// Automatic fire drains the magazine as fast as the cyclic rate allows.
			FireOneRound();
		}
	}
}

float URFWeaponBase::GetShotIntervalS() const
{
	// 60 / rpm; the flamethrower (rpm 0) falls back to a 1 s "shot" so it still works.
	if (Definition.Rpm > KINDA_SMALL_NUMBER)
	{
		return 60.0f / Definition.Rpm;
	}
	return 1.0f;
}

void URFWeaponBase::StartFire()
{
	bFiring = true;

	// Single-shot, bolt and pump actions fire immediately on the input edge and then
	// wait for the next edge; only full auto runs on the tick loop.
	if (CanFireNow() && FireMode != ERFFireMode::FullAuto)
	{
		FireOneRound();
	}
}

void URFWeaponBase::StopFire()
{
	bFiring = false;
}

bool URFWeaponBase::CanFireNow() const
{
	if (bReloading || TimeUntilNextShotS > 0.0f)
	{
		return false;
	}
	if (MagazineAmmo <= 0)
	{
		return false;
	}
	if (bBipodDeployed && !bAimingDownSights)
	{
		// A deployed bipod may still fire from the hip, but the spread penalty is handled
		// in ComputeCurrentSpreadDeg rather than blocking the shot.
		return true;
	}
	return true;
}

void URFWeaponBase::FireOneRound()
{
	if (MagazineAmmo <= 0)
	{
		// Dry fire: the input is consumed and the soldier must reload.
		StopFire();
		return;
	}

	--MagazineAmmo;
	TimeUntilNextShotS = GetShotIntervalS();

	// One round is represented as one ballistic trace along the gameplay plane; the
	// actual trace, falloff and penetration are resolved by RFBallistics::ResolveShot
	// so damage maths stays in one testable place.
	if (const ARFCharacter* Soldier = GetOwningSoldier())
	{
		const FVector Origin = Soldier->GetActorLocation();
		const FVector2D AimDir = Soldier->GetAimDirection2D();
		const float SpreadDeg = ComputeCurrentSpreadDeg();

		RFBallistics::ResolveShot(Origin, AimDir, SpreadDeg, Definition, Soldier, this);
	}
}

bool URFWeaponBase::StartReload()
{
	if (bReloading || MagazineAmmo >= Definition.Magazine || ReserveAmmo <= 0)
	{
		return false;
	}

	bReloading = true;
	StopFire();

	// reload_s is in seconds; a class passive (fast_reload -30%) or an ammo pouch
	// multiplies it. ReloadSpeedMultiplier > 1 means faster.
	const float Effective = Definition.ReloadS / FMath::Max(0.1f, ReloadSpeedMultiplier);
	ReloadRemainingS = Effective;
	return true;
}

void URFWeaponBase::CancelReload()
{
	bReloading = false;
	ReloadRemainingS = 0.0f;
}

void URFWeaponBase::SetMagazineAmmo(int32 NewAmmo)
{
	MagazineAmmo = FMath::Clamp(NewAmmo, 0, FMath::Max(0, Definition.Magazine));
}

void URFWeaponBase::SetReserveAmmo(int32 NewReserve)
{
	ReserveAmmo = FMath::Max(0, NewReserve);
}

int32 URFWeaponBase::ReplenishReserveAmmo()
{
	if (Definition.Magazine <= 0)
	{
		return 0;
	}

	float CapacityMultiplier = 1.0f;
	if (const ARFCharacter* Soldier = GetOwningSoldier())
	{
		if (const URFInventoryComponent* Inventory = Soldier->GetInventoryComponent())
		{
			CapacityMultiplier = Inventory->GetAmmoCapacityMultiplier();
		}
	}

	const int32 RoundsAdded = CalculateResupplyRounds(
		ReserveAmmo, Definition.Magazine, CapacityMultiplier);
	ReserveAmmo += RoundsAdded;
	return RoundsAdded;
}

void URFWeaponBase::CycleFireMode()
{
	// Only weapons with a selector can switch; a bolt-action rifle has exactly one mode.
	switch (Definition.Category)
	{
	case ERFWeaponCategory::AssaultRifle:
	case ERFWeaponCategory::SMG:
	case ERFWeaponCategory::LMG:
	case ERFWeaponCategory::HMGEmplacement:
		FireMode = (FireMode == ERFFireMode::FullAuto) ? ERFFireMode::Single : ERFFireMode::FullAuto;
		break;
	case ERFWeaponCategory::RifleSemi:
	case ERFWeaponCategory::Carbine:
	case ERFWeaponCategory::Sidearm:
		FireMode = ERFFireMode::Single;
		break;
	default:
		break;
	}
}

void URFWeaponBase::SetAimingDownSights(bool bInAiming)
{
	bAimingDownSights = bInAiming;
}

bool URFWeaponBase::SetBipodDeployed(bool bDeployed)
{
	// Only support weapons have a bipod (machine guns and the AT rifle).
	const bool bHasBipod = Definition.Category == ERFWeaponCategory::LMG
		|| Definition.Category == ERFWeaponCategory::HMGEmplacement
		|| Definition.Category == ERFWeaponCategory::ATRifle;
	if (!bHasBipod)
	{
		return false;
	}

	if (bDeployed)
	{
		// Deployment is not instant: 1.2 s by contract (machine_gunner passives).
		TimeUntilNextShotS = FMath::Max(TimeUntilNextShotS, BipodDeployS);
	}
	bBipodDeployed = bDeployed;
	return true;
}

ARFCharacter* URFWeaponBase::GetOwningSoldier() const
{
	return Cast<ARFCharacter>(GetOwner());
}

void URFWeaponBase::ApplyOwnerModifiers()
{
	ReloadSpeedMultiplier = 1.0f;

	const ARFCharacter* Soldier = GetOwningSoldier();
	if (Soldier == nullptr)
	{
		return;
	}

	// Ammo pouches make reloads 10% faster and extend the carry basis by 40%.
	if (const URFWeightComponent* Weight = Soldier->GetWeightComponent())
	{
		// The weight band does not affect reload speed, but it does affect the aim sway
		// folded into ComputeCurrentSpreadDeg.
		(void)Weight;
	}
}

float URFWeaponBase::AccuracyToBaseSpreadDeg(float Accuracy, float EffectiveRangeM)
{
	// Contract accuracy is 0-1 with 1.0 being a match-grade rifle. The mapping below
	// gives roughly 0.25 deg for the PU sniper (0.99 / 800 m) up to about 7 deg for a
	// trench gun (0.35 / 25 m), which reads correctly at the orthographic zoom levels
	// the game uses (1 world unit = 1 cm, so 1 deg is ~17 cm at 10 m).
	const float ClampedAccuracy = FMath::Clamp(Accuracy, 0.05f, 1.0f);
	const float AccuracyTerm = FMath::Pow(1.0f - ClampedAccuracy, 1.6f) * 12.0f;
	const float RangeTerm = FMath::GetMappedRangeValueClamped(
		FVector2D(25.0f, 900.0f), FVector2D(1.6f, 0.55f), EffectiveRangeM);
	return FMath::Max(0.1f, AccuracyTerm * RangeTerm * 0.25f + 0.15f);
}

float URFWeaponBase::ComputeCurrentSpreadDeg() const
{
	float Spread = AccuracyToBaseSpreadDeg(Definition.Accuracy, Definition.EffectiveRangeM);

	const ARFCharacter* Soldier = GetOwningSoldier();
	if (Soldier == nullptr)
	{
		return Spread;
	}

	// Movement: firing on the move is the single largest dispersion source.
	const FVector Velocity = Soldier->GetVelocity();
	const float Speed2D = Velocity.Size2D();
	const float BaseSpeed = FMath::Max(1.0f, Soldier->GetBaseWalkSpeed());
	const float MoveRatio = FMath::Clamp(Speed2D / BaseSpeed, 0.0f, 2.0f);
	Spread *= 1.0f + MoveRatio * 1.6f;

	// Load band: an overloaded soldier cannot hold a rifle steady.
	if (const URFWeightComponent* Weight = Soldier->GetWeightComponent())
	{
		const float BandPenalty = FMath::GetMappedRangeValueClamped(
			FVector2D(0.0f, 3.0f),
			FVector2D(1.0f, 1.6f),
			static_cast<float>(static_cast<uint8>(Weight->GetLoadBand())));
		Spread *= BandPenalty;
	}

	// Stance: crouching steadies, prone is the stable firing position, ADS tightens.
	switch (Soldier->GetStance())
	{
	case ERFStance::Prone:	Spread *= 0.55f; break;
	case ERFStance::Crouch:	Spread *= 0.75f; break;
	default: break;
	}

	if (bAimingDownSights)
	{
		Spread *= AdsSpreadMultiplier;
	}
	if (bBipodDeployed)
	{
		Spread *= BipodSpreadMultiplier;
	}

	// Stamina, blood loss and cold add sway on top of the mechanical dispersion.
	if (const URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
	{
		Spread *= Survival->GetAimSwayMultiplier();
	}

	return FMath::Max(0.05f, Spread);
}
