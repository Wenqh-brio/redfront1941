// Copyright RedFront1941. All Rights Reserved.

#include "Support/RFArtilleryStrike.h"

#include "Combat/RFDamageTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ARFArtilleryStrike::ARFArtilleryStrike()
{
	PrimaryActorTick.bCanEverTick = true;
	// The strike is a controller-like actor: it needs no collision of its own.
	SetActorEnableCollision(false);
}

ARFArtilleryStrike* ARFArtilleryStrike::StartFireMission(const UObject* WorldContextObject,
	const FRFSupportCallInDef& CallIn, const FVector& TargetPoint, float SpreadM, AActor* Caller)
{
	UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = Caller;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARFArtilleryStrike* Strike = World->SpawnActor<ARFArtilleryStrike>(ARFArtilleryStrike::StaticClass(),
		TargetPoint, FRotator::ZeroRotator, Params);
	if (Strike == nullptr)
	{
		return nullptr;
	}

	Strike->Mission = CallIn;
	Strike->TargetPoint = TargetPoint;
	Strike->SpreadM = FMath::Max(0.0f, SpreadM);
	Strike->CallerActor = Caller;

	// Delay is the comms/FDC time: the mission is queued at the battery, not fired now.
	Strike->DelayRemainingS = FMath::Max(0.0f, CallIn.DelayS);
	Strike->TimeToNextRoundS = Strike->DelayRemainingS;
	return Strike;
}

void ARFArtilleryStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMissionComplete)
	{
		return;
	}

	TimeToNextRoundS -= DeltaSeconds;
	if (TimeToNextRoundS > 0.0f)
	{
		return;
	}

	FireOneShell();

	if (RoundsFired >= FMath::Max(1, Mission.Rounds))
	{
		bMissionComplete = true;
		// Fire-and-forget: the mission actor cleans itself up once the last shell lands.
		SetLifeSpan(2.0f);
		return;
	}

	// Shells are distributed over 60% of a nominal 60 s window plus the delay, which
	// gives the 8-round 82 mm mission a ~5 s rhythm and the 18-round 122 mm barrage a
	// steady drumbeat rather than one instantaneous salvo.
	const float WindowS = 60.0f;
	TimeToNextRoundS = FMath::Max(0.6f, (WindowS * 0.6f) / FMath::Max(1, Mission.Rounds));
}

FVector ARFArtilleryStrike::SampleImpactPoint() const
{
	// Uniform sampling in the dispersion disc (sqrt keeps the density even instead of
	// clustering shells at the centre, which is what "散布半径" means in the contract).
	const float RadiusCm = SpreadM * 100.0f;
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Radius = FMath::Sqrt(FMath::FRand()) * RadiusCm;

	return TargetPoint + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
}

void ARFArtilleryStrike::FireOneShell()
{
	const FVector Impact = SampleImpactPoint();
	++RoundsFired;

	// A single shell: area damage with the contract's per-round damage and effect radius.
	// 82 mm fragments reach ~18 m, 122 mm ~35 m, 152 mm ~28 m (contract radius_m).
	const float EffectRadiusM = Mission.RadiusM > 0.0f ? Mission.RadiusM : 20.0f;
	RFDamage::ApplyExplosionDamage(GetWorld(), Impact, EffectRadiusM, Mission.DamagePerRound,
		0.0f, CallerActor, this, EffectRadiusM * 0.35f);
}

float ARFArtilleryStrike::GetDangerRadiusM() const
{
	// Friendly-fire warning radius: the fragment radius plus one dispersion radius, so
	// the HUD warns early enough for the squad to break contact and pull back.
	return (Mission.RadiusM > 0.0f ? Mission.RadiusM : 20.0f) + SpreadM;
}
