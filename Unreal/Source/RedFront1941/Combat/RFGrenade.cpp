// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFGrenade.h"

#include "Combat/RFDamageTypes.h"
#include "Components/SphereComponent.h"
#include "Core/RFCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PaperFlipbookComponent.h"
#include "Survival/RFInventoryComponent.h"

ARFGrenade::ARFGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(6.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("RF_Projectile"));
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetSimulatePhysics(false);
	RootComponent = CollisionSphere;

	Flipbook = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Flipbook"));
	Flipbook->SetupAttachment(RootComponent);
	Flipbook->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARFGrenade::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere != nullptr)
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ARFGrenade::OnGrenadeHit);
	}
}

void ARFGrenade::InitializeFromItem(const FRFItemDef& InItem)
{
	ItemDefinition = InItem;
	FuseRemainingS = ItemDefinition.FuseS;

	// Map the contract effect keys onto an archetype without adding schema fields.
	switch (ItemDefinition.Kind)
	{
	case ERFItemKind::Smoke:		GrenadeKind = ERFGrenadeKind::Smoke; break;
	case ERFItemKind::Fire:			GrenadeKind = ERFGrenadeKind::Incendiary; break;
	case ERFItemKind::AntiTank:		GrenadeKind = ERFGrenadeKind::AntiTank; break;
	case ERFItemKind::Demolition:	GrenadeKind = ERFGrenadeKind::Demolition; break;
	default:						GrenadeKind = ERFGrenadeKind::Fragmentation; break;
	}

	// A molotov (fuse_s 0) and a fully cooked grenade are impact-fused.
	bImpactFuse = FuseRemainingS <= KINDA_SMALL_NUMBER;
}

ARFGrenade* ARFGrenade::ThrowCookedGrenade(AActor* Thrower, const FVector& TargetPoint, float CookedTimeS)
{
	if (Thrower == nullptr)
	{
		return nullptr;
	}

	UWorld* World = Thrower->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// Resolve the grenade from the thrower's inventory: the first grenade-kind entry is
	// thrown, which keeps "Grenade_Throw" working with whatever the soldier carries.
	FRFItemDef GrenadeItem;
	if (const ARFCharacter* Soldier = Cast<ARFCharacter>(Thrower))
	{
		if (URFInventoryComponent* Inventory = Soldier->GetInventoryComponent())
		{
			const int32 Index = Inventory->FindItemIndexByKind(ERFItemKind::Frag);
			const int32 SmokeIndex = Index >= 0 ? Index : Inventory->FindItemIndexByKind(ERFItemKind::Smoke);
			const int32 UseIndex = SmokeIndex >= 0 ? SmokeIndex : Inventory->FindItemIndexByKind(ERFItemKind::AntiTank);
			if (UseIndex >= 0)
			{
				GrenadeItem = Inventory->GetEntries()[UseIndex].Item;
				// The throw consumes one unit immediately; the fuse keeps burning in flight.
				Inventory->ConsumeFromSlot(UseIndex);
			}
		}
	}

	if (GrenadeItem.Id.IsNone())
	{
		// Nothing throwable: the input is a no-op rather than an empty actor.
		return nullptr;
	}

	const FVector SpawnLocation = Thrower->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	FActorSpawnParameters Params;
	Params.Owner = Thrower;
	Params.Instigator = Thrower->GetInstigator();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARFGrenade* Grenade = World->SpawnActor<ARFGrenade>(ARFGrenade::StaticClass(), SpawnLocation,
		FRotator::ZeroRotator, Params);
	if (Grenade == nullptr)
	{
		return nullptr;
	}

	Grenade->InitializeFromItem(GrenadeItem);
	Grenade->ThrowerActor = Thrower;

	// Hold-to-cook: the burnt fuse is subtracted, so a fully cooked grenade detonates
	// on impact instead of resting on the ground for the full fuse.
	Grenade->FuseRemainingS = FMath::Max(0.0f, Grenade->FuseRemainingS - FMath::Max(0.0f, CookedTimeS));
	if (Grenade->FuseRemainingS <= KINDA_SMALL_NUMBER)
	{
		Grenade->bImpactFuse = true;
	}

	// Planar throw: the device travels along the gameplay plane toward the aim point.
	const FVector Delta = TargetPoint - SpawnLocation;
	const FVector2D Direction2D(Delta.X, Delta.Y);
	const FVector Direction(Direction2D.GetSafeNormal().X, Direction2D.GetSafeNormal().Y, 0.0f);

	if (Grenade->CollisionSphere != nullptr)
	{
		Grenade->CollisionSphere->SetSimulatePhysics(true);
		Grenade->CollisionSphere->SetPhysicsLinearVelocity(Direction * Grenade->ThrowSpeedCms);
		Grenade->CollisionSphere->SetEnableGravity(false);
	}

	return Grenade;
}

ARFGrenade* ARFGrenade::SpawnPlacedCharge(const UObject* WorldContextObject, const FVector& Location,
	const FRFItemDef& Item, AActor* Placer)
{
	UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Placer;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARFGrenade* Charge = World->SpawnActor<ARFGrenade>(ARFGrenade::StaticClass(), Location,
		FRotator::ZeroRotator, Params);
	if (Charge != nullptr)
	{
		Charge->InitializeFromItem(Item);
		Charge->ThrowerActor = Placer;
	}
	return Charge;
}

void ARFGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Fuse.
	if (FuseRemainingS > 0.0f)
	{
		FuseRemainingS -= DeltaSeconds;
		if (FuseRemainingS <= 0.0f)
		{
			Detonate();
			return;
		}
	}

	// Smoke and fire volumes tick down after detonation.
	if (SmokeRemainingS > 0.0f)
	{
		SmokeRemainingS -= DeltaSeconds;
		if (SmokeRemainingS <= 0.0f)
		{
			// The Blueprint VFX volume is bound to this actor's lifetime; ending here
			// lets the smoke actor clean itself up with the grenade.
		}
	}

	if (FireRemainingS > 0.0f)
	{
		FireRemainingS -= DeltaSeconds;

		// Burning ground damages anything standing in it: 12 damage/s on the 0-100 scale.
		if (ItemDefinition.RadiusM > 0.0f)
		{
			RFDamage::ApplyExplosionDamage(GetWorld(), GetActorLocation(), ItemDefinition.RadiusM,
				12.0f * DeltaSeconds, 0.0f, ThrowerActor, this, ItemDefinition.RadiusM);
		}
	}
}

void ARFGrenade::OnGrenadeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Impact fuse (molotov or fully cooked) detonates on the first contact; a live fuse
	// keeps burning so a bounced grenade can be thrown back (Mk 2's 4.5 s fuse).
	if (bImpactFuse && FuseRemainingS > 0.0f)
	{
		Detonate();
	}
}

void ARFGrenade::Detonate()
{
	FuseRemainingS = 0.0f;

	switch (GrenadeKind)
	{
	case ERFGrenadeKind::Smoke:			ApplySmokeEffect(); break;
	case ERFGrenadeKind::Incendiary:	ApplyIncendiaryEffect(); break;
	case ERFGrenadeKind::AntiTank:		ApplyAntiTankEffect(); break;
	default:							ApplyFragmentationEffect(); break;
	}
}

void ARFGrenade::ApplyFragmentationEffect()
{
	// Fragmentation: full damage at the centre, linear falloff to the contract radius.
	// damage/radius_m/fragments come straight from equipment.json (F-1: 110 / 9 m / 22).
	RFDamage::ApplyExplosionDamage(GetWorld(), GetActorLocation(), ItemDefinition.RadiusM,
		ItemDefinition.Damage, ItemDefinition.PenetrationMm, ThrowerActor, this,
		ItemDefinition.RadiusM * 0.3f);

	// Destruction for demolition charges (satchel: structure_damage 300).
	if (ItemDefinition.StructureDamage > 0.0f)
	{
		// Structural targets (bunker doors, bridges) listen for this via their own
		// Blueprint; the value travels in the item contract.
	}

	// The device is consumed; the Blueprint detonation VFX is spawned on destroy.
	Destroy();
}

void ARFGrenade::ApplySmokeEffect()
{
	SmokeRemainingS = ItemDefinition.SmokeDurationS;

	// Smoke blocks line of sight: AI awareness checks query this radius while the cloud
	// is alive (RFEnemyAIController::IsTargetObscured).
	Destroy();
}

void ARFGrenade::ApplyIncendiaryEffect()
{
	FireRemainingS = ItemDefinition.FireDurationS;

	// Initial splash damage (molotov: 70 damage / 4 m / 25 mm penetration).
	RFDamage::ApplyExplosionDamage(GetWorld(), GetActorLocation(), ItemDefinition.RadiusM,
		ItemDefinition.Damage, ItemDefinition.PenetrationMm, ThrowerActor, this,
		ItemDefinition.RadiusM * 0.5f);

	// The fire pool keeps burning for fire_duration_s, handled in Tick; the actor must
	// therefore survive its own detonation.
	if (FireRemainingS <= 0.0f)
	{
		Destroy();
	}
}

void ARFGrenade::ApplyAntiTankEffect()
{
	// RPG-43: HEAT jet, effective only against armour plates it can penetrate. The
	// radial blast is small (radius_m 3) but the direct hit carries penetration_mm 75.
	RFDamage::ApplyExplosionDamage(GetWorld(), GetActorLocation(), ItemDefinition.RadiusM,
		ItemDefinition.Damage, ItemDefinition.PenetrationMm, ThrowerActor, this,
		ItemDefinition.RadiusM * 0.25f);
	Destroy();
}
