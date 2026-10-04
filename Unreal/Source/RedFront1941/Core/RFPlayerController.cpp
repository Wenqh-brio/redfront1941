// Copyright RedFront1941. All Rights Reserved.

#include "Core/RFPlayerController.h"

#include "Combat/RFGrenade.h"
#include "Combat/RFWeaponBase.h"
#include "Core/RFCharacter.h"
#include "Core/RFGameMode.h"
#include "Core/RFGameState.h"
#include "Data/RFDataTableLoader.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "InputActionValue.h"
#include "Support/RFSupportSubsystem.h"
#include "Survival/RFInventoryComponent.h"
#include "Survival/RFSurvivalComponent.h"

ARFPlayerController::ARFPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	// The cursor must stay visible: it is the aim reticle of the whole game.
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ARFPlayerController::BeginPlay()
{
	Super::BeginPlay();

	RFPawn = Cast<ARFCharacter>(GetPawn());
	if (UWorld* World = GetWorld())
	{
		RFGameState = World->GetGameState<ARFGameState>();
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (GameplayMappingContext != nullptr)
		{
			InputSubsystem->AddMappingContext(GameplayMappingContext, BaseMappingPriority);
		}
	}

	// Fixed orthographic camera, mouse-driven aiming: keep the cursor free.
	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
}

void ARFPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Bind only the actions that are actually assigned, so an in-progress art/design
		// pass (assets not yet authored) still starts and runs.
		if (IA_Fire) { Enhanced->BindAction(IA_Fire, ETriggerEvent::Started, this, &ARFPlayerController::OnFirePressed); }
		if (IA_Fire) { Enhanced->BindAction(IA_Fire, ETriggerEvent::Completed, this, &ARFPlayerController::OnFireReleased); }
		if (IA_AimDownSights) { Enhanced->BindAction(IA_AimDownSights, ETriggerEvent::Started, this, &ARFPlayerController::OnAimDownSights); }
		if (IA_AimDownSights) { Enhanced->BindAction(IA_AimDownSights, ETriggerEvent::Completed, this, &ARFPlayerController::OnAimDownSights); }
		if (IA_Reload) { Enhanced->BindAction(IA_Reload, ETriggerEvent::Started, this, &ARFPlayerController::OnReload); }
		if (IA_SwapWeapon) { Enhanced->BindAction(IA_SwapWeapon, ETriggerEvent::Started, this, &ARFPlayerController::OnSwapWeapon); }
		if (IA_GrenadeThrow) { Enhanced->BindAction(IA_GrenadeThrow, ETriggerEvent::Started, this, &ARFPlayerController::OnGrenadeThrowPressed); }
		if (IA_GrenadeThrow) { Enhanced->BindAction(IA_GrenadeThrow, ETriggerEvent::Completed, this, &ARFPlayerController::OnGrenadeThrowReleased); }
		if (IA_Melee) { Enhanced->BindAction(IA_Melee, ETriggerEvent::Started, this, &ARFPlayerController::OnMelee); }
		if (IA_Interact) { Enhanced->BindAction(IA_Interact, ETriggerEvent::Started, this, &ARFPlayerController::OnInteract); }
		if (IA_Binoculars) { Enhanced->BindAction(IA_Binoculars, ETriggerEvent::Started, this, &ARFPlayerController::OnBinoculars); }

		if (IA_Move) { Enhanced->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ARFPlayerController::OnMove); }
		if (IA_Move) { Enhanced->BindAction(IA_Move, ETriggerEvent::Completed, this, &ARFPlayerController::OnMoveCompleted); }
		if (IA_Aim) { Enhanced->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &ARFPlayerController::OnAim); }
		if (IA_Sprint) { Enhanced->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ARFPlayerController::OnSprintPressed); }
		if (IA_Sprint) { Enhanced->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ARFPlayerController::OnSprintReleased); }
		if (IA_Crouch) { Enhanced->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ARFPlayerController::OnCrouch); }
		if (IA_Prone) { Enhanced->BindAction(IA_Prone, ETriggerEvent::Started, this, &ARFPlayerController::OnProne); }

		if (IA_Eat) { Enhanced->BindAction(IA_Eat, ETriggerEvent::Started, this, &ARFPlayerController::OnEat); }
		if (IA_Drink) { Enhanced->BindAction(IA_Drink, ETriggerEvent::Started, this, &ARFPlayerController::OnDrink); }
		if (IA_UseMedical) { Enhanced->BindAction(IA_UseMedical, ETriggerEvent::Started, this, &ARFPlayerController::OnUseMedical); }

		if (IA_CallSupportArtillery) { Enhanced->BindAction(IA_CallSupportArtillery, ETriggerEvent::Started, this, &ARFPlayerController::OnCallArtillery); }
		if (IA_CallSupportArmor) { Enhanced->BindAction(IA_CallSupportArmor, ETriggerEvent::Started, this, &ARFPlayerController::OnCallArmor); }
		if (IA_CallSupportAir) { Enhanced->BindAction(IA_CallSupportAir, ETriggerEvent::Started, this, &ARFPlayerController::OnCallAir); }
		if (IA_SupportConfirm) { Enhanced->BindAction(IA_SupportConfirm, ETriggerEvent::Started, this, &ARFPlayerController::OnSupportConfirm); }
		if (IA_OpenMap) { Enhanced->BindAction(IA_OpenMap, ETriggerEvent::Started, this, &ARFPlayerController::OnOpenMap); }
		if (IA_SquadAdvance) { Enhanced->BindAction(IA_SquadAdvance, ETriggerEvent::Started, this, &ARFPlayerController::OnSquadAdvance); }
		if (IA_SquadHold) { Enhanced->BindAction(IA_SquadHold, ETriggerEvent::Started, this, &ARFPlayerController::OnSquadHold); }
		if (IA_EndlessNextWave) { Enhanced->BindAction(IA_EndlessNextWave, ETriggerEvent::Started, this, &ARFPlayerController::OnEndlessNextWave); }
	}

	// Legacy fallback (Config/DefaultInput.ini). Kept so the project is playable before
	// the Enhanced Input assets exist; harmless once they do.
	if (bInstallLegacyFallbackBindings)
	{
		InputComponent->BindAxis(TEXT("Move"), this, &ARFPlayerController::LegacyMoveForward);
		InputComponent->BindAxis(TEXT("MoveRight"), this, &ARFPlayerController::LegacyMoveRight);
		InputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ARFPlayerController::LegacyFirePressed);
		InputComponent->BindAction(TEXT("Fire"), IE_Released, this, &ARFPlayerController::LegacyFireReleased);
		InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ARFPlayerController::LegacyInteractPressed);
	}
}

void ARFPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	RFPawn = Cast<ARFCharacter>(GetPawn());
	UpdateAimFromCursor();

	// Hold-to-cook: the fuse burns while the key is held and the soldier keeps the
	// grenade; releasing (or the fuse running out in RFGrenade) resolves the throw.
	if (bCookingGrenade)
	{
		CookTimeRemainingS -= DeltaTime;
		if (CookTimeRemainingS <= 0.0f)
		{
			OnGrenadeThrowReleased(FInputActionValue());
		}
	}
}

void ARFPlayerController::UpdateAimFromCursor()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	AimWorldLocation = DeprojectScreenToPlane(FVector2D(MouseX, MouseY));
	if (RFPawn != nullptr)
	{
		RFPawn->SetAimWorldLocation(AimWorldLocation);
	}
}

FVector ARFPlayerController::DeprojectScreenToPlane(const FVector2D& ScreenPosition) const
{
	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ForwardVector;
	if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldOrigin, WorldDirection))
	{
		return RFPawn != nullptr ? RFPawn->GetActorLocation() : FVector::ZeroVector;
	}

	// Intersect the view ray with the horizontal gameplay plane at Z = PlaneZ.
	// A near-vertical ray (camera looking straight down) has no stable intersection,
	// so the pawn position is used instead of producing a wild aim point.
	if (FMath::Abs(WorldDirection.Z) < KINDA_SMALL_NUMBER)
	{
		return RFPawn != nullptr ? RFPawn->GetActorLocation() : WorldOrigin;
	}

	const float Distance = (PlaneZ - WorldOrigin.Z) / WorldDirection.Z;
	if (Distance <= 0.0f)
	{
		// Plane is behind the camera: the cursor is above the horizon line.
		return RFPawn != nullptr ? RFPawn->GetActorLocation() : WorldOrigin;
	}

	return WorldOrigin + WorldDirection * Distance;
}

ARFCharacter* ARFPlayerController::GetRFPawn() const
{
	return RFPawn != nullptr ? RFPawn.Get() : Cast<ARFCharacter>(GetPawn());
}

// ------------------------------ Enhanced handlers ----------------------------

void ARFPlayerController::OnMove(const FInputActionValue& Value)
{
	// The Input Action is a 2D vector in screen space: X = East, Y = North.
	const FVector2D Axis = Value.Get<FVector2D>();
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetMoveInput2D(Axis);
	}
}

void ARFPlayerController::OnMoveCompleted(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetMoveInput2D(FVector2D::ZeroVector);
	}
}

void ARFPlayerController::OnAim(const FInputActionValue& Value)
{
	// Gamepad aiming: the 2D vector is treated as a planar direction from the soldier.
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero() || RFPawn == nullptr)
	{
		return;
	}
	AimWorldLocation = RFPawn->GetActorLocation() + FVector(Axis.X, Axis.Y, 0.0f) * 1000.0f;
	RFPawn->SetAimWorldLocation(AimWorldLocation);
}

void ARFPlayerController::OnFirePressed(const FInputActionValue& Value)
{
	bFiring = true;
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
		{
			Weapon->StartFire();
		}
	}
}

void ARFPlayerController::OnFireReleased(const FInputActionValue& Value)
{
	bFiring = false;
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
		{
			Weapon->StopFire();
		}
	}
}

void ARFPlayerController::OnAimDownSights(const FInputActionValue& Value)
{
	// Press flips ADS on; release flips it off (the action reports the new state).
	bAimingDownSights = !bAimingDownSights;
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
		{
			Weapon->SetAimingDownSights(bAimingDownSights);
		}
	}
}

void ARFPlayerController::OnReload(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
		{
			Weapon->StartReload();
		}
	}
}

void ARFPlayerController::OnSwapWeapon(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFInventoryComponent* Inventory = Soldier->GetInventoryComponent())
		{
			Inventory->CycleActiveSlot();
		}
	}
}

void ARFPlayerController::OnGrenadeThrowPressed(const FInputActionValue& Value)
{
	if (bCookingGrenade || GetRFPawn() == nullptr)
	{
		return;
	}
	// Fuse values come from the equipped grenade's contract; the fallback keeps the
	// input usable before an item is resolved.
	bCookingGrenade = true;
	CookFuseS = DefaultGrenadeFuseS;
	CookTimeRemainingS = CookFuseS;
}

void ARFPlayerController::OnGrenadeThrowReleased(const FInputActionValue& Value)
{
	if (!bCookingGrenade)
	{
		return;
	}
	bCookingGrenade = false;

	const float CookedS = FMath::Max(0.0f, CookFuseS - CookTimeRemainingS);
	CookTimeRemainingS = 0.0f;

	if (ARFCharacter* Soldier = GetRFPawn())
	{
		// The grenade actor subtracts the already-burned fuse time, so a fully cooked
		// grenade detonates on impact instead of resting for another 3.5 s.
		ARFGrenade::ThrowCookedGrenade(Soldier, Soldier->GetAimWorldLocation(), CookedS);
	}
}

void ARFPlayerController::OnMelee(const FInputActionValue& Value)
{
	// Melee resolves through the equipped tool (entrenching tool deals 60) or the
	// weapon's bayonet; the damage application lives in RFBallistics/Blueprint.
}

void ARFPlayerController::OnInteract(const FInputActionValue& Value)
{
	ARFCharacter* Soldier = GetRFPawn();
	if (Soldier == nullptr || RFGameState == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	ATargetPoint* NearestObjective = nullptr;
	FName NearestObjectiveId = NAME_None;
	bool bNearestIsRescue = false;
	bool bNearestIsResupply = false;
	FName NearestRescueMarkerTag = NAME_None;
	FName NearestResupplyMarkerTag = NAME_None;
	float NearestDistance = ObjectiveInteractRangeCm;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		ATargetPoint* Marker = *It;
		FName ObjectiveId = NAME_None;
		FName RescueMarkerTag = NAME_None;
		for (const FName& Tag : Marker->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(TEXT("RF_Objective:")))
			{
				ObjectiveId = FName(*TagString.RightChop(13));
			}
			else if (TagString.StartsWith(TEXT("RF_RescueFor:")))
			{
				ObjectiveId = FName(*TagString.RightChop(13));
				for (const FName& MarkerTag : Marker->Tags)
				{
					if (MarkerTag.ToString().StartsWith(TEXT("RF_RescueMarker:")))
					{
						RescueMarkerTag = MarkerTag;
						break;
					}
				}
			}
		}
		const bool bIsRescue = !RescueMarkerTag.IsNone();
		bool bCanInteract = false;
		if (bIsRescue)
		{
			const bool bAlreadyRescued = Marker->ActorHasTag(FName(TEXT("RF_Rescued")));
			for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
			{
				if (Objective.Id == ObjectiveId
					&& Objective.Condition == FName(TEXT("rescue_count"))
					&& RFGameState->GetObjectiveState(ObjectiveId) == ERFObjectiveState::Active
					&& !bAlreadyRescued)
				{
					bCanInteract = true;
					break;
				}
			}
		}
		else
		{
			bCanInteract = !ObjectiveId.IsNone()
				&& RFGameState->GetObjectiveState(ObjectiveId) == ERFObjectiveState::Active
				&& RFGameState->IsObjectiveInteractable(ObjectiveId);
		}
		if (!bCanInteract)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Soldier->GetActorLocation(), Marker->GetActorLocation());
		if (Distance <= NearestDistance)
		{
			NearestDistance = Distance;
			NearestObjective = Marker;
			NearestObjectiveId = ObjectiveId;
			bNearestIsRescue = bIsRescue;
			bNearestIsResupply = false;
			NearestRescueMarkerTag = RescueMarkerTag;
			NearestResupplyMarkerTag = NAME_None;
		}
	}

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		ATargetPoint* Marker = *It;
		FName ResupplyMarkerTag = NAME_None;
		for (const FName& Tag : Marker->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("RF_ResupplyPoint:")))
			{
				ResupplyMarkerTag = Tag;
				break;
			}
		}
		if (ResupplyMarkerTag.IsNone() || Marker->ActorHasTag(FName(TEXT("RF_Resupplied"))))
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Soldier->GetActorLocation(), Marker->GetActorLocation());
		if (Distance <= NearestDistance)
		{
			NearestDistance = Distance;
			NearestObjective = Marker;
			NearestObjectiveId = NAME_None;
			bNearestIsRescue = false;
			bNearestIsResupply = true;
			NearestRescueMarkerTag = NAME_None;
			NearestResupplyMarkerTag = ResupplyMarkerTag;
		}
	}

	if (NearestObjective == nullptr)
	{
		ClientMessage(FString::Printf(TEXT("靠近活动目标（%.0f cm）后按 E 完成目标。"),
			ObjectiveInteractRangeCm));
		return;
	}

	if (bNearestIsResupply)
	{
		ARFGameMode* GameMode = GetWorld()->GetAuthGameMode<ARFGameMode>();
		URFDataTableLoader* Loader = GameMode != nullptr ? GameMode->GetDataTableLoader() : nullptr;
		int32 AmmoAdded = 0;
		if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
		{
			AmmoAdded = Weapon->ReplenishReserveAmmo();
		}

		bool bAddedRation = false;
		bool bAddedWater = false;
		URFInventoryComponent* Inventory = Soldier->GetInventoryComponent();
		if (Loader == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("RFPlayerController: data loader unavailable at resupply point."));
		}
		else if (Inventory != nullptr)
		{
			FRFItemDef Ration;
			const FName RationId = RFGameState->GetCurrentLevel().Faction == ERFFaction::US
				? FName(TEXT("k_ration"))
				: FName(TEXT("black_bread"));
			bAddedRation = Loader->FindItem(RationId, Ration) && Inventory->AddItem(Ration, 1);
			FRFItemDef Water;
			bAddedWater = Loader->FindItem(FName(TEXT("canteen_1l")), Water)
				&& Inventory->AddItem(Water, 1);
		}

		if (AmmoAdded <= 0 && !bAddedRation && !bAddedWater)
		{
			ClientMessage(TEXT("携带容量已满或补给耗尽，暂时无法从此处取得物资。"));
			return;
		}

		NearestObjective->Tags.AddUnique(FName(TEXT("RF_Resupplied")));
		NearestObjective->SetActorHiddenInGame(true);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (*It != NearestObjective && It->ActorHasTag(NearestResupplyMarkerTag))
			{
				It->SetActorHiddenInGame(true);
			}
		}
		ClientMessage(FString::Printf(TEXT("野战补给完成：弹药 +%d，口粮 %s，水壶 %s（食物/饮水请手动使用）。"),
			AmmoAdded, bAddedRation ? TEXT("已装入") : TEXT("无空位"),
			bAddedWater ? TEXT("已装入") : TEXT("无空位")));
		return;
	}

	if (bNearestIsRescue)
	{
		ARFGameMode* GameMode = GetWorld()->GetAuthGameMode<ARFGameMode>();
		if (GameMode == nullptr || !GameMode->NotifySurvivorRescued(NearestObjectiveId))
		{
			UE_LOG(LogTemp, Error, TEXT("RFPlayerController: could not record rescue for objective '%s'."),
				*NearestObjectiveId.ToString());
			return;
		}
		NearestObjective->Tags.AddUnique(FName(TEXT("RF_Rescued")));
		NearestObjective->SetActorHiddenInGame(true);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (*It != NearestObjective && It->ActorHasTag(NearestRescueMarkerTag))
			{
				It->SetActorHiddenInGame(true);
			}
		}
		int32 TargetCount = 0;
		for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
		{
			if (Objective.Id == NearestObjectiveId)
			{
				TargetCount = Objective.TargetCount;
				break;
			}
		}
		ClientMessage(FString::Printf(TEXT("幸存者已撤离：%.0f/%d"),
			RFGameState->GetObjectiveProgress(NearestObjectiveId), TargetCount));
		return;
	}

	FText ObjectiveText = FText::FromName(NearestObjectiveId);
	for (const FRFObjectiveDef& Objective : RFGameState->GetCurrentLevel().Objectives)
	{
		if (Objective.Id == NearestObjectiveId)
		{
			ObjectiveText = Objective.TextZh;
			break;
		}
	}

	RFGameState->SetObjectiveState(NearestObjectiveId, ERFObjectiveState::Completed);
	ClientMessage(FString::Printf(TEXT("目标完成：%s"), *ObjectiveText.ToString()));
	UE_LOG(LogTemp, Log, TEXT("RFPlayerController: objective '%s' completed."),
		*NearestObjectiveId.ToString());
}

void ARFPlayerController::OnBinoculars(const FInputActionValue& Value)
{
	// Binoculars do not change aim here; they set the observation flag consulted by
	// RFSupportSubsystem when computing artillery spread (-35% with binoculars).
}

void ARFPlayerController::OnSprintPressed(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetSprinting(true);
	}
}

void ARFPlayerController::OnSprintReleased(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetSprinting(false);
	}
}

void ARFPlayerController::OnCrouch(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		const bool bWasCrouched = Soldier->GetStance() == ERFStance::Crouch;
		Soldier->SetStance(bWasCrouched ? ERFStance::Stand : ERFStance::Crouch);
	}
}

void ARFPlayerController::OnProne(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		const bool bWasProne = Soldier->GetStance() == ERFStance::Prone;
		Soldier->SetStance(bWasProne ? ERFStance::Stand : ERFStance::Prone);
	}
}

void ARFPlayerController::OnEat(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
		{
			Survival->EatBestAvailableFood();
		}
	}
}

void ARFPlayerController::OnDrink(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
		{
			Survival->DrinkBestAvailableWater();
		}
	}
}

void ARFPlayerController::OnUseMedical(const FInputActionValue& Value)
{
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		if (URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
		{
			Survival->UseBestMedicalItem();
		}
	}
}

void ARFPlayerController::OnCallArtillery(const FInputActionValue& Value)
{
	OnCallSupport(Value, ERFSupportCallType::Indirect);
}

void ARFPlayerController::OnCallArmor(const FInputActionValue& Value)
{
	OnCallSupport(Value, ERFSupportCallType::Armor);
}

void ARFPlayerController::OnCallAir(const FInputActionValue& Value)
{
	OnCallSupport(Value, ERFSupportCallType::Air);
}

void ARFPlayerController::OnCallSupport(const FInputActionValue& Value, ERFSupportCallType CallType)
{
	// The three support keys open the same wheel filtered by call-in type, and register
	// the cursor target; confirming the request is a separate action (Support_Confirm)
	// so a mis-press can be cancelled before the CP are spent.
	PendingSupportType = CallType;

	if (UWorld* World = GetWorld())
	{
		if (URFSupportSubsystem* Support = World->GetSubsystem<URFSupportSubsystem>())
		{
			Support->SetPendingCallInType(CallType);
			Support->SetPendingTarget(AimWorldLocation);
			// Observation state: an observer with line of sight needs binoculars to earn
			// the -35% spread bonus; both flags are authored per level/observer actor.
			Support->SetPendingObservation(bObserverHasLineOfSight, bObserverHasBinoculars);
		}
	}
}

void ARFPlayerController::OnSupportConfirm(const FInputActionValue& Value)
{
	if (UWorld* World = GetWorld())
	{
		if (URFSupportSubsystem* Support = World->GetSubsystem<URFSupportSubsystem>())
		{
			Support->ConfirmPendingRequest();
		}
	}
}

void ARFPlayerController::OnOpenMap(const FInputActionValue& Value)
{
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MapMappingContext != nullptr)
		{
			// Toggle: the map pops the gameplay context while it is open.
			InputSubsystem->AddMappingContext(MapMappingContext, BaseMappingPriority + 1);
		}
	}
}

void ARFPlayerController::OnSquadAdvance(const FInputActionValue& Value)
{
	if (RFGameState != nullptr)
	{
		RFGameState->SetSquadOrder(ERFSquadOrder::Advance);
	}
}

void ARFPlayerController::OnSquadHold(const FInputActionValue& Value)
{
	if (RFGameState != nullptr)
	{
		RFGameState->SetSquadOrder(ERFSquadOrder::Hold);
	}
}

void ARFPlayerController::OnEndlessNextWave(const FInputActionValue& Value)
{
	if (ARFGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARFGameMode>() : nullptr)
	{
		GameMode->StartNextEndlessWave();
	}
}

// ----------------------------- Legacy fallback -------------------------------

void ARFPlayerController::LegacyMoveForward(float Value)
{
	LegacyMoveAxis.Y = Value;
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetMoveInput2D(LegacyMoveAxis);
	}
}

void ARFPlayerController::LegacyMoveRight(float Value)
{
	LegacyMoveAxis.X = Value;
	if (ARFCharacter* Soldier = GetRFPawn())
	{
		Soldier->SetMoveInput2D(LegacyMoveAxis);
	}
}

void ARFPlayerController::LegacyFirePressed()
{
	OnFirePressed(FInputActionValue());
}

void ARFPlayerController::LegacyFireReleased()
{
	OnFireReleased(FInputActionValue());
}

void ARFPlayerController::LegacyInteractPressed()
{
	OnInteract(FInputActionValue());
}
