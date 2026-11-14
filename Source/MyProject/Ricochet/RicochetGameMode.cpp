#include "RicochetGameMode.h"

#include "RicochetCharacter.h"
#include "RicochetHUD.h"
#include "RicochetMaterials.h"
#include "RicochetRoom.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ARicochetGameMode::ARicochetGameMode()
{
	DefaultPawnClass = ARicochetCharacter::StaticClass();
	HUDClass = ARicochetHUD::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
}

void ARicochetGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString SeedOpt = UGameplayStatics::ParseOption(Options, TEXT("Seed"));
	Seed = SeedOpt.IsEmpty() ? FMath::RandRange(1, 999999) : FCString::Atoi(*SeedOpt);
	const FString RoomOpt = UGameplayStatics::ParseOption(Options, TEXT("Room"));
	RoomNumber = RoomOpt.IsEmpty() ? 1 : FMath::Max(1, FCString::Atoi(*RoomOpt));
}

void ARicochetGameMode::SanitizeLevel()
{
	// The game builds its own room/lighting. Remove any sky, sun, fog and floor that came with the map
	// (runtime only: the map asset on disk is untouched).
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == this || A->IsA<AWorldSettings>() || A->IsA<APlayerController>() || A->IsA<APawn>())
		{
			continue;
		}
		const bool bKill =
			A->IsA<ALight>() || A->IsA<ASkyLight>() || A->IsA<AExponentialHeightFog>() ||
			A->IsA<APostProcessVolume>() || A->IsA<AStaticMeshActor>() ||
			A->FindComponentByClass<USkyAtmosphereComponent>() || A->FindComponentByClass<UVolumetricCloudComponent>() ||
			A->GetClass()->GetName().Contains(TEXT("Sky"));
		if (bKill)
		{
			A->Destroy();
		}
	}
}

void ARicochetGameMode::StartPlay()
{
	SanitizeLevel();
	RicochetMat::EnsureAssets();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.bDeferConstruction = true;
	Room = GetWorld()->SpawnActor<ARicochetRoom>(ARicochetRoom::StaticClass(), FTransform::Identity, P);
	if (Room)
	{
		Room->Seed = Seed;
		Room->FinishSpawning(FTransform::Identity);
	}

	Super::StartPlay();
	SetState(ERicochetState::Aiming);
}

void ARicochetGameMode::RestartPlayer(AController* NewPlayer)
{
	// Fixed firing position (the puzzle is generated from exactly this eye point).
	RestartPlayerAtTransform(NewPlayer, ARicochetRoom::GetPlayerSpawnTransform());
}

void ARicochetGameMode::SetState(ERicochetState NewState)
{
	State = NewState;
	StateStartRealTime = FPlatformTime::Seconds();
}

float ARicochetGameMode::GetStateAge() const
{
	return static_cast<float>(FPlatformTime::Seconds() - StateStartRealTime);
}

void ARicochetGameMode::NotifyFired()
{
	ChainSteps.Reset();
	SetState(ERicochetState::InFlight);
}

void ARicochetGameMode::NotifyChainStep(const FString& Step)
{
	ChainSteps.Add(Step);
}

void ARicochetGameMode::NotifyBulletStopped()
{
	if (State == ERicochetState::InFlight)
	{
		SetState(ERicochetState::Failed);
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
}

void ARicochetGameMode::NotifyRoomCleared()
{
	if (State == ERicochetState::Cleared)
	{
		return;
	}
	ChainSteps.Add(TEXT("TARGET"));
	SetState(ERicochetState::Cleared);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f); // before any timer, or it would run 4x slower

	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ReloadRoom(true, true);
	}), 2.5f, false);
}

void ARicochetGameMode::ReloadRoom(bool bNewRoom, bool bAdvance)
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	const int32 NextSeed = bNewRoom ? FMath::RandRange(1, 999999) : Seed;
	const int32 NextRoom = bAdvance ? RoomNumber + 1 : RoomNumber;
	const FString Options = FString::Printf(TEXT("Seed=%d?Room=%d?game=/Script/MyProject.RicochetGameMode"), NextSeed, NextRoom);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, Options);
}
