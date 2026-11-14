// The player: stands on the firing pad with a blockout pistol and exactly one bullet.
// Input is created in code (no input assets needed).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RicochetCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class ARicochetBullet;
struct FInputActionValue;

UCLASS()
class MYPROJECT_API ARicochetCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	ARicochetCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Ricochet")
	int32 AimBounces = 2;

	UPROPERTY(EditAnywhere, Category="Ricochet")
	float SlowMoDilation = 0.25f;

	UPROPERTY(EditAnywhere, Category="Ricochet")
	float LookSensitivity = 1.f;

	/** Look-speed multiplier while holding right mouse. */
	UPROPERTY(EditAnywhere, Category="Ricochet")
	float SteadyAimScale = 0.2f;

	bool HasBullet() const { return bHasBullet; }

protected:

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:

	void Look(const FInputActionValue& Value);
	void Fire();
	void Retry();
	void NewRoom();
	void SteadyOn() { bSteady = true; }
	void SteadyOff() { bSteady = false; }

	UFUNCTION()
	void HandleBulletStopped(ARicochetBullet* Bullet);

	void UpdateAimLine();
	FVector MuzzleStart(FVector& OutDir) const;

	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<USceneComponent> GunRoot;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> GunBody;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> GunGrip;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> GunBarrel;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> GunStrip;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> GunChamber;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UInstancedStaticMeshComponent> AimDashes;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UInstancedStaticMeshComponent> AimDots;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RetryAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NewRoomAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteadyAction;

	UPROPERTY(Transient) TObjectPtr<ARicochetBullet> ActiveBullet;

	bool bHasBullet = true;
	bool bSteady = false;
	float Recoil = 0.f;
};
