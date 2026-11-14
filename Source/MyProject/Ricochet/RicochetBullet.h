// Ricochet bullet for the "Bounce Back" jam puzzle shooter.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RicochetBullet.generated.h"

class ARicochetBullet;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBulletRicochet, ARicochetBullet*, Bullet, const FHitResult&, Hit, int32, BounceCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBulletStopped, ARicochetBullet*, Bullet);

/**
 *  A single bullet that bounces forever with zero speed loss.
 *  Every bounce is re-snapped to a perfect mirror reflection at constant speed,
 *  so the in-flight path matches PredictRicochetPath() exactly.
 */
UCLASS(Blueprintable)
class MYPROJECT_API ARicochetBullet : public AActor
{
	GENERATED_BODY()

public:

	ARicochetBullet();

	/** Collision sphere (root). Its radius + collision responses are also used by the aim preview. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> Collision;

	/** Visual: engine basic sphere. Assign MI_Glow_Cyan in the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Slow-motion chase camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Small cyan light so the bullet paints the walls it passes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> GlowLight;

	/** Constant flight speed (cm/s). Never changes across bounces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet", meta=(ClampMin=100, Units="CentimetersPerSecond"))
	float BulletSpeed = 2500.f;

	/** Bullet stops after this many bounces. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet", meta=(ClampMin=0))
	int32 MaxBounces = 10;

	/** Safety lifetime in game seconds (time dilation stretches this in real time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet", meta=(ClampMin=0.5, Units="s"))
	float MaxLifetime = 4.f;

	/** If true, the bullet launches along its spawn rotation in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet")
	bool bAutoLaunch = true;

	/** Fired on every bounce (after the velocity has been reflected). */
	UPROPERTY(BlueprintAssignable, Category="Ricochet")
	FOnBulletRicochet OnRicochet;

	/** Fired once when the bullet stops (target hit, bounce limit, lifetime, or StopBullet). */
	UPROPERTY(BlueprintAssignable, Category="Ricochet")
	FOnBulletStopped OnBulletStopped;

	/** Sends the bullet along Direction at BulletSpeed. */
	UFUNCTION(BlueprintCallable, Category="Ricochet")
	void Launch(const FVector& Direction);

	/** Freezes the bullet, disables collision and broadcasts OnBulletStopped. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category="Ricochet")
	void StopBullet();

	UFUNCTION(BlueprintPure, Category="Ricochet")
	int32 GetBounceCount() const { return BounceCount; }

	UFUNCTION(BlueprintPure, Category="Ricochet")
	bool IsInFlight() const { return !bStopped; }

	/**
	 *  Simulates the bullet's path with sphere sweeps (same radius + collision responses as BulletClass).
	 *  OutPoints = Start, each bounce point, final point. OutHits = one entry per surface hit.
	 *  Returns true if the final segment ended on a surface (false = flew off into empty space).
	 */
	UFUNCTION(BlueprintCallable, Category="Ricochet|Aim", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool PredictRicochetPath(const UObject* WorldContextObject, TSubclassOf<ARicochetBullet> BulletClass,
		FVector Start, FVector Direction, int32 NumBounces, float MaxSegmentLength,
		const TArray<AActor*>& ActorsToIgnore, TArray<FVector>& OutPoints, TArray<FHitResult>& OutHits);

	/** Draws a dashed line through Points for one frame (debug draw: works in Editor/Development, not Shipping). */
	UFUNCTION(BlueprintCallable, Category="Ricochet|Aim", meta=(WorldContext="WorldContextObject"))
	static void DrawRicochetPath(const UObject* WorldContextObject, const TArray<FVector>& Points,
		FLinearColor Color = FLinearColor(0.f, 0.75f, 1.f), float DashLength = 20.f, float GapLength = 15.f, float Thickness = 1.5f);

protected:

	virtual void BeginPlay() override;
	virtual void LifeSpanExpired() override;

	/** Blueprint hook for VFX/SFX on each bounce. */
	UFUNCTION(BlueprintImplementableEvent, Category="Ricochet", meta=(DisplayName="On Ricochet"))
	void BP_OnRicochet(const FHitResult& Hit, int32 NewBounceCount);

private:

	UFUNCTION()
	void HandleBounce(const FHitResult& Hit, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& Hit);

	void MarkStopped();

	int32 BounceCount = 0;
	bool bStopped = false;
};
