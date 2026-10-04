// Copyright RedFront1941. All Rights Reserved.

#include "Support/RFTankCallIn.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ARFTankCallIn::ARFTankCallIn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
}

void ARFTankCallIn::BeginPlay()
{
	Super::BeginPlay();
	ResolveAirInterception();
}

ARFTankCallIn* ARFTankCallIn::RequestTankSupport(const UObject* WorldContextObject, const FRFSupportCallInDef& CallIn,
	const FVector& TargetPoint, const TArray<FVector>& ArrivalPath, AActor* Caller)
{
	UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	// No road to the objective means no armour: the contract is explicit that tanks follow
	// roads and open terrain and cannot cross rubble or dense forest.
	if (ArrivalPath.Num() < 2)
	{
		return nullptr;
	}

	// Reject a "route" that is really a straight line across the map: the rear-road entry
	// must actually approach the objective rather than cut through the middle of the level.
	const FVector RouteStart = ArrivalPath[0];
	const FVector RouteEnd = ArrivalPath.Last();
	const FVector2D RouteDir(RouteEnd.X - RouteStart.X, RouteEnd.Y - RouteStart.Y);
	const FVector2D ObjectiveDir(TargetPoint.X - RouteEnd.X, TargetPoint.Y - RouteEnd.Y);
	if (!RouteDir.IsNearlyZero() && !ObjectiveDir.IsNearlyZero())
	{
		const float Cos = FVector2D::DotProduct(RouteDir.GetSafeNormal(), ObjectiveDir.GetSafeNormal());
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0f, 1.0f)));
		if (AngleDeg > 75.0f)
		{
			// The road runs away from the objective; the platoon cannot support this attack.
			return nullptr;
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = Caller;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARFTankCallIn* CallInActor = World->SpawnActor<ARFTankCallIn>(ARFTankCallIn::StaticClass(),
		ArrivalPath[0], FRotator::ZeroRotator, Params);
	if (CallInActor == nullptr)
	{
		return nullptr;
	}

	CallInActor->Platoon = CallIn;
	CallInActor->ObjectivePoint = TargetPoint;
	CallInActor->RoutePoints = ArrivalPath;
	CallInActor->CallerActor = Caller;
	CallInActor->OperationalVehicles = FMath::Max(1, CallIn.Units);
	CallInActor->FreeDesantSlots = FMath::Max(0, CallIn.DesantSlots) * FMath::Max(1, CallIn.Units);
	CallInActor->DutyRemainingS = 480.0f;

	// Arrival time from the column's road speed (T-34/76: 40 km/h; ISU-152: slower).
	const float SpeedKph = FMath::Max(5.0f, CallIn.HpEach > 500.0f ? 30.0f : 40.0f);
	const float RouteLengthM = CallInActor->GetRouteLengthM();
	CallInActor->ArrivalRemainingS = FMath::Max(CallIn.DelayS, (RouteLengthM / 1000.0f) / SpeedKph * 3600.0f);
	return CallInActor;
}

float ARFTankCallIn::GetRouteLengthM() const
{
	float LengthCm = 0.0f;
	for (int32 Index = 1; Index < RoutePoints.Num(); ++Index)
	{
		LengthCm += FVector::Dist(RoutePoints[Index - 1], RoutePoints[Index]);
	}
	// Add the final leg from the last road point to the objective.
	if (RoutePoints.Num() > 0)
	{
		LengthCm += FVector::Dist(RoutePoints.Last(), ObjectivePoint);
	}
	return LengthCm / 100.0f;
}

void ARFTankCallIn::ResolveAirInterception()
{
	// Contract: enemy aircraft (Ju 87, Fw 190) attack armour columns, and Flak protects
	// them. A larger column is easier to spot, so the interception chance scales with it.
	const int32 UnitCount = FMath::Max(1, Platoon.Units);
	const float InterceptionChance = FMath::Clamp(0.05f * static_cast<float>(UnitCount), 0.0f, 0.35f);

	if (FMath::FRand() < InterceptionChance)
	{
		// Enemy air found the column on the road: one vehicle is lost before it arrives,
		// and any tank-desant infantry riding it is dismounted early.
		OperationalVehicles = FMath::Max(0, OperationalVehicles - 1);
		FreeDesantSlots = FMath::Min(FreeDesantSlots, OperationalVehicles * FMath::Max(0, Platoon.DesantSlots));
		if (OperationalVehicles <= 0)
		{
			CallState = ERFArmorCallState::Destroyed;
		}
	}
}

void ARFTankCallIn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CallState == ERFArmorCallState::Destroyed)
	{
		return;
	}

	if (CallState == ERFArmorCallState::AwaitingArrival)
	{
		ArrivalRemainingS -= DeltaSeconds;
		if (ArrivalRemainingS <= 0.0f)
		{
			ArrivalRemainingS = 0.0f;
			CallState = ERFArmorCallState::OnStation;
		}
		return;
	}

	if (CallState == ERFArmorCallState::OnStation)
	{
		// Fuel and ammunition are finite: 8 minutes on station per the contract.
		DutyRemainingS -= DeltaSeconds;
		if (DutyRemainingS <= 0.0f)
		{
			DutyRemainingS = 0.0f;
			CallState = ERFArmorCallState::Withdrawing;
			UnloadAllDesant();
			SetLifeSpan(20.0f);
		}
	}
}

bool ARFTankCallIn::TryLoadDesant(AActor* Soldier)
{
	// Riders must be near the column: tank desant is a physical action, not a menu.
	if (CallState != ERFArmorCallState::OnStation || FreeDesantSlots <= 0 || Soldier == nullptr)
	{
		return false;
	}

	const float DistanceCm = FVector::Dist(Soldier->GetActorLocation(), ObjectivePoint);
	if (DistanceCm > 800.0f)
	{
		return false;
	}

	--FreeDesantSlots;
	return true;
}

void ARFTankCallIn::UnloadAllDesant()
{
	// FreeSeats go back to the pool; the Blueprint tanks detach their riders here.
	FreeDesantSlots = OperationalVehicles * FMath::Max(0, Platoon.DesantSlots);
}

void ARFTankCallIn::ReportVehicleLost()
{
	OperationalVehicles = FMath::Max(0, OperationalVehicles - 1);
	FreeDesantSlots = FMath::Min(FreeDesantSlots, OperationalVehicles * FMath::Max(0, Platoon.DesantSlots));

	if (OperationalVehicles <= 0)
	{
		CallState = ERFArmorCallState::Destroyed;
		SetLifeSpan(2.0f);
	}
}
