// Game flow: one room per level load. Seed + room number travel through the URL options,
// so R (retry) reloads the exact same room and clearing it loads a fresh random one.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RicochetGameMode.generated.h"

class ARicochetRoom;

UENUM(BlueprintType)
enum class ERicochetState : uint8
{
	Aiming,
	InFlight,
	Failed,
	Cleared
};

UCLASS()
class MYPROJECT_API ARicochetGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ARicochetGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	void NotifyFired();
	void NotifyBulletStopped();
	void NotifyChainStep(const FString& Step);
	void NotifyRoomCleared();

	/** Reloads the level. bNewRoom = fresh random layout, otherwise the same seed. */
	void ReloadRoom(bool bNewRoom, bool bAdvance = false);

	ERicochetState GetState() const { return State; }
	int32 GetRoomNumber() const { return RoomNumber; }
	int32 GetSeed() const { return Seed; }
	const TArray<FString>& GetChainSteps() const { return ChainSteps; }
	float GetStateAge() const;

private:

	void SanitizeLevel();
	void SetState(ERicochetState NewState);

	int32 Seed = 0;
	int32 RoomNumber = 1;
	ERicochetState State = ERicochetState::Aiming;
	double StateStartRealTime = 0.0;
	TArray<FString> ChainSteps;

	UPROPERTY(Transient)
	TObjectPtr<ARicochetRoom> Room;
};
