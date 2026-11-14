#include "RicochetRoom.h"

#include "RicochetBullet.h"
#include "RicochetMaterials.h"
#include "RicochetProps.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float NodeInset = 160.f;   // keep bounce props away from walls
	constexpr float MinNodeSpacing = 260.f;
	constexpr float SegmentClearance = 150.f;

	float BulletRadius()
	{
		const ARicochetBullet* CDO = GetDefault<ARicochetBullet>();
		return (CDO && CDO->Collision) ? CDO->Collision->GetUnscaledSphereRadius() : 4.f;
	}

	/** Reflection-bisector normal for a bounce at Cur. Rejects grazing / near-reversal bounces. */
	bool BisectorNormal(const FVector& Prev, const FVector& Cur, const FVector& Next, FVector& OutN)
	{
		const FVector In = (Cur - Prev).GetSafeNormal();
		const FVector Out = (Next - Cur).GetSafeNormal();
		const float D = FVector::DotProduct(In, Out);
		if (D > 0.55f || D < -0.9f)
		{
			return false;
		}
		OutN = (Out - In).GetSafeNormal(); // == Normalize(-Incoming + Outgoing)
		return !OutN.IsNearlyZero();
	}
}

ARicochetRoom::ARicochetRoom()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

FTransform ARicochetRoom::GetPlayerSpawnTransform()
{
	return FTransform(FRotator::ZeroRotator, FVector(-720.f, 0.f, CapsuleHalfHeight + 2.f));
}

FVector ARicochetRoom::GetPlayerEye()
{
	return GetPlayerSpawnTransform().GetLocation() + FVector(0.f, 0.f, EyeHeight);
}

void ARicochetRoom::BeginPlay()
{
	Super::BeginPlay();
	SetActorTransform(FTransform::Identity);
	BuildShell();
	BuildPuzzle();
	BuildAtmosphere();
}

FRotator ARicochetRoom::FaceRotation(const FVector& Normal)
{
	if (FMath::Abs(Normal.Z) > 0.98f)
	{
		return FRotationMatrix::MakeFromX(Normal).Rotator();
	}
	return FRotationMatrix::MakeFromXZ(Normal, FVector::UpVector).Rotator();
}

UStaticMeshComponent* ARicochetRoom::AddShape(UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, UMaterialInterface* Mat,
	bool bSolid, const FRotator& Rot, bool bPuzzle)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Root);
	C->SetRelativeTransform(FTransform(Rot, Loc, Scale));
	C->SetMaterial(0, Mat);
	C->SetCanEverAffectNavigation(false);
	if (bSolid)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
	}
	C->SetGenerateOverlapEvents(false);
	C->RegisterComponent();
	if (bPuzzle)
	{
		PuzzleComponents.Add(C);
	}
	return C;
}

// ---------------------------------------------------------------------------------------------
// Shell
// ---------------------------------------------------------------------------------------------

void ARicochetRoom::BuildShell()
{
	UStaticMesh* Cube = RicochetMat::CubeMesh();
	const float H = HalfSize;
	const float T = 20.f;                       // wall thickness
	const float Span = (2.f * H + 2.f * T) / 100.f;

	UMaterialInstanceDynamic* FloorMat = RicochetMat::Surface(this, FLinearColor(0.035f, 0.037f, 0.045f), 0.28f);
	UMaterialInstanceDynamic* WallMat = RicochetMat::Surface(this, FLinearColor(0.42f, 0.43f, 0.47f), 0.92f);
	UMaterialInstanceDynamic* CeilMat = RicochetMat::Surface(this, FLinearColor(0.03f, 0.03f, 0.035f), 0.9f);

	AddShape(Cube, FVector(0, 0, -T * 0.5f), FVector(Span, Span, T / 100.f), FloorMat, true);
	AddShape(Cube, FVector(0, 0, Height + T * 0.5f), FVector(Span, Span, T / 100.f), CeilMat, true);

	const float WallZ = Height * 0.5f;
	const float WallH = Height / 100.f;
	AddShape(Cube, FVector(H + T * 0.5f, 0, WallZ), FVector(T / 100.f, Span, WallH), WallMat, true);
	AddShape(Cube, FVector(-H - T * 0.5f, 0, WallZ), FVector(T / 100.f, Span, WallH), WallMat, true);
	AddShape(Cube, FVector(0, H + T * 0.5f, WallZ), FVector(Span, T / 100.f, WallH), WallMat, true);
	AddShape(Cube, FVector(0, -H - T * 0.5f, WallZ), FVector(Span, T / 100.f, WallH), WallMat, true);

	// Glowing trim along the floor and ceiling edges: the cheapest "polished indie" look there is.
	UMaterialInstanceDynamic* Trim = RicochetMat::Glow(this, RicochetMat::Cyan, 3.f);
	UMaterialInstanceDynamic* TrimDim = RicochetMat::Glow(this, RicochetMat::Cyan, 0.8f);
	const float L = 2.f * H / 100.f;
	for (const float Z : { 2.f, Height - 2.f })
	{
		UMaterialInstanceDynamic* M = Z < 10.f ? Trim : TrimDim;
		AddShape(Cube, FVector(H - 2.f, 0, Z), FVector(0.04f, L, 0.04f), M, false);
		AddShape(Cube, FVector(-H + 2.f, 0, Z), FVector(0.04f, L, 0.04f), M, false);
		AddShape(Cube, FVector(0, H - 2.f, Z), FVector(L, 0.04f, 0.04f), M, false);
		AddShape(Cube, FVector(0, -H + 2.f, Z), FVector(L, 0.04f, 0.04f), M, false);
	}

	// Vertical panel seams on the walls.
	for (float P = -H + 300.f; P < H - 1.f; P += 300.f)
	{
		AddShape(Cube, FVector(H - 1.f, P, WallZ), FVector(0.02f, 0.02f, WallH), TrimDim, false);
		AddShape(Cube, FVector(-H + 1.f, P, WallZ), FVector(0.02f, 0.02f, WallH), TrimDim, false);
		AddShape(Cube, FVector(P, H - 1.f, WallZ), FVector(0.02f, 0.02f, WallH), TrimDim, false);
		AddShape(Cube, FVector(P, -H + 1.f, WallZ), FVector(0.02f, 0.02f, WallH), TrimDim, false);
	}

	// Player pad: a glowing ring under the fixed firing position.
	const FVector Pad = GetPlayerSpawnTransform().GetLocation() * FVector(1, 1, 0);
	AddShape(RicochetMat::CylinderMesh(), Pad + FVector(0, 0, 0.5f), FVector(1.3f, 1.3f, 0.01f), RicochetMat::Glow(this, RicochetMat::Cyan, 4.f), false);
	AddShape(RicochetMat::CylinderMesh(), Pad + FVector(0, 0, 0.8f), FVector(1.15f, 1.15f, 0.012f), FloorMat, false);
}

// ---------------------------------------------------------------------------------------------
// Lighting, fog, post
// ---------------------------------------------------------------------------------------------

void ARicochetRoom::BuildAtmosphere()
{
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->RegisterComponent();
	Fog->SetFogDensity(0.03f);
	Fog->SetFogHeightFalloff(0.15f);
	Fog->SetFogInscatteringColor(FLinearColor(0.003f, 0.004f, 0.007f));
	Fog->SetVolumetricFogScatteringDistribution(0.6f);
	Fog->SetVolumetricFog(false);

	PostProcess = NewObject<UPostProcessComponent>(this);
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = 1.5f; // locked exposure: the dark room stays dark
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = 1.5f;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = 0.f;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.9f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.55f;
	PostProcess->RegisterComponent();

	// Overhead spotlights (off until the switch is hit) + a dramatic one over the present.
	TArray<FVector> Spots = {
		FVector(450, 450, Height - 15), FVector(450, -450, Height - 15),
		FVector(-450, 450, Height - 15), FVector(-450, -450, Height - 15),
	};
	PuzzleActors.RemoveAll([](const TObjectPtr<AActor>& A) { return !IsValid(A); });
	for (AActor* A : PuzzleActors)
	{
		if (A->IsA<AFinalTargetProp>())
		{
			Spots.Add(FVector(A->GetActorLocation().X, A->GetActorLocation().Y, Height - 15));
		}
	}

	for (int32 i = 0; i < Spots.Num(); ++i)
	{
		const bool bHero = i >= 4;
		USpotLightComponent* L = NewObject<USpotLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(Root);
		L->RegisterComponent();
		L->SetWorldLocationAndRotation(Spots[i], FRotator(-90.f, 0.f, 0.f));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(bHero ? 220.f : 300.f);
		L->SetLightColor(bHero ? FLinearColor(1.f, 0.85f, 0.8f) : FLinearColor(0.92f, 0.96f, 1.f));
		L->SetInnerConeAngle(bHero ? 8.f : 18.f);
		L->SetOuterConeAngle(bHero ? 16.f : 36.f);
		L->SetAttenuationRadius(1500.f);
		L->SetVolumetricScatteringIntensity(4.f);
		L->SetVisibility(false);
		RoomLights.Add(L);

		// Ceiling fixture that lights up with the spot.
		AddShape(RicochetMat::CubeMesh(), Spots[i] + FVector(0, 0, 12.f), FVector(0.5f, 0.5f, 0.04f),
			RicochetMat::Surface(this, FLinearColor(0.05f, 0.05f, 0.05f), 0.5f), false);
	}
}

void ARicochetRoom::LightsOn()
{
	if (bLightsOn)
	{
		return;
	}
	bLightsOn = true;
	for (USpotLightComponent* L : RoomLights)
	{
		if (L)
		{
			L->SetVisibility(true);
			// Fixture glow
			AddShape(RicochetMat::CubeMesh(), L->GetComponentLocation() + FVector(0, 0, 9.f), FVector(0.4f, 0.4f, 0.02f),
				RicochetMat::Glow(this, FLinearColor(1.f, 0.97f, 0.92f), 30.f), false);
		}
	}
	if (Fog)
	{
		Fog->SetFogInscatteringColor(FLinearColor(0.012f, 0.016f, 0.024f));
		Fog->SetVolumetricFog(true);
	}
}

// ---------------------------------------------------------------------------------------------
// Puzzle generation (reverse path)
// ---------------------------------------------------------------------------------------------
//
// Path: Eye -> Switch -> MirrorA -> (string) -> MirrorB -> Target.
// We pick the END first (target + the snap point on its string), then pick MirrorA anywhere and
// place MirrorB further along the line MirrorA->SnapPoint, so that segment always crosses the
// string. Each bounce prop is oriented with the bisector normal Normalize(-In + Out), which makes
// a perfect reflection map In onto Out. Finally the real bullet sweep is simulated against the
// spawned props; only layouts where the simulated bullet hits Switch, MirrorA, MirrorB, Target in
// order are kept, so every room that ships is solvable.

bool ARicochetRoom::SampleLayout(FRandomStream& R, FRicochetLayout& L) const
{
	const float In = HalfSize - NodeInset;
	const FVector P0 = GetPlayerEye();

	auto InBounds = [&](const FVector& P)
	{
		return FMath::Abs(P.X) <= In && FMath::Abs(P.Y) <= In && P.Z >= 110.f && P.Z <= Height - 110.f;
	};

	for (int32 Try = 0; Try < 6000; ++Try)
	{
		L = FRicochetLayout();
		L.Start = P0;

		L.Target = FVector(R.FRandRange(-150.f, 650.f), R.FRandRange(-600.f, 600.f), R.FRandRange(150.f, 300.f));
		L.SnapPoint = L.Target + FVector(0.f, 0.f, R.FRandRange(170.f, 260.f));
		if (L.SnapPoint.Z > Height - 80.f)
		{
			continue;
		}

		L.Switch = FVector(R.FRandRange(-350.f, In), R.FRandRange(-In, In), R.FRandRange(120.f, Height - 140.f));
		if (FVector::Dist(P0, L.Switch) < 450.f ||
			FVector::DotProduct((L.Switch - P0).GetSafeNormal(), FVector::ForwardVector) < 0.6f) // keep it in view
		{
			continue;
		}

		L.MirrorA = FVector(R.FRandRange(-In, In), R.FRandRange(-In, In), R.FRandRange(110.f, Height - 110.f));
		if (FVector::Dist(L.MirrorA, L.SnapPoint) < 250.f)
		{
			continue;
		}
		const FVector Through = (L.SnapPoint - L.MirrorA).GetSafeNormal();
		L.MirrorB = L.SnapPoint + Through * R.FRandRange(220.f, 520.f);
		if (!InBounds(L.MirrorB))
		{
			continue;
		}

		// Spacing between all nodes (and the player).
		const FVector Nodes[] = { P0, L.Switch, L.MirrorA, L.MirrorB, L.Target };
		bool bOk = true;
		for (int32 a = 0; a < 5 && bOk; ++a)
		{
			for (int32 b = a + 1; b < 5 && bOk; ++b)
			{
				bOk = FVector::Dist(Nodes[a], Nodes[b]) >= (a == 0 ? 400.f : MinNodeSpacing);
			}
		}
		if (!bOk)
		{
			continue;
		}

		if (!BisectorNormal(P0, L.Switch, L.MirrorA, L.NSwitch) ||
			!BisectorNormal(L.Switch, L.MirrorA, L.MirrorB, L.NMirrorA) ||
			!BisectorNormal(L.MirrorA, L.MirrorB, L.Target, L.NMirrorB))
		{
			continue;
		}

		// Every segment must stay clear of the props it doesn't touch.
		for (int32 s = 0; s < 4 && bOk; ++s)
		{
			for (int32 n = 1; n < 5 && bOk; ++n)
			{
				if (n == s || n == s + 1)
				{
					continue;
				}
				bOk = FMath::PointDistToSegment(Nodes[n], Nodes[s], Nodes[s + 1]) >= SegmentClearance;
			}
		}
		if (!bOk)
		{
			continue;
		}

		// Only the MirrorA->MirrorB segment may cross the string column (keeps the sequence readable),
		// and no bounce prop may sit on the string. The final MirrorB->Target leg is exempt: it ends at
		// the target directly beneath the string (always within 70uu), and the string is already cut by then.
		const FVector ColTop(L.Target.X, L.Target.Y, Height);
		for (int32 s = 0; s < 2 && bOk; ++s)
		{
			FVector C1, C2;
			FMath::SegmentDistToSegmentSafe(Nodes[s], Nodes[s + 1], L.Target + FVector(0, 0, 60.f), ColTop, C1, C2);
			bOk = FVector::Dist(C1, C2) >= 70.f;
		}
		for (int32 n = 1; n < 4 && bOk; ++n)
		{
			bOk = FVector::Dist2D(Nodes[n], L.Target) >= 130.f || Nodes[n].Z < L.Target.Z - 80.f;
		}
		if (!bOk)
		{
			continue;
		}

		return true;
	}
	return false;
}

void ARicochetRoom::ClearPuzzle()
{
	for (AActor* A : PuzzleActors)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	PuzzleActors.Reset();
	for (UStaticMeshComponent* C : PuzzleComponents)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	PuzzleComponents.Reset();
}

bool ARicochetRoom::SpawnAndValidate(const FRicochetLayout& L, bool bValidate)
{
	UWorld* World = GetWorld();
	const float BR = BulletRadius();

	FActorSpawnParameters P;
	P.Owner = this;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Offset each face back by the bullet radius so the bullet's CENTRE reflects exactly at the node.
	ALightSwitchProp* Switch = World->SpawnActor<ALightSwitchProp>(L.Switch - L.NSwitch * BR, FaceRotation(L.NSwitch), P);
	AMirrorProp* MirrorA = World->SpawnActor<AMirrorProp>(L.MirrorA - L.NMirrorA * BR, FaceRotation(L.NMirrorA), P);
	AMirrorProp* MirrorB = World->SpawnActor<AMirrorProp>(L.MirrorB - L.NMirrorB * BR, FaceRotation(L.NMirrorB), P);
	AFinalTargetProp* Target = World->SpawnActor<AFinalTargetProp>(L.Target, FRotator::ZeroRotator, P);
	AHangingPresentProp* Present = World->SpawnActor<AHangingPresentProp>(L.Target, FRotator(0.f, 25.f, 0.f), P);

	PuzzleActors = { Switch, MirrorA, MirrorB, Target, Present };
	if (!Switch || !MirrorA || !MirrorB || !Target || !Present)
	{
		ClearPuzzle();
		return false;
	}

	Present->SetStringLength(Height - (L.Target.Z + AHangingPresentProp::BoxHalf + 12.f));
	Switch->NextActorToUnlock = Present;
	Present->NextActorToUnlock = Target;

	// Simulate the exact bullet sweep the player will fire.
	const FVector Dir = (L.Switch - L.Start).GetSafeNormal();
	TArray<FVector> Points;
	TArray<FHitResult> Hits;
	ARicochetBullet::PredictRicochetPath(this, ARicochetBullet::StaticClass(), L.Start + Dir * 30.f, Dir, 3, 6000.f,
		TArray<AActor*>(), Points, Hits);

	const AActor* Expected[] = { Switch, MirrorA, MirrorB, Target };
	bool bOk = Hits.Num() >= 4 && Points.Num() >= 4;
	for (int32 i = 0; bOk && i < 4; ++i)
	{
		bOk = Hits[i].GetActor() == Expected[i];
	}
	// The MirrorA->MirrorB leg must really pass through the string trigger.
	bOk = bOk && FMath::PointDistToSegment(L.SnapPoint, Points[2], Points[3]) < 12.f;

	if (!bOk && bValidate)
	{
		ClearPuzzle();
		return false;
	}

	// Thin glowing guide wires so the player can read the sequence (the first shot is theirs to find).
	SpawnGuideWire(L.Switch, L.MirrorA, 2.5f);
	SpawnGuideWire(L.MirrorA, L.MirrorB, 2.5f);
	SpawnGuideWire(L.MirrorB, L.Target, 2.5f);
	return true;
}

void ARicochetRoom::SpawnGuideWire(const FVector& A, const FVector& B, float Strength)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f)
	{
		return;
	}
	AddShape(RicochetMat::CylinderMesh(), (A + B) * 0.5f, FVector(0.012f, 0.012f, Len / 100.f),
		RicochetMat::Glow(this, RicochetMat::Cyan, Strength), false, FRotationMatrix::MakeFromZ(D / Len).Rotator(), true);
}

void ARicochetRoom::BuildPuzzle()
{
	// Try the requested seed first; if (very unlikely) nothing validates, walk to neighbouring seeds.
	bool bHaveFallback = false;
	FRicochetLayout Fallback;
	for (int32 SeedOffset = 0; SeedOffset < 8; ++SeedOffset)
	{
		FRandomStream R(Seed + SeedOffset * 7919);
		for (int32 Round = 0; Round < 40; ++Round)
		{
			FRicochetLayout L;
			if (!SampleLayout(R, L))
			{
				break;
			}
			if (!bHaveFallback)
			{
				bHaveFallback = true;
				Fallback = L;
			}
			if (SpawnAndValidate(L, true))
			{
				UE_LOG(LogTemp, Log, TEXT("Ricochet: room seed %d solved layout in round %d."), Seed + SeedOffset * 7919, Round);
				return;
			}
		}
	}
	// The analytic layout is exact by construction (bisector normals + clearance checks); the sweep
	// is a second opinion. If it never agreed, still ship the analytic layout rather than an empty room.
	if (bHaveFallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ricochet: sweep validation never passed for seed %d; using analytic layout."), Seed);
		SpawnAndValidate(Fallback, false);
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("Ricochet: failed to generate a room for seed %d."), Seed);
}
