// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RFGameHUD.generated.h"

/** Lightweight native mission HUD used until the final UMG art pass is ready. */
UCLASS()
class REDFRONT1941_API ARFGameHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
