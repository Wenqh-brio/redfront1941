// Copyright RedFront1941. All Rights Reserved.

#include "Combat/RFObjectiveTargetActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/RFGameMode.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
float GetRemainingTargetHealth(float CurrentHealth, float DamageAmount)
{
	if (!FMath::IsFinite(CurrentHealth) || CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return CurrentHealth;
	}
	return FMath::Max(0.0f, CurrentHealth - DamageAmount);
}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRFObjectiveTargetDamageTest,
	"RedFront.Objectives.DestroyTargetDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRFObjectiveTargetDamageTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Damage reduces the target's remaining health"),
		GetRemainingTargetHealth(300.0f, 75.0f), 225.0f);
	TestEqual(TEXT("Overkill clamps target health to zero"),
		GetRemainingTargetHealth(40.0f, 75.0f), 0.0f);
	TestEqual(TEXT("Invalid damage cannot harm an objective target"),
		GetRemainingTargetHealth(100.0f, -1.0f), 100.0f);
	return true;
}
#endif

ARFObjectiveTargetActor::ARFObjectiveTargetActor()
{
	SetCanBeDamaged(true);
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TargetCollision"));
	CollisionBox->SetBoxExtent(FVector(55.0f, 55.0f, 58.0f));
	CollisionBox->SetCollisionProfileName(TEXT("RF_2DSprite"));
	RootComponent = CollisionBox;

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	TargetMesh->SetupAttachment(RootComponent);
	TargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetMesh->SetRelativeScale3D(FVector(1.1f, 1.1f, 1.16f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TargetMesh->SetStaticMesh(CubeMesh.Object);
	}

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetNameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetVerticalAlignment(EVRTA_TextCenter);
	Nameplate->SetWorldSize(28.0f);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 82.0f));
	Nameplate->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Nameplate->SetTextRenderColor(FColor(255, 200, 120));
	Tags.Add(FName(TEXT("RF_ObjectiveTarget")));
}

void ARFObjectiveTargetActor::InitializeTarget(FName InObjectiveId, float InMaxHealth,
	FName InDisplayName)
{
	ObjectiveId = InObjectiveId;
	MaxHealth = FMath::Max(1.0f, InMaxHealth);
	Health = MaxHealth;
	DisplayName = InDisplayName.IsNone() ? ObjectiveId : InDisplayName;
	Tags.AddUnique(FName(*FString::Printf(TEXT("RF_DestroyTargetFor:%s"),
		*ObjectiveId.ToString())));
	Nameplate->SetText(FText::FromString(FString::Printf(TEXT("%s  %.0f/%.0f"),
		*DisplayName.ToString(), Health, MaxHealth)));
}

float ARFObjectiveTargetActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (Health <= 0.0f || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = Health;
	Health = GetRemainingTargetHealth(Health, DamageAmount);
	const float AppliedDamage = PreviousHealth - Health;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TargetMesh->SetVisibility(false, true);
		Nameplate->SetText(FText::FromString(TEXT("目标已摧毁")));
		SetLifeSpan(8.0f);

		if (GetWorld() != nullptr)
		{
			if (ARFGameMode* GameMode = GetWorld()->GetAuthGameMode<ARFGameMode>())
			{
				GameMode->NotifyObjectiveTargetDestroyed(ObjectiveId);
			}
		}
	}
	else
	{
		Nameplate->SetText(FText::FromString(FString::Printf(TEXT("%s  %.0f/%.0f"),
			*DisplayName.ToString(), Health, MaxHealth)));
	}
	return AppliedDamage;
}
