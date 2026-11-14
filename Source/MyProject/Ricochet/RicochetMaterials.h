// Procedural materials for the Ricochet jam game. No hand-made assets required:
// the two base materials are built in code (and saved to /Game/Ricochet/Materials in the editor
// so packaged builds can cook them). Everything else is a dynamic instance of those two.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;

namespace RicochetMat
{
	/** Lit surface: params Color (vector), Roughness, Metallic. */
	MYPROJECT_API UMaterialInterface* GetSurfaceBase();

	/** Unlit emissive: params Color (vector), Strength. */
	MYPROJECT_API UMaterialInterface* GetGlowBase();

	MYPROJECT_API UMaterialInstanceDynamic* Surface(UObject* Outer, const FLinearColor& Color, float Roughness, float Metallic = 0.f);
	MYPROJECT_API UMaterialInstanceDynamic* Mirror(UObject* Outer);
	MYPROJECT_API UMaterialInstanceDynamic* Glow(UObject* Outer, const FLinearColor& Color, float Strength);

	/** Builds + saves the base materials if they don't exist yet (editor only, no-op otherwise). */
	MYPROJECT_API void EnsureAssets();

	// Palette
	inline const FLinearColor Cyan(0.f, 0.85f, 1.f);
	inline const FLinearColor Coral(1.f, 0.28f, 0.22f);

	// Engine basic shapes
	MYPROJECT_API UStaticMesh* CubeMesh();
	MYPROJECT_API UStaticMesh* SphereMesh();
	MYPROJECT_API UStaticMesh* CylinderMesh();
}
