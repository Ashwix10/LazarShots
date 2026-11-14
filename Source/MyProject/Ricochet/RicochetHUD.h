// Minimal canvas HUD: crosshair, room counter, bullet count, state prompts. No UMG assets needed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RicochetHUD.generated.h"

UCLASS()
class MYPROJECT_API ARicochetHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

private:

	void DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale, UFont* Font);
};
