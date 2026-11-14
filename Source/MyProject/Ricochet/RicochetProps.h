// Chain-reaction props built entirely from engine basic shapes.
// Convention: each prop's bounce surface is a flat face at the actor origin facing +X,
// so the room generator can orient it with the reflection-bisector normal.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletHitInterface.h"
#include "RicochetProps.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USpotLightComponent;
class UPointLightComponent;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

UCLASS(Abstract)
class MYPROJECT_API ARicochetProp : public AActor, public IBulletHitInterface
{
	GENERATED_BODY()

public:

	ARicochetProp();

	/** Prop that UnlockProp() is sent to when this one fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ricochet", meta=(ExposeOnSpawn))
	TObjectPtr<AActor> NextActorToUnlock;

	/** Only unlocked props react to the bullet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ricochet", meta=(ExposeOnSpawn))
	bool bIsUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category="Ricochet")
	bool bTriggered = false;

	virtual void OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit) override;
	virtual void UnlockProp_Implementation() override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	/** Constructor helper: creates a basic-shape mesh child with no collision. */
	UStaticMeshComponent* MakeShape(FName Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale,
		const FRotator& Rot = FRotator::ZeroRotator, USceneComponent* Parent = nullptr);

	static void MakeSolid(UPrimitiveComponent* C);
	static void MakeTrigger(UPrimitiveComponent* C);

	/** Applies a glow MID to a mesh and registers it for flash/dim animation. */
	UMaterialInstanceDynamic* ApplyGlow(UStaticMeshComponent* C, const FLinearColor& Color, float Strength);

	void UnlockNext();
	void Flash(float Amount = 1.f) { FlashAmount = FMath::Max(FlashAmount, Amount); }
	void SetGlowScale(float Scale) { GlowScale = Scale; }

	virtual void OnUnlocked() {}

private:

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;
	TArray<float> GlowBase;

	float FlashAmount = 0.f;
	float GlowScale = 1.f;
};

/** First bounce: flips on the room lights + volumetric fog, unlocks the next prop. */
UCLASS()
class MYPROJECT_API ALightSwitchProp : public ARicochetProp
{
	GENERATED_BODY()

public:

	ALightSwitchProp();
	virtual void OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit) override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Plate;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Button;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<USpotLightComponent> SoftSpot;
};

/** Angled metallic mirror panel with a glowing frame. */
UCLASS()
class MYPROJECT_API AMirrorProp : public ARicochetProp
{
	GENERATED_BODY()

public:

	AMirrorProp();
	virtual void OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit) override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Panel;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Frame;
};

/** Gift box hanging from a glowing string. The bullet passing through the string snaps it. */
UCLASS()
class MYPROJECT_API AHangingPresentProp : public ARicochetProp
{
	GENERATED_BODY()

public:

	AHangingPresentProp();

	/** String length from the box top up to the ceiling anchor. */
	void SetStringLength(float Length);

	virtual void OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit) override;
	virtual void Tick(float DeltaSeconds) override;

	static constexpr float BoxHalf = 35.f;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Box;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Lid;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> RibbonA;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> RibbonB;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> String;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Anchor;
	/** Generous invisible trigger around the string so the shot is humanly possible. */
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UCapsuleComponent> StringTrigger;

private:

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Snap();

	bool bBursting = false;
	float BurstTime = 0.f;
	static constexpr float BurstDuration = 0.45f;
};

/** The coral orb. Solid from the start (it's just a wall until unlocked). */
UCLASS()
class MYPROJECT_API AFinalTargetProp : public ARicochetProp
{
	GENERATED_BODY()

public:

	AFinalTargetProp();
	virtual void OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit) override;
	virtual void Tick(float DeltaSeconds) override;

	static constexpr float OrbRadius = 25.f;

protected:

	virtual void BeginPlay() override;
	virtual void OnUnlocked() override;

	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UStaticMeshComponent> Orb;
	UPROPERTY(VisibleAnywhere, Category="Components") TObjectPtr<UPointLightComponent> Halo;

private:

	void Shatter(const FVector& FromDir);
	float Age = 0.f;
};
