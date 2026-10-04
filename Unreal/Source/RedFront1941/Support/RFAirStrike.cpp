// Copyright RedFront1941. All Rights Reserved.

#include "Support/RFAirStrike.h"

#include "Combat/RFDamageTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ARFAirStrike::ARFAirStrike()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
}

ARFAirStrike* ARFAirStrike::RequestAirSupport(const UObject* WorldContextObject, const FRFSupportCallInDef& CallIn,
	const FVector& TargetPoint, bool bTargetMarked, int32 AAUnitsInCorridor, AActor* Caller)
{
	UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// Contract requirements: marked_target, and clear_weather for the P-47.
	if (CallIn.RequiresMarkedTarget() && !bTargetMarked)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Caller;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARFAirStrike* Strike = World->SpawnActor<ARFAirStrike>(ARFAirStrike::StaticClass(), TargetPoint,
		FRotator::ZeroRotator, Params);
	if (Strike == nullptr)
	{
		return nullptr;
	}

	Strike->Mission = CallIn;
	Strike->TargetPoint = TargetPoint;
	Strike->CallerActor = Caller;
	Strike->AAThreatCount = FMath::Max(0, AAUnitsInCorridor);
	Strike->PhaseRemainingS = FMath::Max(0.0f, CallIn.DelayS);
	Strike->StrikeState = ERFAirStrikeState::Inbound;
	return Strike;
}

void ARFAirStrike::ResolveFlakInterception()
{
	// Flak 36 (targets_aircraft, threat 6) is the main threat; each additional AA unit in
	// the approach corridor adds 12% interception chance, capped at 65% so air support is
	// never a wasted CP spend by default.
	const float Chance = FMath::Clamp(0.12f * static_cast<float>(AAThreatCount), 0.0f, 0.65f);
	if (FMath::FRand() < Chance)
	{
		StrikeState = ERFAirStrikeState::Intercepted;
		// The aircraft never delivers ordnance; the actor cleans up after the egress beat.
		SetLifeSpan(6.0f);
	}
}

void ARFAirStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PhaseRemainingS -= DeltaSeconds;
	if (PhaseRemainingS > 0.0f)
	{
		return;
	}

	switch (StrikeState)
	{
	case ERFAirStrikeState::Intercepted:
	case ERFAirStrikeState::Aborted:
		return;

	case ERFAirStrikeState::Inbound:
		// First pass: the flak dice are rolled only now, once the aircraft is committed.
		ResolveFlakInterception();
		if (StrikeState == ERFAirStrikeState::Intercepted)
		{
			return;
		}
		StrikeState = ERFAirStrikeState::OnTarget;
		DeliverOnePass();
		PhaseRemainingS = 8.0f;	// 8 s between the two passes of a pair.
		return;

	case ERFAirStrikeState::OnTarget:
		if (PassesFlown < FMath::Max(1, Mission.Passes))
		{
			DeliverOnePass();
			PhaseRemainingS = 8.0f;
			return;
		}
		StrikeState = ERFAirStrikeState::Egressing;
		PhaseRemainingS = 5.0f;
		return;

	case ERFAirStrikeState::Egressing:
		SetLifeSpan(1.0f);
		return;

	default:
		return;
	}
}

void ARFAirStrike::DeliverOnePass()
{
	++PassesFlown;

	// One pass: a strafing/bombing run over the marked point. The radius is the contract
	// effect radius; the penetration value lets the same code work on soft skins and tanks.
	const float RadiusM = Mission.RadiusM > 0.0f ? Mission.RadiusM : 25.0f;
	RFDamage::ApplyExplosionDamage(GetWorld(), TargetPoint, RadiusM, Mission.DamagePerPass,
		Mission.ArmorPenetrationMm, CallerActor, this, RadiusM * 0.3f);
}
