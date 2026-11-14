#include "RicochetProps.h"

#include "RicochetBullet.h"
#include "RicochetGameMode.h"
#include "RicochetMaterials.h"
#include "RicochetRoom.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

// ---------------------------------------------------------------------------------------------
// Base
// ---------------------------------------------------------------------------------------------

ARicochetProp::ARicochetProp()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UStaticMeshComponent* ARicochetProp::MakeShape(FName Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale,
	const FRotator& Rot, USceneComponent* Parent)
{
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Parent ? Parent : Root.Get());
	C->SetStaticMesh(Mesh);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	return C;
}

void ARicochetProp::MakeSolid(UPrimitiveComponent* C)
{
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetGenerateOverlapEvents(false);
}

void ARicochetProp::MakeTrigger(UPrimitiveComponent* C)
{
	// Bullet is WorldDynamic + blocks everything -> Block vs Overlap resolves to Overlap: it flies straight through.
	C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	C->SetCollisionObjectType(ECC_WorldDynamic);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	C->SetGenerateOverlapEvents(true);
}

UMaterialInstanceDynamic* ARicochetProp::ApplyGlow(UStaticMeshComponent* C, const FLinearColor& Color, float Strength)
{
	UMaterialInstanceDynamic* MID = RicochetMat::Glow(this, Color, Strength);
	C->SetMaterial(0, MID);
	C->SetCastShadow(false);
	GlowMIDs.Add(MID);
	GlowBase.Add(Strength);
	return MID;
}

void ARicochetProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FlashAmount = FMath::Max(0.f, FlashAmount - DeltaSeconds * 3.f);
	const float Mult = GlowScale * (1.f + FlashAmount * 6.f);
	for (int32 i = 0; i < GlowMIDs.Num(); ++i)
	{
		if (GlowMIDs[i])
		{
			GlowMIDs[i]->SetScalarParameterValue(TEXT("Strength"), GlowBase[i] * Mult);
		}
	}
}

void ARicochetProp::OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit)
{
	Flash(0.5f);
}

void ARicochetProp::UnlockProp_Implementation()
{
	if (bIsUnlocked)
	{
		return;
	}
	bIsUnlocked = true;
	Flash(0.6f);
	OnUnlocked();
}

void ARicochetProp::UnlockNext()
{
	if (IsValid(NextActorToUnlock) && NextActorToUnlock->Implements<UBulletHitInterface>())
	{
		IBulletHitInterface::Execute_UnlockProp(NextActorToUnlock);
	}
}

// ---------------------------------------------------------------------------------------------
// Light switch
// ---------------------------------------------------------------------------------------------

ALightSwitchProp::ALightSwitchProp()
{
	bIsUnlocked = true;

	Plate = MakeShape(TEXT("Plate"), RicochetMat::CubeMesh(), FVector(-2.5f, 0.f, 0.f), FVector(0.05f, 0.75f, 1.05f));
	MakeSolid(Plate);
	Button = MakeShape(TEXT("Button"), RicochetMat::CubeMesh(), FVector(0.4f, 0.f, 0.f), FVector(0.01f, 0.32f, 0.5f));
	Frame = MakeShape(TEXT("Frame"), RicochetMat::CubeMesh(), FVector(-5.5f, 0.f, 0.f), FVector(0.03f, 0.83f, 1.13f));

	SoftSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("SoftSpot"));
	SoftSpot->SetupAttachment(Root);
	SoftSpot->SetRelativeLocation(FVector(90.f, 0.f, 120.f));
	SoftSpot->SetRelativeRotation((FVector(-90.f, 0.f, -120.f)).Rotation());
	SoftSpot->SetIntensityUnits(ELightUnits::Candelas);
	SoftSpot->SetIntensity(18.f);
	SoftSpot->SetLightColor(FLinearColor(0.7f, 0.95f, 1.f));
	SoftSpot->SetInnerConeAngle(10.f);
	SoftSpot->SetOuterConeAngle(32.f);
	SoftSpot->SetAttenuationRadius(450.f);
	SoftSpot->SetVolumetricScatteringIntensity(2.f);
	SoftSpot->SetCastShadows(false);
}

void ALightSwitchProp::BeginPlay()
{
	Super::BeginPlay();
	Plate->SetMaterial(0, RicochetMat::Surface(this, FLinearColor(0.05f, 0.05f, 0.06f), 0.35f));
	ApplyGlow(Button, RicochetMat::Cyan, 14.f);
	ApplyGlow(Frame, RicochetMat::Cyan, 3.f);
}

void ALightSwitchProp::OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit)
{
	Flash(1.f);
	if (!bIsUnlocked || bTriggered)
	{
		return;
	}
	bTriggered = true;

	for (TActorIterator<ARicochetRoom> It(GetWorld()); It; ++It)
	{
		It->LightsOn();
	}
	if (ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>())
	{
		GM->NotifyChainStep(TEXT("LIGHTS"));
	}
	UnlockNext();
}

// ---------------------------------------------------------------------------------------------
// Mirror
// ---------------------------------------------------------------------------------------------

AMirrorProp::AMirrorProp()
{
	bIsUnlocked = true;
	Panel = MakeShape(TEXT("Panel"), RicochetMat::CubeMesh(), FVector(-2.5f, 0.f, 0.f), FVector(0.05f, 1.3f, 1.3f));
	MakeSolid(Panel);
	Frame = MakeShape(TEXT("Frame"), RicochetMat::CubeMesh(), FVector(-5.5f, 0.f, 0.f), FVector(0.03f, 1.4f, 1.4f));
}

void AMirrorProp::BeginPlay()
{
	Super::BeginPlay();
	Panel->SetMaterial(0, RicochetMat::Mirror(this));
	ApplyGlow(Frame, RicochetMat::Cyan, 4.f);
}

void AMirrorProp::OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit)
{
	Flash(1.f);
}

// ---------------------------------------------------------------------------------------------
// Hanging present
// ---------------------------------------------------------------------------------------------

AHangingPresentProp::AHangingPresentProp()
{
	const float S = BoxHalf / 50.f; // cube is 100uu
	Box = MakeShape(TEXT("Box"), RicochetMat::CubeMesh(), FVector::ZeroVector, FVector(S));
	MakeTrigger(Box);
	RibbonA = MakeShape(TEXT("RibbonA"), RicochetMat::CubeMesh(), FVector::ZeroVector, FVector(1.03f, 0.16f, 1.03f), FRotator::ZeroRotator, Box);
	RibbonB = MakeShape(TEXT("RibbonB"), RicochetMat::CubeMesh(), FVector::ZeroVector, FVector(0.16f, 1.03f, 1.03f), FRotator::ZeroRotator, Box);
	Lid = MakeShape(TEXT("Lid"), RicochetMat::CubeMesh(), FVector(0.f, 0.f, BoxHalf + 6.f), FVector(S * 1.1f, S * 1.1f, 0.12f));

	String = MakeShape(TEXT("String"), RicochetMat::CylinderMesh(), FVector::ZeroVector, FVector(0.03f, 0.03f, 1.f));
	Anchor = MakeShape(TEXT("Anchor"), RicochetMat::CubeMesh(), FVector::ZeroVector, FVector(0.25f, 0.25f, 0.06f));

	StringTrigger = CreateDefaultSubobject<UCapsuleComponent>(TEXT("StringTrigger"));
	StringTrigger->SetupAttachment(Root);
	StringTrigger->SetCapsuleRadius(22.f);
	MakeTrigger(StringTrigger);

	SetStringLength(300.f);
}

void AHangingPresentProp::SetStringLength(float Length)
{
	Length = FMath::Max(Length, 20.f);
	const float Base = BoxHalf + 12.f;
	String->SetRelativeLocation(FVector(0.f, 0.f, Base + Length * 0.5f));
	String->SetRelativeScale3D(FVector(0.03f, 0.03f, Length / 100.f));
	StringTrigger->SetRelativeLocation(String->GetRelativeLocation());
	StringTrigger->SetCapsuleHalfHeight(Length * 0.5f + 22.f);
	Anchor->SetRelativeLocation(FVector(0.f, 0.f, Base + Length));
}

void AHangingPresentProp::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Paper(0.85f, 0.86f, 0.9f);
	Box->SetMaterial(0, RicochetMat::Surface(this, Paper, 0.5f));
	Lid->SetMaterial(0, RicochetMat::Surface(this, Paper, 0.5f));
	Anchor->SetMaterial(0, RicochetMat::Surface(this, FLinearColor(0.03f, 0.03f, 0.035f), 0.4f));
	ApplyGlow(RibbonA, RicochetMat::Coral, 6.f);
	ApplyGlow(RibbonB, RicochetMat::Coral, 6.f);
	ApplyGlow(String, RicochetMat::Cyan, 10.f);

	Box->OnComponentBeginOverlap.AddDynamic(this, &AHangingPresentProp::HandleOverlap);
	StringTrigger->OnComponentBeginOverlap.AddDynamic(this, &AHangingPresentProp::HandleOverlap);
}

void AHangingPresentProp::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<ARicochetBullet>(OtherActor))
	{
		Snap();
	}
}

void AHangingPresentProp::OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit)
{
	Snap();
}

void AHangingPresentProp::Snap()
{
	if (!bIsUnlocked || bTriggered)
	{
		return;
	}
	bTriggered = true;

	String->SetVisibility(false);
	StringTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flash(1.f);

	// Unlock immediately: the target must be live before the bullet arrives, whatever the animation is doing.
	UnlockNext();
	if (ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>())
	{
		GM->NotifyChainStep(TEXT("SNAP"));
	}

	bBursting = true;
	BurstTime = 0.f;
}

void AHangingPresentProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBursting)
	{
		return;
	}

	// Game-time animation: slow motion stretches it exactly like the bullet.
	BurstTime += DeltaSeconds;
	const float A = FMath::Clamp(BurstTime / BurstDuration, 0.f, 1.f);
	const float Shrink = A < 0.55f ? 1.f : 1.f - (A - 0.55f) / 0.45f;
	const float S = BoxHalf / 50.f;

	Box->SetRelativeLocation(FVector(0.f, 0.f, -90.f * A * A));
	Box->SetRelativeRotation(FRotator(25.f * A, 80.f * A, 0.f));
	Box->SetRelativeScale3D(FVector(S * (1.f + 0.35f * A) * Shrink));

	Lid->SetRelativeLocation(FVector(45.f * A, 0.f, BoxHalf + 6.f + 170.f * A));
	Lid->SetRelativeRotation(FRotator(0.f, 30.f * A, 175.f * A));
	Lid->SetRelativeScale3D(FVector(S * 1.1f, S * 1.1f, 0.12f) * Shrink);

	if (A >= 1.f)
	{
		bBursting = false;
		Box->SetVisibility(false, true);
		Lid->SetVisibility(false);
	}
}

// ---------------------------------------------------------------------------------------------
// Final target
// ---------------------------------------------------------------------------------------------

AFinalTargetProp::AFinalTargetProp()
{
	Orb = MakeShape(TEXT("Orb"), RicochetMat::SphereMesh(), FVector::ZeroVector, FVector(OrbRadius / 50.f));
	MakeSolid(Orb);

	Halo = CreateDefaultSubobject<UPointLightComponent>(TEXT("Halo"));
	Halo->SetupAttachment(Root);
	Halo->SetIntensityUnits(ELightUnits::Candelas);
	Halo->SetIntensity(25.f);
	Halo->SetAttenuationRadius(380.f);
	Halo->SetLightColor(RicochetMat::Coral);
	Halo->SetCastShadows(false);
	Halo->SetVisibility(false);
}

void AFinalTargetProp::BeginPlay()
{
	Super::BeginPlay();
	ApplyGlow(Orb, RicochetMat::Coral, 14.f);
	SetGlowScale(bIsUnlocked ? 1.f : 0.12f);
	if (bIsUnlocked)
	{
		OnUnlocked();
	}
}

void AFinalTargetProp::OnUnlocked()
{
	SetGlowScale(1.f);
	Halo->SetVisibility(true);
}

void AFinalTargetProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (bIsUnlocked && !bTriggered)
	{
		Orb->SetRelativeScale3D(FVector(OrbRadius / 50.f * (1.f + 0.05f * FMath::Sin(Age * 7.f))));
	}
}

void AFinalTargetProp::OnBulletHit_Implementation(ARicochetBullet* Bullet, const FHitResult& Hit)
{
	if (!bIsUnlocked || bTriggered)
	{
		Flash(0.4f);
		return;
	}
	bTriggered = true;

	const FVector InDir = Bullet ? Bullet->GetVelocity().GetSafeNormal() : FVector::ForwardVector;
	if (ARicochetGameMode* GM = GetWorld()->GetAuthGameMode<ARicochetGameMode>())
	{
		GM->NotifyRoomCleared();
	}
	if (Bullet)
	{
		Bullet->StopBullet();
	}
	Shatter(InDir);
}

void AFinalTargetProp::Shatter(const FVector& FromDir)
{
	Orb->SetVisibility(false);
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Halo->SetIntensity(120.f);

	UMaterialInstanceDynamic* ShardMat = RicochetMat::Glow(this, RicochetMat::Coral, 20.f);
	FRandomStream R(GetUniqueID());
	for (int32 i = 0; i < 18; ++i)
	{
		UStaticMeshComponent* Shard = NewObject<UStaticMeshComponent>(this);
		Shard->SetStaticMesh(RicochetMat::CubeMesh());
		Shard->SetMaterial(0, ShardMat);
		Shard->SetCastShadow(false);
		Shard->SetWorldLocation(GetActorLocation() + R.VRand() * 12.f);
		Shard->SetWorldRotation(FRotator(R.FRandRange(0, 360), R.FRandRange(0, 360), 0));
		Shard->SetWorldScale3D(FVector(R.FRandRange(0.05f, 0.11f)));
		Shard->SetCollisionProfileName(TEXT("PhysicsActor"));
		Shard->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Shard->SetSimulatePhysics(true);
		Shard->RegisterComponent();
		Shard->AddImpulse((R.VRand() + FromDir * 0.6f).GetSafeNormal() * R.FRandRange(150.f, 450.f), NAME_None, true);
	}
}
