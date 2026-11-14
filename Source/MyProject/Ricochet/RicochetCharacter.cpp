#include "RicochetCharacter.h"

#include "RicochetBullet.h"
#include "RicochetGameMode.h"
#include "RicochetMaterials.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	UStaticMeshComponent* GunPart(ACharacter* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh,
		const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
		return C;
	}

	UInstancedStaticMeshComponent* MakeISM(ACharacter* Owner, FName Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetStaticMesh(Mesh);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		return C;
	}
}

ARicochetCharacter::ARicochetCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 96.f);
	bUseControllerRotationYaw = true;
	BaseEyeHeight = 64.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.f);

	GunRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GunRoot"));
	GunRoot->SetupAttachment(Camera);
	GunRoot->SetRelativeLocation(FVector(34.f, 15.f, -17.f));

	UStaticMesh* Cube = RicochetMat::CubeMesh();
	GunBody = GunPart(this, TEXT("GunBody"), GunRoot, Cube, FVector::ZeroVector, FVector(0.3f, 0.065f, 0.1f));
	GunGrip = GunPart(this, TEXT("GunGrip"), GunRoot, Cube, FVector(-9.f, 0.f, -9.f), FVector(0.07f, 0.055f, 0.16f), FRotator(-15.f, 0.f, 0.f));
	GunBarrel = GunPart(this, TEXT("GunBarrel"), GunRoot, RicochetMat::CylinderMesh(), FVector(20.f, 0.f, 1.5f), FVector(0.04f, 0.04f, 0.16f), FRotator(90.f, 0.f, 0.f));
	GunStrip = GunPart(this, TEXT("GunStrip"), GunRoot, Cube, FVector(0.f, 0.f, 5.4f), FVector(0.24f, 0.012f, 0.012f));
	GunChamber = GunPart(this, TEXT("GunChamber"), GunRoot, RicochetMat::SphereMesh(), FVector(6.f, 3.6f, 1.f), FVector(0.035f));

	AimDashes = MakeISM(this, TEXT("AimDashes"), RicochetMat::CylinderMesh());
	AimDots = MakeISM(this, TEXT("AimDots"), RicochetMat::SphereMesh());
}

void ARicochetCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Stand still on the firing pad: the room is generated from this exact eye point.
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->DisableMovement();

	const FLinearColor Gunmetal(0.03f, 0.03f, 0.035f);
	GunBody->SetMaterial(0, RicochetMat::Surface(this, Gunmetal, 0.35f, 0.3f));
	GunGrip->SetMaterial(0, RicochetMat::Surface(this, Gunmetal, 0.6f, 0.f));
	GunBarrel->SetMaterial(0, RicochetMat::Surface(this, FLinearColor(0.6f, 0.62f, 0.65f), 0.2f, 1.f));
	GunStrip->SetMaterial(0, RicochetMat::Glow(this, RicochetMat::Cyan, 8.f));
	GunChamber->SetMaterial(0, RicochetMat::Glow(this, RicochetMat::Cyan, 30.f));
	AimDashes->SetMaterial(0, RicochetMat::Glow(this, RicochetMat::Cyan, 6.f));
	AimDots->SetMaterial(0, RicochetMat::Glow(this, RicochetMat::Cyan, 25.f));

	AimDashes->SetWorldTransform(FTransform::Identity);
	AimDots->SetWorldTransform(FTransform::Identity);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->SetControlRotation(FRotator::ZeroRotator);
	}
}

void ARicochetCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto MakeAction = [this](EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this);
		A->ValueType = Type;
		return A;
	};

	Mapping = NewObject<UInputMappingContext>(this);
	LookAction = MakeAction(EInputActionValueType::Axis2D);
	FireAction = MakeAction(EInputActionValueType::Boolean);
	RetryAction = MakeAction(EInputActionValueType::Boolean);
	NewRoomAction = MakeAction(EInputActionValueType::Boolean);
	SteadyAction = MakeAction(EInputActionValueType::Boolean);

	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	Mapping->MapKey(FireAction, EKeys::LeftMouseButton);
	Mapping->MapKey(FireAction, EKeys::SpaceBar);
	Mapping->MapKey(RetryAction, EKeys::R);
	Mapping->MapKey(NewRoomAction, EKeys::N);
	Mapping->MapKey(SteadyAction, EKeys::RightMouseButton);
	Mapping->MapKey(SteadyAction, EKeys::LeftShift);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARicochetCharacter::Look);
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &ARicochetCharacter::Fire);
		EIC->BindAction(RetryAction, ETriggerEvent::Started, this, &ARicochetCharacter::Retry);
		EIC->BindAction(NewRoomAction, ETriggerEvent::Started, this, &ARicochetCharacter::NewRoom);
		EIC->BindAction(SteadyAction, ETriggerEvent::Started, this, &ARicochetCharacter::SteadyOn);
		EIC->BindAction(SteadyAction, ETriggerEvent::Completed, this, &ARicochetCharacter::SteadyOff);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->ClearAllMappings();
				Sub->AddMappingContext(Mapping, 0);
			}
		}
	}
}

void ARicochetCharacter::Look(const FInputActionValue& Value)
{
	if (!bHasBullet)
	{
		return; // camera belongs to the bullet now
	}
	const FVector2D V = Value.Get<FVector2D>();
	const float S = LookSensitivity * (bSteady ? SteadyAimScale : 1.f);
	AddControllerYawInput(V.X * S);
	AddControllerPitchInput(-V.Y * S);
}

FVector ARicochetCharacter::MuzzleStart(FVector& OutDir) const
{
	// Fire along the exact view ray so the dashed preview == the crosshair == the shot.
	OutDir = Camera->GetForwardVector();
	return Camera->GetComponentLocation() + OutDir * 30.f;
}

void ARicochetCharacter::Fire()
{
	ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>();
	if (!bHasBullet || (GM && GM->GetState() != ERicochetState::Aiming))
	{
		return;
	}

	FVector Dir;
	const FVector Start = MuzzleStart(Dir);

	FActorSpawnParameters P;
	P.Owner = this;
	P.Instigator = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveBullet = GetWorld()->SpawnActor<ARicochetBullet>(ARicochetBullet::StaticClass(), Start, Dir.Rotation(), P);
	if (!ActiveBullet)
	{
		return;
	}

	bHasBullet = false;
	Recoil = 1.f;
	GunChamber->SetVisibility(false);
	AimDashes->ClearInstances();
	AimDots->ClearInstances();

	ActiveBullet->OnBulletStopped.AddDynamic(this, &ARicochetCharacter::HandleBulletStopped);

	if (GM)
	{
		GM->NotifyFired();
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(ActiveBullet, 0.2f, VTBlend_EaseOut, 2.f);
	}
	UGameplayStatics::SetGlobalTimeDilation(this, SlowMoDilation);
}

void ARicochetCharacter::HandleBulletStopped(ARicochetBullet* Bullet)
{
	ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>();
	if (GM)
	{
		GM->NotifyBulletStopped();
	}
	if (GM && GM->GetState() == ERicochetState::Cleared)
	{
		return; // stay on the bullet cam for the win moment
	}

	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->SetViewTargetWithBlend(this, 0.4f, VTBlend_EaseInOut, 2.f);
		}
	}), 0.6f, false);
}

void ARicochetCharacter::Retry()
{
	if (ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>())
	{
		GM->ReloadRoom(false);
	}
}

void ARicochetCharacter::NewRoom()
{
	if (ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>())
	{
		GM->ReloadRoom(true);
	}
}

void ARicochetCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Tiny recoil kick on the blockout gun.
	Recoil = FMath::FInterpTo(Recoil, 0.f, DeltaSeconds, 10.f);
	GunRoot->SetRelativeLocation(FVector(34.f - 6.f * Recoil, 15.f, -17.f + 1.5f * Recoil));
	GunRoot->SetRelativeRotation(FRotator(12.f * Recoil, 0.f, 0.f));

	if (bHasBullet)
	{
		UpdateAimLine();
	}
}

void ARicochetCharacter::UpdateAimLine()
{
	FVector Dir;
	const FVector Start = MuzzleStart(Dir);

	TArray<FVector> Points;
	TArray<FHitResult> Hits;
	ARicochetBullet::PredictRicochetPath(this, ARicochetBullet::StaticClass(), Start, Dir, AimBounces, 6000.f,
		TArray<AActor*>{ this }, Points, Hits);

	// Dashed beam made of tiny glowing cylinders (works in Shipping builds, unlike debug lines).
	TArray<FTransform> Dashes;
	TArray<FTransform> Dots;
	constexpr float Dash = 22.f;
	constexpr float Gap = 14.f;
	constexpr int32 MaxDashes = 400;

	for (int32 i = 0; i + 1 < Points.Num() && Dashes.Num() < MaxDashes; ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		const float Len = FVector::Dist(A, B);
		if (Len < 1.f)
		{
			continue;
		}
		const FVector D = (B - A) / Len;
		const FQuat Q = FRotationMatrix::MakeFromZ(D).ToQuat();
		const float Thick = i == 0 ? 0.008f : 0.012f;
		for (float t = (i == 0 ? 20.f : 0.f); t < Len && Dashes.Num() < MaxDashes; t += Dash + Gap)
		{
			const float L = FMath::Min(Dash, Len - t);
			Dashes.Add(FTransform(Q, A + D * (t + L * 0.5f), FVector(Thick, Thick, L / 100.f)));
		}
		if (i > 0 || Hits.Num() > 0)
		{
			Dots.Add(FTransform(FQuat::Identity, B, FVector(0.07f)));
		}
	}

	AimDashes->ClearInstances();
	AimDots->ClearInstances();
	AimDashes->AddInstances(Dashes, false, true);
	AimDots->AddInstances(Dots, false, true);
}
