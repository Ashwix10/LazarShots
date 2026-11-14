#include "RicochetMaterials.h"

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/SavePackage.h"
#endif

namespace
{
	const TCHAR* SurfacePkg = TEXT("/Game/Ricochet/Materials/M_RicSurface");
	const TCHAR* GlowPkg = TEXT("/Game/Ricochet/Materials/M_RicGlow");

	UMaterialInterface* GSurface = nullptr;
	UMaterialInterface* GGlow = nullptr;

	template <typename T>
	T* Root(T* Obj)
	{
		if (Obj)
		{
			Obj->AddToRoot(); // lives for the whole session; avoids static smart-pointer teardown order issues
		}
		return Obj;
	}

	UMaterialInterface* LoadIfExists(const TCHAR* PkgName)
	{
		if (!FPackageName::DoesPackageExist(PkgName))
		{
			return nullptr;
		}
		const FString ObjPath = FString(PkgName) + TEXT(".") + FPackageName::GetShortName(PkgName);
		return LoadObject<UMaterialInterface>(nullptr, *ObjPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

#if WITH_EDITOR
	template <typename T>
	T* AddExpr(UMaterial* M, int32 X, int32 Y)
	{
		T* E = NewObject<T>(M);
		E->MaterialExpressionEditorX = X;
		E->MaterialExpressionEditorY = Y;
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	void SaveMaterial(UMaterial* M, const TCHAR* PkgName);

	UMaterial* BuildMaterial(const TCHAR* PkgName, bool bGlow)
	{
		const FString Name = FPackageName::GetShortName(PkgName);
		UPackage* Pkg = CreatePackage(PkgName);
		Pkg->FullyLoad();

		UMaterial* M = NewObject<UMaterial>(Pkg, *Name, RF_Public | RF_Standalone | RF_Transactional);
		UMaterialEditorOnlyData* ED = M->GetEditorOnlyData();

		UMaterialExpressionVectorParameter* Color = AddExpr<UMaterialExpressionVectorParameter>(M, -600, 0);
		Color->ParameterName = TEXT("Color");
		Color->DefaultValue = bGlow ? RicochetMat::Cyan : FLinearColor(0.5f, 0.5f, 0.5f);

		if (bGlow)
		{
			M->SetShadingModel(MSM_Unlit);

			UMaterialExpressionScalarParameter* Strength = AddExpr<UMaterialExpressionScalarParameter>(M, -600, 250);
			Strength->ParameterName = TEXT("Strength");
			Strength->DefaultValue = 10.f;

			UMaterialExpressionMultiply* Mul = AddExpr<UMaterialExpressionMultiply>(M, -300, 100);
			Mul->A.Connect(0, Color);
			Mul->B.Connect(0, Strength);

			ED->EmissiveColor.Connect(0, Mul);
		}
		else
		{
			UMaterialExpressionScalarParameter* Metallic = AddExpr<UMaterialExpressionScalarParameter>(M, -600, 250);
			Metallic->ParameterName = TEXT("Metallic");
			Metallic->DefaultValue = 0.f;

			UMaterialExpressionScalarParameter* Roughness = AddExpr<UMaterialExpressionScalarParameter>(M, -600, 400);
			Roughness->ParameterName = TEXT("Roughness");
			Roughness->DefaultValue = 0.85f;

			ED->BaseColor.Connect(0, Color);
			ED->Metallic.Connect(0, Metallic);
			ED->Roughness.Connect(0, Roughness);
		}

		// The character's aim-preview uses instanced meshes; without this flag the glow falls back to the default material.
		M->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);

		// PostEditChange runs the Substrate legacy conversion and kicks off shader compilation.
		M->PreEditChange(nullptr);
		M->PostEditChange();

		SaveMaterial(M, PkgName);
		return M;
	}

	void SaveMaterial(UMaterial* M, const TCHAR* PkgName)
	{
		UPackage* Pkg = M->GetPackage();
		Pkg->MarkPackageDirty();
		const FString File = FPackageName::LongPackageNameToFilename(PkgName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Pkg, M, *File, Args))
		{
			UE_LOG(LogTemp, Warning, TEXT("Ricochet: could not save %s (it still works in-memory this session)."), PkgName);
		}
	}

	// Upgrades materials generated before the instanced-mesh usage flag was added.
	void EnsureInstancedUsage(UMaterialInterface* MI, const TCHAR* PkgName)
	{
		UMaterial* M = Cast<UMaterial>(MI);
		if (!M || M->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes))
		{
			return;
		}
		M->PreEditChange(nullptr);
		M->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);
		M->PostEditChange();
		SaveMaterial(M, PkgName);
	}

	// Build the materials as soon as the editor is up so shaders are compiled before the first Play.
	struct FAutoEnsure
	{
		FAutoEnsure()
		{
			FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
			{
				if (GIsEditor && !IsRunningCommandlet())
				{
					RicochetMat::EnsureAssets();
				}
			});
		}
	} GAutoEnsure;
#endif

	UMaterialInterface* GetBase(UMaterialInterface*& Cache, const TCHAR* PkgName, bool bGlow)
	{
		if (Cache && IsValid(Cache))
		{
			return Cache;
		}
		UMaterialInterface* M = LoadIfExists(PkgName);
#if WITH_EDITOR
		if (!M && GIsEditor)
		{
			M = BuildMaterial(PkgName, bGlow);
		}
		else if (M && GIsEditor)
		{
			EnsureInstancedUsage(M, PkgName);
		}
#endif
		if (!M)
		{
			// Last-resort fallback (should only happen in a packaged build that didn't cook /Game/Ricochet).
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		Cache = Root(M);
		return M;
	}
}

void RicochetMat::EnsureAssets()
{
	GetSurfaceBase();
	GetGlowBase();
}

UMaterialInterface* RicochetMat::GetSurfaceBase() { return GetBase(GSurface, SurfacePkg, false); }
UMaterialInterface* RicochetMat::GetGlowBase() { return GetBase(GGlow, GlowPkg, true); }

UMaterialInstanceDynamic* RicochetMat::Surface(UObject* Outer, const FLinearColor& Color, float Roughness, float Metallic)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GetSurfaceBase(), Outer);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	return MID;
}

UMaterialInstanceDynamic* RicochetMat::Mirror(UObject* Outer)
{
	return Surface(Outer, FLinearColor(0.95f, 0.97f, 1.f), 0.03f, 1.f);
}

UMaterialInstanceDynamic* RicochetMat::Glow(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GetGlowBase(), Outer);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Strength"), Strength);
	return MID;
}

UStaticMesh* RicochetMat::CubeMesh()
{
	static UStaticMesh* P = nullptr;
	if (!P) { P = Root(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))); }
	return P;
}

UStaticMesh* RicochetMat::SphereMesh()
{
	static UStaticMesh* P = nullptr;
	if (!P) { P = Root(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))); }
	return P;
}

UStaticMesh* RicochetMat::CylinderMesh()
{
	static UStaticMesh* P = nullptr;
	if (!P) { P = Root(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"))); }
	return P;
}
