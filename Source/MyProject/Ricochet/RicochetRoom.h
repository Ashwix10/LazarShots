// Builds the whole level at runtime: room shell, lighting, fog, post-process, and a
// reverse-path generated, sweep-validated (=> guaranteed solvable) chain-reaction puzzle.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RicochetRoom.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USpotLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UMaterialInterface;
class ARicochetProp;

USTRUCT()
struct FRicochetLayout
{
	GENERATED_BODY()

	FVector Start = FVector::ZeroVector;   // player eye
	FVector Switch = FVector::ZeroVector;  // bounce 1
	FVector MirrorA = FVector::ZeroVector; // bounce 2
	FVector MirrorB = FVector::ZeroVector; // bounce 3
	FVector Target = FVector::ZeroVector;  // orb centre (inside the present)
	FVector SnapPoint = FVector::ZeroVector; // where segment MirrorA->MirrorB crosses the string
	FVector NSwitch = FVector::ForwardVector;
	FVector NMirrorA = FVector::ForwardVector;
	FVector NMirrorB = FVector::ForwardVector;
};

UCLASS()
class MYPROJECT_API ARicochetRoom : public AActor
{
	GENERATED_BODY()

public:

	ARicochetRoom();

	UPROPERTY(EditAnywhere, Category="Ricochet")
	int32 Seed = 1;

	/** Turns on the overhead spotlights + volumetric fog (called by the light switch). */
	void LightsOn();

	// Room dimensions (cm). Floor top at Z=0, centred on the origin.
	static constexpr float HalfSize = 900.f;
	static constexpr float Height = 700.f;
	static constexpr float EyeHeight = 64.f;
	static constexpr float CapsuleHalfHeight = 96.f;

	static FTransform GetPlayerSpawnTransform();
	static FVector GetPlayerEye();

protected:

	virtual void BeginPlay() override;

private:

	void BuildShell();
	void BuildAtmosphere();
	void BuildPuzzle();

	bool SampleLayout(FRandomStream& R, FRicochetLayout& Out) const;
	bool SpawnAndValidate(const FRicochetLayout& L, bool bValidate);
	void ClearPuzzle();
	void SpawnGuideWire(const FVector& A, const FVector& B, float Strength);

	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, UMaterialInterface* Mat,
		bool bSolid, const FRotator& Rot = FRotator::ZeroRotator, bool bPuzzle = false);

	static FRotator FaceRotation(const FVector& Normal);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USpotLightComponent>> RoomLights;

	UPROPERTY(Transient)
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> PuzzleActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PuzzleComponents;

	bool bLightsOn = false;
};
