#include "RicochetHUD.h"

#include "RicochetCharacter.h"
#include "RicochetGameMode.h"
#include "RicochetMaterials.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void ARicochetHUD::DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale, UFont* Font)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void ARicochetHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>();
	const ARicochetCharacter* Player = Cast<ARicochetCharacter>(GetOwningPawn());
	if (!GM)
	{
		return;
	}

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();
	const float UI = FMath::Max(Canvas->ClipY / 1080.f, 0.6f);
	const FLinearColor Cyan = RicochetMat::Cyan;
	const FLinearColor Coral = RicochetMat::Coral;
	const FLinearColor Dim(0.55f, 0.6f, 0.65f, 0.9f);
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const ERicochetState State = GM->GetState();

	// Title / room counter
	DrawText(TEXT("BOUNCE BACK"), Dim, 40.f * UI, 30.f * UI, Small, 1.2f * UI);
	DrawText(FString::Printf(TEXT("ROOM %02d"), GM->GetRoomNumber()), FLinearColor::White, 40.f * UI, 52.f * UI, Big, 1.6f * UI);

	// Bullet count (top right)
	const bool bLoaded = Player && Player->HasBullet();
	const FString Ammo = bLoaded ? TEXT("1 BULLET") : TEXT("0 BULLETS");
	float AW = 0.f, AH = 0.f;
	GetTextSize(Ammo, AW, AH, Big, 1.4f * UI);
	DrawText(Ammo, bLoaded ? Cyan : Dim, Canvas->ClipX - AW - 40.f * UI, 40.f * UI, Big, 1.4f * UI);

	if (State == ERicochetState::Aiming)
	{
		// Crosshair
		const float S = 7.f * UI;
		DrawLine(CX - S, CY, CX - 2.f, CY, Cyan, 1.5f);
		DrawLine(CX + 2.f, CY, CX + S, CY, Cyan, 1.5f);
		DrawLine(CX, CY - S, CX, CY - 2.f, Cyan, 1.5f);
		DrawLine(CX, CY + 2.f, CX, CY + S, Cyan, 1.5f);

		DrawCentered(TEXT("ONE BULLET.  SWITCH  >  MIRROR  >  STRING  >  MIRROR  >  TARGET"), Canvas->ClipY - 110.f * UI, FLinearColor::White, 1.15f * UI, Small);
		DrawCentered(TEXT("Hit the glowing switch first, then follow the wires.   LMB fire  |  RMB steady aim  |  R restart  |  N new room"),
			Canvas->ClipY - 80.f * UI, Dim, 1.f * UI, Small);
	}
	else if (State == ERicochetState::InFlight)
	{
		FString Chain;
		for (const FString& S : GM->GetChainSteps())
		{
			Chain += S + TEXT("   ");
		}
		DrawCentered(Chain.IsEmpty() ? TEXT("...") : Chain, Canvas->ClipY - 100.f * UI, Cyan, 1.5f * UI, Big);
	}
	else if (State == ERicochetState::Failed)
	{
		const float Pulse = 0.75f + 0.25f * FMath::Sin(GM->GetStateAge() * 5.f);
		DrawCentered(TEXT("MISSED"), CY - 60.f * UI, Coral * Pulse, 3.f * UI, Big);
		DrawCentered(TEXT("R  retry this room      N  new room"), CY + 10.f * UI, FLinearColor::White, 1.3f * UI, Small);
	}
	else if (State == ERicochetState::Cleared)
	{
		DrawCentered(TEXT("ROOM CLEARED"), CY - 60.f * UI, Cyan, 3.f * UI, Big);
		DrawCentered(TEXT("next room incoming..."), CY + 10.f * UI, Dim, 1.2f * UI, Small);
	}
}
