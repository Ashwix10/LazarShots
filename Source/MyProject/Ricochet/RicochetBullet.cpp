// Ricochet bullet for the "Bounce Back" jam puzzle shooter.

#include "RicochetBullet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "BulletHitInterface.h"
#include "RicochetMaterials.h"

ARicochetBullet::ARicochetBullet()
{
	PrimaryActorTick.bCanEverTick = false;

	// --- Collision (root) ---
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(4.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);   // never hit the shooter
	Collision->SetGenerateOverlapEvents(true); // needed to snap overlap-only triggers (the present's string)
	Collision->CanCharacterStepUpOn = ECB_No;
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	// --- Visual ---
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.08f)); // 100uu engine sphere -> 8uu diameter, matches radius 4
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	// --- Chase camera (used as view target while the bullet flies in slow motion) ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = 140.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 25.f);
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 7.f;
	CameraBoom->bUsePawnControlRotation = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->SetFieldOfView(95.f);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(Collision);
	GlowLight->SetIntensityUnits(ELightUnits::Candelas);
	GlowLight->SetIntensity(6.f);
	GlowLight->SetAttenuationRadius(220.f);
	GlowLight->SetLightColor(RicochetMat::Cyan);
	GlowLight->SetCastShadows(false);

	// --- Movement: zero-loss bouncing ---
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = BulletSpeed;
	ProjectileMovement->MaxSpeed = BulletSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 1.f;
	ProjectileMovement->Friction = 0.f;
	ProjectileMovement->MinFrictionFraction = 0.f;
	ProjectileMovement->bBounceAngleAffectsFriction = false;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 1.f;
	ProjectileMovement->bSweepCollision = true;           // swept moves = no tunnelling through thin mirrors
	ProjectileMovement->bForceSubStepping = true;         // fixed small steps, stable at any framerate / time dilation
	ProjectileMovement->MaxSimulationTimeStep = 0.005f;
	ProjectileMovement->MaxSimulationIterations = 16;
}

void ARicochetBullet::BeginPlay()
{
	Super::BeginPlay();

	ProjectileMovement->MaxSpeed = BulletSpeed;
	Mesh->SetMaterial(0, RicochetMat::Glow(this, RicochetMat::Cyan, 40.f));
	Mesh->SetRelativeScale3D(FVector(0.14f, 0.08f, 0.08f)); // slightly stretched along velocity = reads as a tracer
	ProjectileMovement->OnProjectileBounce.AddDynamic(this, &ARicochetBullet::HandleBounce);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARicochetBullet::HandleStop);

	if (AActor* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
	if (AActor* OwnerActor = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}

	SetLifeSpan(MaxLifetime);

	if (bAutoLaunch)
	{
		Launch(GetActorForwardVector());
	}
}

void ARicochetBullet::Launch(const FVector& Direction)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || bStopped)
	{
		return;
	}
	ProjectileMovement->Velocity = Dir * BulletSpeed;
}

void ARicochetBullet::HandleBounce(const FHitResult& Hit, const FVector& ImpactVelocity)
{
	if (bStopped)
	{
		return;
	}

	++BounceCount;

	// Perfect mirror reflection at constant speed. Uses Hit.Normal (sphere-sweep normal),
	// which is exactly what PredictRicochetPath uses, so preview == flight.
	const FVector InDir = ImpactVelocity.GetSafeNormal();
	ProjectileMovement->Velocity = FMath::GetReflectionVector(InDir, Hit.Normal) * BulletSpeed;

	// Chain reaction: tell whatever we hit.
	if (AActor* HitActor = Hit.GetActor())
	{
		if (HitActor->Implements<UBulletHitInterface>())
		{
			IBulletHitInterface::Execute_OnBulletHit(HitActor, this, Hit);
			if (!IsValid(this) || bStopped)
			{
				return;
			}
		}
	}

	OnRicochet.Broadcast(this, Hit, BounceCount);
	if (!IsValid(this) || bStopped)
	{
		return; // a listener stopped or destroyed us
	}

	BP_OnRicochet(Hit, BounceCount);
	if (!IsValid(this) || bStopped)
	{
		return;
	}

	if (MaxBounces > 0 && BounceCount >= MaxBounces)
	{
		StopBullet();
	}
}

void ARicochetBullet::HandleStop(const FHitResult& Hit)
{
	MarkStopped();
}

void ARicochetBullet::StopBullet()
{
	if (bStopped)
	{
		return;
	}
	ProjectileMovement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkStopped();
}

void ARicochetBullet::MarkStopped()
{
	if (bStopped)
	{
		return;
	}
	bStopped = true;
	SetLifeSpan(3.f); // linger briefly so the camera/VFX can settle
	OnBulletStopped.Broadcast(this);
}

void ARicochetBullet::LifeSpanExpired()
{
	MarkStopped(); // no-op if already stopped
	Super::LifeSpanExpired();
}

bool ARicochetBullet::PredictRicochetPath(const UObject* WorldContextObject, TSubclassOf<ARicochetBullet> BulletClass,
	FVector Start, FVector Direction, int32 NumBounces, float MaxSegmentLength,
	const TArray<AActor*>& ActorsToIgnore, TArray<FVector>& OutPoints, TArray<FHitResult>& OutHits)
{
	OutPoints.Reset();
	OutHits.Reset();

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	FVector Dir = Direction.GetSafeNormal();
	if (!World || Dir.IsNearlyZero())
	{
		return false;
	}

	// Mirror the real bullet's sweep: same radius, object channel and responses.
	const ARicochetBullet* CDO = BulletClass ? BulletClass->GetDefaultObject<ARicochetBullet>() : GetDefault<ARicochetBullet>();
	const USphereComponent* CDOSphere = CDO->Collision;
	const float Radius = CDOSphere ? CDOSphere->GetUnscaledSphereRadius() : 4.f;
	const ECollisionChannel Channel = CDOSphere ? CDOSphere->GetCollisionObjectType() : ECC_WorldDynamic;
	const FCollisionResponseParams ResponseParams = CDOSphere ? FCollisionResponseParams(CDOSphere->GetCollisionResponseToChannels()) : FCollisionResponseParams::DefaultResponseParam;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RicochetAimPreview), false);
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FVector Pos = Start;
	OutPoints.Add(Pos);

	for (int32 Segment = 0; Segment <= NumBounces; ++Segment)
	{
		const FVector End = Pos + Dir * MaxSegmentLength;
		FHitResult Hit;
		if (!World->SweepSingleByChannel(Hit, Pos, End, FQuat::Identity, Channel, Shape, QueryParams, ResponseParams))
		{
			OutPoints.Add(End);
			return false;
		}

		OutPoints.Add(Hit.Location);
		OutHits.Add(Hit);

		if (Hit.bStartPenetrating)
		{
			return true; // starting inside geometry; nothing sensible to reflect
		}

		Dir = FMath::GetReflectionVector(Dir, Hit.Normal).GetSafeNormal();
		Pos = Hit.Location + Hit.Normal * 0.1f; // nudge off the surface so the next sweep doesn't re-hit it
	}

	return true;
}

void ARicochetBullet::DrawRicochetPath(const UObject* WorldContextObject, const TArray<FVector>& Points,
	FLinearColor Color, float DashLength, float GapLength, float Thickness)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || Points.Num() < 2)
	{
		return;
	}

	const FColor DrawColor = Color.ToFColor(true);
	const float Step = FMath::Max(DashLength + GapLength, 1.f);

	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		const float Len = FVector::Dist(A, B);
		if (Len <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector D = (B - A) / Len;

		for (float t = 0.f; t < Len; t += Step)
		{
			DrawDebugLine(World, A + D * t, A + D * FMath::Min(t + DashLength, Len), DrawColor, false, -1.f, 0, Thickness);
		}

		if (i > 0)
		{
			DrawDebugPoint(World, A, 8.f, DrawColor, false, -1.f); // bounce marker
		}
	}
#endif
}
