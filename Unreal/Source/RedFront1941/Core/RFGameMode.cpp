// Copyright RedFront1941. All Rights Reserved.

#include "Core/RFGameMode.h"

#include "AI/RFEnemyAIController.h"
#include "Combat/RFAircraftActor.h"
#include "Combat/RFWeaponBase.h"
#include "Combat/RFVehicleActor.h"
#include "Core/RFCharacter.h"
#include "Core/RFGameState.h"
#include "Core/RFPlayerController.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Data/RFDataTableLoader.h"
#include "Survival/RFInventoryComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Support/RFSupportSubsystem.h"
#include "Survival/RFSurvivalComponent.h"
#include "UI/RFGameHUD.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
float AdvanceHoldPositionProgress(bool bPlayerHolding, float CurrentProgressS,
	float DeltaSeconds, float HoldTimeS)
{
	if (!bPlayerHolding)
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentProgressS + FMath::Max(0.0f, DeltaSeconds), 0.0f, HoldTimeS);
}

float AdvanceEliminationProgress(float CurrentProgress, bool bEnemyMatches, int32 TargetCount)
{
	if (!bEnemyMatches || TargetCount <= 0)
	{
		return CurrentProgress;
	}
	return FMath::Min(CurrentProgress + 1.0f, static_cast<float>(TargetCount));
}

float AdvanceRescueProgress(float CurrentProgress, int32 TargetCount)
{
	if (TargetCount <= 0)
	{
		return CurrentProgress;
	}
	return FMath::Min(CurrentProgress + 1.0f, static_cast<float>(TargetCount));
}

float AdvanceObjectiveTargetProgress(float CurrentProgress, int32 TargetCount)
{
	return AdvanceRescueProgress(CurrentProgress, TargetCount);
}

bool IsWithinObjectiveRadius(const FVector& PlayerLocation, const FVector& ObjectiveLocation,
	float RadiusM)
{
	const float RadiusCm = FMath::Max(100.0f, RadiusM * 100.0f);
	return FVector::Dist2D(PlayerLocation, ObjectiveLocation) <= RadiusCm;
}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFHoldPositionProgressTest,
	"RedFront.Objectives.HoldPositionProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFHoldPositionProgressTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Progress advances while the player holds the zone"),
		FMath::IsNearlyEqual(AdvanceHoldPositionProgress(true, 3.0f, 0.5f, 8.0f), 3.5f));
	TestTrue(TEXT("Leaving the zone resets uninterrupted hold progress"),
		FMath::IsNearlyZero(AdvanceHoldPositionProgress(false, 3.5f, 0.5f, 8.0f)));
	TestTrue(TEXT("Progress is capped at the contract duration"),
		FMath::IsNearlyEqual(AdvanceHoldPositionProgress(true, 7.9f, 0.5f, 8.0f), 8.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFEliminationObjectiveProgressTest,
	"RedFront.Objectives.EliminationProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFEliminationObjectiveProgressTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Matching enemy advances elimination progress"),
		FMath::IsNearlyEqual(AdvanceEliminationProgress(1.0f, true, 3), 2.0f));
	TestTrue(TEXT("Non-matching enemy does not advance filtered progress"),
		FMath::IsNearlyEqual(AdvanceEliminationProgress(1.0f, false, 3), 1.0f));
	TestTrue(TEXT("Elimination progress is capped at target count"),
		FMath::IsNearlyEqual(AdvanceEliminationProgress(3.0f, true, 3), 3.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFRescueObjectiveProgressTest,
	"RedFront.Objectives.RescueProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFRescueObjectiveProgressTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A rescued survivor advances progress"),
		FMath::IsNearlyEqual(AdvanceRescueProgress(2.0f, 6), 3.0f));
	TestTrue(TEXT("Rescue progress is capped at the contract count"),
		FMath::IsNearlyEqual(AdvanceRescueProgress(6.0f, 6), 6.0f));
	TestTrue(TEXT("Invalid rescue counts do not change progress"),
		FMath::IsNearlyEqual(AdvanceRescueProgress(2.0f, 0), 2.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFObjectiveTargetProgressTest,
	"RedFront.Objectives.DestroyTargetProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFObjectiveTargetProgressTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Destroying one target advances the objective"),
		AdvanceObjectiveTargetProgress(0.0f, 2), 1.0f);
	TestEqual(TEXT("Destroyed target progress cannot exceed the contract"),
		AdvanceObjectiveTargetProgress(2.0f, 2), 2.0f);
	TestEqual(TEXT("Invalid target count does not advance progress"),
		AdvanceObjectiveTargetProgress(1.0f, 0), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFObjectiveArrivalRadiusTest,
	"RedFront.Objectives.ArrivalRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFObjectiveArrivalRadiusTest::RunTest(const FString& Parameters)
{
	const FVector ObjectiveLocation(1000.0f, 2000.0f, 0.0f);
	TestTrue(TEXT("Player inside the arrival radius is in the objective zone"),
		IsWithinObjectiveRadius(FVector(1350.0f, 2000.0f, 500.0f), ObjectiveLocation, 4.0f));
	TestFalse(TEXT("Player outside the arrival radius has not reached the objective"),
		IsWithinObjectiveRadius(FVector(1401.0f, 2000.0f, 0.0f), ObjectiveLocation, 4.0f));
	TestTrue(TEXT("Vertical height does not change planar arrival detection"),
		IsWithinObjectiveRadius(FVector(1000.0f, 2000.0f, 1200.0f), ObjectiveLocation, 1.0f));
	return true;
}
#endif

ARFGameMode::ARFGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = ARFGameState::StaticClass();
	PlayerControllerClass = ARFPlayerController::StaticClass();
	HUDClass = ARFGameHUD::StaticClass();
	DefaultPawnClass = ARFCharacter::StaticClass();

	static ConstructorHelpers::FClassFinder<ARFCharacter> SovietPawn(
		TEXT("/Game/RedFront/Blueprints/Characters/BP_SOV_PlayerCharacter"));
	if (SovietPawn.Succeeded())
	{
		SovietPlayerCharacterClass = SovietPawn.Class;
	}

	static ConstructorHelpers::FClassFinder<ARFCharacter> USPawn(
		TEXT("/Game/RedFront/Blueprints/Characters/BP_US_PlayerCharacter"));
	if (USPawn.Succeeded())
	{
		USPlayerCharacterClass = USPawn.Class;
	}

}

UClass* ARFGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const FString MapName = GetWorld() != nullptr ? GetWorld()->GetMapName() : FString();
	if (MapName.Contains(TEXT("C05_US_Western")) && USPlayerCharacterClass != nullptr)
	{
		return USPlayerCharacterClass.Get();
	}
	if (SovietPlayerCharacterClass != nullptr)
	{
		return SovietPlayerCharacterClass.Get();
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ARFGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GermanEnemyCharacterClass == nullptr)
	{
		static const FSoftClassPath GermanPawnPath(
			TEXT("/Game/RedFront/Blueprints/Characters/BP_GER_EnemyCharacter.BP_GER_EnemyCharacter_C"));
		GermanEnemyCharacterClass = GermanPawnPath.TryLoadClass<ARFCharacter>();
	}

	RFGameState = GetGameState<ARFGameState>();
	if (RFGameState != nullptr)
	{
		RFGameState->SetDifficulty(DefaultDifficulty);
	}
	bMapGameplayInitialized = false;

	// The loader is created here (not as a subsystem) so a headless -game run without a
	// world subsystem still gets the contract tables.
	DataTableLoader = NewObject<URFDataTableLoader>(this, TEXT("RFDataTableLoader"));
	if (DataTableLoader != nullptr)
	{
		if (!DataTableLoader->LoadAllContracts())
		{
			UE_LOG(LogTemp, Error,
				TEXT("RFGameMode: one or more required campaign contracts failed to load; mission gameplay may not initialize."));
		}
	}

	if (!DefaultLevelId.IsNone() && DataTableLoader != nullptr)
	{
		StartLevelById(DefaultLevelId);
	}

	WavePrepRemainingS = bEndlessMode ? WavePrepDurationS : 0.0f;
}

void ARFGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TryInitializeMapGameplay();

	// The support economy is driven from here: it must keep running while the mission is
	// paused for a briefing, so it is ticked before the flow-state early-out.
	if (UWorld* World = GetWorld())
	{
		if (URFSupportSubsystem* Support = World->GetSubsystem<URFSupportSubsystem>())
		{
			Support->TickSupport(DeltaSeconds);
		}

	}

	if (RFGameState == nullptr || MatchState != ERFMatchState::Playing)
	{
		return;
	}

	RFGameState->AdvanceMissionTime(DeltaSeconds);
	EvaluateObjectiveConditions(DeltaSeconds);
	EvaluateObjectiveDeadlines();

	// Endless mode: a 45 s prep window runs between waves, and the wave gate is opened
	// either by the timer or by the Endless_NextWave input.
	if (bEndlessMode && WavePrepRemainingS > 0.0f)
	{
		WavePrepRemainingS = FMath::Max(0.0f, WavePrepRemainingS - DeltaSeconds);
	}

	// Failure: player death is a hard fail unless the mission already succeeded.
	// Death is owned by the survival component (bleed-out, blood loss, hypothermia);
	// the game mode is the single place that turns it into a mission result.
	if (RFGameState != nullptr && RFGameState->GetMissionTimeS() >= 0.0f)
	{
		if (const APlayerController* PC = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (const ARFCharacter* Soldier = Cast<ARFCharacter>(PC->GetPawn()))
			{
				if (const URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
				{
					if (Survival->IsDead())
					{
						EndMission(false);
						return;
					}
				}
			}
		}
	}

	EvaluateMissionProgress();
}

void ARFGameMode::TryInitializeMapGameplay()
{
	if (bMapGameplayInitialized || DataTableLoader == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	ARFCharacter* PlayerCharacter = PlayerController != nullptr
		? Cast<ARFCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	FRFLevelDef Level;
	bool bFoundLevel = false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& ActorTag : It->Tags)
		{
			const FString TagString = ActorTag.ToString();
			if (TagString.StartsWith(TEXT("RF_LevelId:")))
			{
				bFoundLevel = DataTableLoader->FindLevel(FName(*TagString.RightChop(11)), Level);
				break;
			}
		}
		if (bFoundLevel)
		{
			break;
		}
	}

	if (!bFoundLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("RFGameMode: no RF_LevelId marker found; map enemy spawning is disabled."));
		bMapGameplayInitialized = true;
		return;
	}

	StartLevel(Level);
	if (PlayerCharacter->GetCurrentWeapon() == nullptr)
	{
		const FName WeaponId = Level.Faction == ERFFaction::US
			? FName(TEXT("m1_garand")) : FName(TEXT("mosin_m1891_30"));
		FRFWeaponDef WeaponDefinition;
		if (DataTableLoader->FindWeapon(WeaponId, WeaponDefinition))
		{
			EquipContractWeapon(PlayerCharacter, WeaponDefinition);
			if (URFInventoryComponent* Inventory = PlayerCharacter->GetInventoryComponent())
			{
				FRFClassDef RiflemanClass;
				if (DataTableLoader->FindClass(FName(TEXT("rifleman")), RiflemanClass))
				{
					Inventory->ConfigureSlots(RiflemanClass);
					Inventory->AddWeapon(WeaponDefinition.Id, WeaponDefinition.WeightKg,
						WeaponDefinition.Magazine, ERFSlotType::Primary);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RFGameMode: player weapon '%s' is missing from weapons.json."),
				*WeaponId.ToString());
		}
	}

	SpawnMapEnemies(Level);
	bMapGameplayInitialized = true;
}

void ARFGameMode::EquipContractWeapon(ARFCharacter* Soldier, const FRFWeaponDef& WeaponDefinition)
{
	if (Soldier == nullptr)
	{
		return;
	}

	URFWeaponBase* Weapon = NewObject<URFWeaponBase>(Soldier);
	if (Weapon == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("RFGameMode: failed to create weapon component for %s."),
			*GetNameSafe(Soldier));
		return;
	}
	Soldier->AddInstanceComponent(Weapon);
	Weapon->InitializeFromDefinition(WeaponDefinition);
	Weapon->RegisterComponent();
	Soldier->EquipWeapon(Weapon);
}

void ARFGameMode::SpawnMapEnemies(const FRFLevelDef& Level)
{
	int32 SpawnedCount = 0;

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		ATargetPoint* Marker = *It;
		FName EnemyId = NAME_None;
		int32 ContractCount = 1;
		for (const FName& Tag : Marker->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(TEXT("RF_EnemyClass:")))
			{
				EnemyId = FName(*TagString.RightChop(14));
			}
			else if (TagString.StartsWith(TEXT("RF_EnemyCount:")))
			{
				ContractCount = FCString::Atoi(*TagString.RightChop(14));
			}
		}
		if (EnemyId.IsNone() || ContractCount <= 0)
		{
			continue;
		}

		FRFEnemyDef EnemyDefinition;
		if (!DataTableLoader->FindEnemy(EnemyId, EnemyDefinition))
		{
			FRFVehicleDef VehicleDefinition;
			if (DataTableLoader->FindVehicle(EnemyId, VehicleDefinition))
			{
				const int32 VehicleCount = FMath::Min(ContractCount, 8);
				if (VehicleCount < ContractCount)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("RFGameMode: vehicle marker '%s' requests %d; spawning %d (vehicle marker limit 8)."),
						*EnemyId.ToString(), ContractCount, VehicleCount);
				}
				for (int32 VehicleIndex = 0; VehicleIndex < VehicleCount; ++VehicleIndex)
				{
					const float LateralOffset = (static_cast<float>(VehicleIndex) -
						(static_cast<float>(VehicleCount) - 1.0f) * 0.5f) * 250.0f;
					const FVector SpawnLocation = Marker->GetActorLocation() +
						FVector(0.0f, LateralOffset, 0.0f);
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride =
						ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					ARFVehicleActor* Vehicle = GetWorld()->SpawnActor<ARFVehicleActor>(
						ARFVehicleActor::StaticClass(), SpawnLocation, Marker->GetActorRotation(),
						SpawnParameters);
					if (Vehicle == nullptr)
					{
						UE_LOG(LogTemp, Warning, TEXT("RFGameMode: failed spawning vehicle '%s'."),
							*EnemyId.ToString());
						continue;
					}
					Vehicle->Tags.AddUnique(FName(TEXT("RF_Enemy")));
					Vehicle->Tags.AddUnique(FName(*FString::Printf(TEXT("RF_EnemyId:%s"),
						*EnemyId.ToString())));
					Vehicle->InitializeFromDefinition(VehicleDefinition, Level.Faction);
					UE_LOG(LogTemp, Log, TEXT("RFGameMode: spawned vehicle '%s' HP=%.0f armor=%.0fmm."),
						*EnemyId.ToString(), VehicleDefinition.Hp, VehicleDefinition.ArmorMm);
				}
			}
			else
			{
				if (const FRFAirUnitDef* AirDefinition = DataTableLoader->GetAirUnits().Find(EnemyId))
				{
					const int32 AircraftCount = FMath::Min(ContractCount, 2);
					if (AircraftCount < ContractCount)
					{
						UE_LOG(LogTemp, Warning,
							TEXT("RFGameMode: air marker '%s' requests %d units; spawning %d (air marker limit 2)."),
							*EnemyId.ToString(), ContractCount, AircraftCount);
					}
					for (int32 AircraftIndex = 0; AircraftIndex < AircraftCount; ++AircraftIndex)
					{
						const FVector SpawnLocation = Marker->GetActorLocation() +
							FVector(0.0f, 450.0f * AircraftIndex, 0.0f);
						FActorSpawnParameters SpawnParameters;
						SpawnParameters.SpawnCollisionHandlingOverride =
							ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
						ARFAircraftActor* Aircraft = GetWorld()->SpawnActor<ARFAircraftActor>(
							ARFAircraftActor::StaticClass(), SpawnLocation,
							Marker->GetActorRotation(), SpawnParameters);
						if (Aircraft == nullptr)
						{
							UE_LOG(LogTemp, Warning, TEXT("RFGameMode: failed spawning aircraft '%s'."),
								*EnemyId.ToString());
							continue;
						}
						Aircraft->Tags.AddUnique(FName(TEXT("RF_Enemy")));
						Aircraft->Tags.AddUnique(FName(*FString::Printf(TEXT("RF_EnemyId:%s"),
							*EnemyId.ToString())));
						Aircraft->InitializeFromDefinition(*AirDefinition, Level.Faction,
							4.0f + 5.0f * static_cast<float>(AircraftIndex));
						UE_LOG(LogTemp, Log, TEXT("RFGameMode: spawned air unit '%s' HP=%.0f bomb_radius=%.0fm."),
							*EnemyId.ToString(), AirDefinition->Hp, AirDefinition->BombRadiusM);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("RFGameMode: enemy contract '%s' not found; marker skipped."),
						*EnemyId.ToString());
				}
			}
			continue;
		}

		const int32 SpawnCount = FMath::Min(
			ContractCount,
			FMath::Min(MaxEnemyUnitsPerMarker, FMath::Max(0, MaxEnemyUnitsPerMap - SpawnedCount)));
		if (SpawnCount < ContractCount)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("RFGameMode: marker '%s' requests %d units; spawning %d due to the %d-per-marker / %d-per-map budget."),
				*EnemyId.ToString(), ContractCount, SpawnCount, MaxEnemyUnitsPerMarker, MaxEnemyUnitsPerMap);
		}
		if (SpawnCount == 0)
		{
			continue;
		}

		for (int32 UnitIndex = 0; UnitIndex < SpawnCount; ++UnitIndex)
		{
			const float Angle = 2.39996323f * static_cast<float>(UnitIndex);
			const float Radius = 95.0f * FMath::Sqrt(static_cast<float>(UnitIndex));
			const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			TSubclassOf<ARFCharacter> EnemyClass = GermanEnemyCharacterClass;
			if (EnemyDefinition.Faction == ERFFaction::Soviet && SovietPlayerCharacterClass != nullptr)
			{
				EnemyClass = SovietPlayerCharacterClass;
			}
			else if (EnemyDefinition.Faction == ERFFaction::US && USPlayerCharacterClass != nullptr)
			{
				EnemyClass = USPlayerCharacterClass;
			}
			if (EnemyClass == nullptr)
			{
				EnemyClass = ARFCharacter::StaticClass();
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			ARFCharacter* Enemy = GetWorld()->SpawnActor<ARFCharacter>(
				EnemyClass, Marker->GetActorLocation() + Offset,
				Marker->GetActorRotation(), SpawnParameters);
			if (Enemy == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("RFGameMode: failed spawning enemy '%s'."), *EnemyId.ToString());
				continue;
			}

			Enemy->Tags.AddUnique(FName(TEXT("RF_Enemy")));
			Enemy->Tags.AddUnique(FName(*FString::Printf(TEXT("RF_EnemyId:%s"),
				*EnemyId.ToString())));
			Enemy->AIControllerClass = ARFEnemyAIController::StaticClass();
			Enemy->SpawnDefaultController();
			if (URFSurvivalComponent* Survival = Enemy->GetSurvivalComponent())
			{
				Survival->InitializeHealth(EnemyDefinition.Hp);
			}
			if (ARFEnemyAIController* AIController = Cast<ARFEnemyAIController>(Enemy->GetController()))
			{
				AIController->InitializeFromDefinition(EnemyDefinition);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("RFGameMode: enemy '%s' has no AI controller."),
					*EnemyId.ToString());
			}

			FRFWeaponDef EnemyWeapon;
			EnemyWeapon.Id = EnemyDefinition.Weapon;
			EnemyWeapon.Faction = EnemyDefinition.Faction;
			EnemyWeapon.Damage = EnemyDefinition.Damage;
			EnemyWeapon.Rpm = 60.0f / FMath::Max(0.05f, EnemyDefinition.FireRateS);
			EnemyWeapon.Magazine = EnemyDefinition.Role == FName(TEXT("support")) ? 50 : 30;
			EnemyWeapon.ReloadS = 3.0f;
			EnemyWeapon.WeightKg = 4.0f;
			EnemyWeapon.Accuracy = FMath::Clamp(EnemyDefinition.Accuracy, 0.1f, 0.95f);
			EnemyWeapon.EffectiveRangeM = EnemyDefinition.Role == FName(TEXT("sniper")) ? 500.0f : 250.0f;
			if (EnemyDefinition.Role == FName(TEXT("sniper")))
			{
				EnemyWeapon.Category = ERFWeaponCategory::Sniper;
			}
			else if (EnemyDefinition.Role == FName(TEXT("support")))
			{
				EnemyWeapon.Category = ERFWeaponCategory::LMG;
			}
			else if (EnemyWeapon.Rpm >= 400.0f)
			{
				EnemyWeapon.Category = ERFWeaponCategory::SMG;
			}
			else
			{
				EnemyWeapon.Category = ERFWeaponCategory::RifleBolt;
			}
			EquipContractWeapon(Enemy, EnemyWeapon);
			++SpawnedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RFGameMode: spawned %d enemy infantry for level '%s' (faction %d)."),
		SpawnedCount, *Level.Id.ToString(), static_cast<int32>(Level.Faction));
}

bool ARFGameMode::StartLevelById(FName LevelId)
{
	if (DataTableLoader == nullptr)
	{
		DataTableLoader = NewObject<URFDataTableLoader>(this, TEXT("RFDataTableLoader"));
	}

	FRFLevelDef Found;
	if (DataTableLoader == nullptr || !DataTableLoader->FindLevel(LevelId, Found))
	{
		UE_LOG(LogTemp, Warning, TEXT("RFGameMode: level contract '%s' not found."), *LevelId.ToString());
		return false;
	}

	StartLevel(Found);
	return true;
}

void ARFGameMode::StartLevel(const FRFLevelDef& Level)
{
	if (RFGameState == nullptr)
	{
		RFGameState = GetGameState<ARFGameState>();
	}
	if (RFGameState == nullptr)
	{
		return;
	}

	RFGameState->SetCurrentLevel(Level);
	RFGameState->SetDifficulty(DefaultDifficulty);

	// Command points start from the contract, not from the previous level (CP never
	// carries over between levels; endless mode grants +2 every 5 waves instead).
	if (DataTableLoader != nullptr)
	{
		const FRFCommandPointRules& CPCost = DataTableLoader->GetCommandPointRules();
		RFGameState->SetCommsRules(DataTableLoader->GetCommsRules());
		RFGameState->AddCommandPoints(static_cast<float>(CPCost.Start) - RFGameState->GetCommandPoints());
	}

	WavePrepRemainingS = bEndlessMode ? WavePrepDurationS : 0.0f;
	MatchState = ERFMatchState::Playing;
}

bool ARFGameMode::AreRequiredObjectivesMet() const
{
	if (RFGameState == nullptr)
	{
		return false;
	}

	const FRFLevelDef& Level = RFGameState->GetCurrentLevel();
	if (Level.Objectives.Num() == 0)
	{
		return false;
	}

	for (const FRFObjectiveDef& Objective : Level.Objectives)
	{
		if (!Objective.bRequired && !Objective.IsPrimary())
		{
			continue;
		}
		if (RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Completed)
		{
			return false;
		}
	}
	return true;
}

bool ARFGameMode::HasFailedRequiredObjective() const
{
	if (RFGameState == nullptr)
	{
		return false;
	}

	const FRFLevelDef& Level = RFGameState->GetCurrentLevel();
	for (const FRFObjectiveDef& Objective : Level.Objectives)
	{
		if (!Objective.bRequired && !Objective.IsPrimary())
		{
			continue;
		}
		if (RFGameState->GetObjectiveState(Objective.Id) == ERFObjectiveState::Failed)
		{
			return true;
		}
	}
	return false;
}

void ARFGameMode::StartNextEndlessWave()
{
	if (RFGameState == nullptr || !bEndlessMode)
	{
		return;
	}
	if (WavePrepRemainingS > 0.0f)
	{
		// The prep window is a real constraint: it lets the squad resupply food/water/ammo.
		return;
	}

	RFGameState->AdvanceEndlessWave();
	WavePrepRemainingS = WavePrepDurationS;
}

void ARFGameMode::EndMission(bool bSuccess)
{
	if (MatchState == ERFMatchState::Success || MatchState == ERFMatchState::Failure)
	{
		return;
	}

	MatchState = bSuccess ? ERFMatchState::Success : ERFMatchState::Failure;
	if (RFGameState != nullptr)
	{
		RFGameState->SetMissionComplete(bSuccess);
	}
}

ARFPlayerController* ARFGameMode::GetRFPlayerController() const
{
	if (const UWorld* World = GetWorld())
	{
		return Cast<ARFPlayerController>(World->GetFirstPlayerController());
	}
	return nullptr;
}

void ARFGameMode::EvaluateMissionProgress()
{
	if (HasFailedRequiredObjective())
	{
		EndMission(false);
		return;
	}
	if (AreRequiredObjectivesMet())
	{
		EndMission(true);
	}
}

void ARFGameMode::NotifyEnemyEliminated(FName EnemyId)
{
	if (RFGameState == nullptr || EnemyId.IsNone()
		|| MatchState != ERFMatchState::Playing)
	{
		return;
	}

	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.Condition != FName(TEXT("eliminate_count"))
			|| Objective.TargetCount <= 0
			|| RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Active
			|| (Objective.TargetEnemyIds.Num() > 0 && !Objective.TargetEnemyIds.Contains(EnemyId)))
		{
			continue;
		}

		const float NewProgress = AdvanceEliminationProgress(
			RFGameState->GetObjectiveProgress(Objective.Id), true, Objective.TargetCount);
		RFGameState->SetObjectiveProgress(Objective.Id, NewProgress);
		UE_LOG(LogTemp, Log, TEXT("RFGameMode: elimination objective '%s' progress %.0f/%d (%s)."),
			*Objective.Id.ToString(), NewProgress, Objective.TargetCount, *EnemyId.ToString());
		if (NewProgress >= static_cast<float>(Objective.TargetCount))
		{
			RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Completed);
		}
	}
	EvaluateMissionProgress();
}

bool ARFGameMode::NotifySurvivorRescued(FName ObjectiveId)
{
	if (RFGameState == nullptr || ObjectiveId.IsNone()
		|| MatchState != ERFMatchState::Playing)
	{
		return false;
	}

	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.Id != ObjectiveId
			|| Objective.Condition != FName(TEXT("rescue_count"))
			|| Objective.TargetCount <= 0
			|| RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Active)
		{
			continue;
		}

		const float NewProgress = AdvanceRescueProgress(
			RFGameState->GetObjectiveProgress(Objective.Id), Objective.TargetCount);
		RFGameState->SetObjectiveProgress(Objective.Id, NewProgress);
		UE_LOG(LogTemp, Log, TEXT("RFGameMode: rescue objective '%s' progress %.0f/%d."),
			*Objective.Id.ToString(), NewProgress, Objective.TargetCount);
		if (NewProgress >= static_cast<float>(Objective.TargetCount))
		{
			RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Completed);
		}
		EvaluateMissionProgress();
		return true;
	}
	return false;
}

bool ARFGameMode::NotifyObjectiveTargetDestroyed(FName ObjectiveId)
{
	if (RFGameState == nullptr || ObjectiveId.IsNone()
		|| MatchState != ERFMatchState::Playing)
	{
		return false;
	}

	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.Id != ObjectiveId
			|| Objective.Condition != FName(TEXT("destroy_target"))
			|| Objective.TargetCount <= 0
			|| RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Active)
		{
			continue;
		}

		const float NewProgress = AdvanceObjectiveTargetProgress(
			RFGameState->GetObjectiveProgress(Objective.Id), Objective.TargetCount);
		RFGameState->SetObjectiveProgress(Objective.Id, NewProgress);
		UE_LOG(LogTemp, Log, TEXT("RFGameMode: destruction objective '%s' progress %.0f/%d."),
			*Objective.Id.ToString(), NewProgress, Objective.TargetCount);
		if (NewProgress >= static_cast<float>(Objective.TargetCount))
		{
			RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Completed);
		}
		EvaluateMissionProgress();
		return true;
	}
	return false;
}

void ARFGameMode::EvaluateObjectiveConditions(float DeltaSeconds)
{
	if (RFGameState == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.Condition == FName(TEXT("reach_location"))
			&& RFGameState->GetObjectiveState(Objective.Id) == ERFObjectiveState::Active)
		{
			for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
			{
				if (!It->ActorHasTag(FName(*FString::Printf(TEXT("RF_Objective:%s"),
					*Objective.Id.ToString()))))
				{
					continue;
				}

				if (PlayerPawn != nullptr && IsWithinObjectiveRadius(PlayerPawn->GetActorLocation(),
					It->GetActorLocation(), Objective.RadiusM))
				{
					RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Completed);
					RFGameState->SetObjectiveProgress(Objective.Id, 1.0f);
					UE_LOG(LogTemp, Log, TEXT("RFGameMode: reached location objective '%s'."),
						*Objective.Id.ToString());
				}
				break;
			}
			continue;
		}

		if (Objective.Condition != FName(TEXT("hold_position"))
			|| Objective.HoldTimeS <= 0.0f
			|| RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Active)
		{
			continue;
		}

		const ATargetPoint* ObjectiveMarker = nullptr;
		for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(FName(*FString::Printf(TEXT("RF_Objective:%s"),
				*Objective.Id.ToString()))))
			{
				ObjectiveMarker = *It;
				break;
			}
		}
		const float RadiusCm = FMath::Max(100.0f, Objective.RadiusM * 100.0f);
		const bool bPlayerHolding = PlayerPawn != nullptr && ObjectiveMarker != nullptr
			&& FVector::Dist2D(PlayerPawn->GetActorLocation(), ObjectiveMarker->GetActorLocation()) <= RadiusCm;
		const float ProgressS = AdvanceHoldPositionProgress(bPlayerHolding,
			RFGameState->GetObjectiveProgress(Objective.Id), DeltaSeconds, Objective.HoldTimeS);
		RFGameState->SetObjectiveProgress(Objective.Id, ProgressS);

		if (ProgressS >= Objective.HoldTimeS)
		{
			RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Completed);
			UE_LOG(LogTemp, Log, TEXT("RFGameMode: hold objective '%s' secured after %.1f s."),
				*Objective.Id.ToString(), ProgressS);
		}
	}
}

void ARFGameMode::EvaluateObjectiveDeadlines()
{
	if (RFGameState == nullptr)
	{
		return;
	}

	const float MissionTimeS = RFGameState->GetMissionTimeS();
	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.TimeLimitS <= 0.0f
			|| RFGameState->GetObjectiveState(Objective.Id) != ERFObjectiveState::Active
			|| MissionTimeS < Objective.TimeLimitS)
		{
			continue;
		}

		RFGameState->SetObjectiveState(Objective.Id, ERFObjectiveState::Failed);
		UE_LOG(LogTemp, Warning, TEXT("RFGameMode: objective '%s' timed out at %.1f s (limit %.1f s)."),
			*Objective.Id.ToString(), MissionTimeS, Objective.TimeLimitS);
	}
}
