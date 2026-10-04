// Copyright RedFront1941. All Rights Reserved.

#include "UI/RFGameHUD.h"

#include "Combat/RFWeaponBase.h"
#include "Core/RFCharacter.h"
#include "Core/RFGameMode.h"
#include "Core/RFGameState.h"
#include "Core/RFPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Survival/RFSurvivalComponent.h"

namespace
{
	FString ObjectiveStateLabel(ERFObjectiveState State)
	{
		switch (State)
		{
		case ERFObjectiveState::Active: return TEXT("进行中");
		case ERFObjectiveState::Completed: return TEXT("完成");
		case ERFObjectiveState::Failed: return TEXT("失败");
		default: return TEXT("未开始");
		}
	}

	void DrawColoredText(UCanvas* Canvas, UFont* Font, const FString& Text,
		float X, float Y, const FLinearColor& Color)
	{
		FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(Text), Font, Color);
		TextItem.Scale = FVector2D(1.0f, 1.0f);
		TextItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(TextItem);
	}
}

void ARFGameHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr || PlayerOwner == nullptr || GEngine == nullptr)
	{
		return;
	}

	ARFGameState* GameState = GetWorld() != nullptr
		? GetWorld()->GetGameState<ARFGameState>() : nullptr;
	ARFPlayerController* PlayerController = Cast<ARFPlayerController>(PlayerOwner);
	ARFCharacter* Soldier = PlayerController != nullptr ? Cast<ARFCharacter>(PlayerController->GetPawn()) : nullptr;
	const float PanelWidth = FMath::Min(460.0f, Canvas->SizeX - 32.0f);
	const float PanelHeight = 158.0f;
	const FVector2D PanelOrigin(16.0f, 16.0f);
	DrawRect(FLinearColor(0.015f, 0.025f, 0.03f, 0.82f),
		PanelOrigin.X, PanelOrigin.Y, PanelWidth, PanelHeight);

	UFont* Font = GEngine->GetSmallFont();
	float Y = PanelOrigin.Y + 12.0f;
	const float LineHeight = 20.0f;
	const auto DrawLine = [this, Font, &Y, LineHeight](const FString& Text, const FLinearColor& Color)
	{
		DrawColoredText(Canvas, Font, Text, 28.0f, Y, Color);
		Y += LineHeight;
	};

	if (GameState != nullptr)
	{
		const FRFLevelDef& Level = GameState->GetCurrentLevel();
		DrawLine(FString::Printf(TEXT("%s   %02d:%02d   CP %.1f"),
			*Level.NameZh.ToString(),
			FMath::FloorToInt(GameState->GetMissionTimeS() / 60.0f),
			FMath::FloorToInt(FMath::Fmod(GameState->GetMissionTimeS(), 60.0f)),
			GameState->GetCommandPoints()), FLinearColor(0.92f, 0.82f, 0.58f));

		const ARFGameMode* GameMode = GetWorld() != nullptr
			? GetWorld()->GetAuthGameMode<ARFGameMode>() : nullptr;
		if (GameMode != nullptr && GameMode->IsEndlessMode())
		{
			const float PrepRemainingS = GameMode->GetWavePrepRemainingS();
			const int32 CurrentWave = GameState->GetEndlessWave();
			FString WaveStatus;
			if (PrepRemainingS > 0.0f)
			{
				WaveStatus = FString::Printf(TEXT("无限战线  波次 %d  整备 %.0f秒（空格开始下一波）"),
					CurrentWave, PrepRemainingS);
			}
			else if (GameMode->IsEndlessWaveActive())
			{
				WaveStatus = FString::Printf(TEXT("无限战线  波次 %d  交战中"), CurrentWave);
			}
			else
			{
				WaveStatus = FString::Printf(TEXT("无限战线  整备完成（空格开始第 %d 波）"),
					CurrentWave + 1);
			}
			DrawLine(WaveStatus, FLinearColor(0.95f, 0.68f, 0.32f));
		}

		for (const FRFObjectiveDef& Objective : Level.Objectives)
		{
			if (GameMode != nullptr && GameMode->IsEndlessMode())
			{
				break;
			}
			if (Y + LineHeight > PanelOrigin.Y + PanelHeight)
			{
				break;
			}
			const ERFObjectiveState State = GameState->GetObjectiveState(Objective.Id);
			const FLinearColor Color = State == ERFObjectiveState::Completed
				? FLinearColor(0.45f, 0.86f, 0.55f)
				: State == ERFObjectiveState::Failed
					? FLinearColor(0.95f, 0.38f, 0.32f)
					: FLinearColor(0.82f, 0.86f, 0.84f);
			FString ObjectiveLine = FString::Printf(TEXT("[%s] %s"),
				*ObjectiveStateLabel(State), *Objective.TextZh.ToString());
			if (Objective.Condition == FName(TEXT("hold_position"))
				&& State == ERFObjectiveState::Active)
			{
				ObjectiveLine += FString::Printf(TEXT("  %.0f/%.0f秒"),
					GameState->GetObjectiveProgress(Objective.Id), Objective.HoldTimeS);
			}
			else if (Objective.Condition == FName(TEXT("eliminate_count"))
				&& State == ERFObjectiveState::Active)
			{
				ObjectiveLine += FString::Printf(TEXT("  %.0f/%d"),
					GameState->GetObjectiveProgress(Objective.Id), Objective.TargetCount);
			}
			else if (Objective.Condition == FName(TEXT("rescue_count"))
				&& State == ERFObjectiveState::Active)
			{
				ObjectiveLine += FString::Printf(TEXT("  幸存者 %.0f/%d（按 E 护送）"),
					GameState->GetObjectiveProgress(Objective.Id), Objective.TargetCount);
			}
			else if (Objective.Condition == FName(TEXT("destroy_target"))
				&& State == ERFObjectiveState::Active)
			{
				ObjectiveLine += FString::Printf(TEXT("  目标 %.0f/%d"),
					GameState->GetObjectiveProgress(Objective.Id), Objective.TargetCount);
				ObjectiveLine += TEXT("（摧毁目标物）");
			}
			else if (Objective.Condition == FName(TEXT("reach_location"))
				&& State == ERFObjectiveState::Active)
			{
				ObjectiveLine += TEXT("  前往目标区域");
			}
			DrawLine(ObjectiveLine, Color);
		}
	}

	if (Soldier == nullptr)
	{
		return;
	}
	const float StatusY = Canvas->SizeY - 56.0f;
	DrawRect(FLinearColor(0.015f, 0.025f, 0.03f, 0.82f),
		16.0f, StatusY - 8.0f, FMath::Min(470.0f, Canvas->SizeX - 32.0f), 42.0f);

	if (URFSurvivalComponent* Survival = Soldier->GetSurvivalComponent())
	{
		DrawColoredText(Canvas, Font,
			FString::Printf(TEXT("生命 %.0f   口粮 %.0f   饮水 %.0f   体力 %.0f"),
				Survival->GetHealth(), Survival->GetFood(),
				Survival->GetHydration(), Survival->GetStamina()),
			28.0f, StatusY, FLinearColor::White);
	}
	if (URFWeaponBase* Weapon = Soldier->GetCurrentWeapon())
	{
		const FRFWeaponDef& Definition = Weapon->GetDefinition();
		DrawColoredText(Canvas, Font,
			FString::Printf(TEXT("%s   %d / %d"),
				*Definition.NameZh.ToString(),
				Weapon->GetMagazineAmmo(), Weapon->GetReserveAmmo()),
			28.0f, StatusY + LineHeight, FLinearColor(0.92f, 0.82f, 0.58f));
	}
}
