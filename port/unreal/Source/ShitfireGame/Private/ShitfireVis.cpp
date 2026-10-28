#include "ShitfireVis.h"
#include "ShitfireGame.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameters.h"
#include "UObject/UObjectIterator.h"

namespace
{
	// shitfire.VisMode - flat-signature preview (ART-a). Not the P4.5f rig.
	TAutoConsoleVariable<int32> CVarVisMode(
		TEXT("shitfire.VisMode"), 0,
		TEXT("0 day (default), 1 render every material's VisNvg flat, 2 render VisThermal flat. ")
		TEXT("Checks a pack's optic-signature tagging by eye; the real NVG/thermal rig is P4.5f."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			if (!GEngine) { return; }
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if (Ctx.World() && (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE))
				{
					ShitfireVis::ApplyPreviewMode(Ctx.World());
				}
			}
		}));

	FAutoConsoleCommandWithWorld CmdVisAudit(
		TEXT("shitfire.VisAudit"),
		TEXT("Walk every primitive component and log an Error per material slot that lacks VisNvg/VisThermal."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			ShitfireVis::AuditWorld(World);
		}));

	bool HasScalar(const UMaterialInterface* Mat, const TCHAR* Name)
	{
		float V = 0.f;
		return Mat->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(Name)), V);
	}

	bool HasVector(const UMaterialInterface* Mat, const TCHAR* Name)
	{
		FLinearColor V;
		return Mat->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(Name)), V);
	}
}

namespace ShitfireVis
{
	FShitfireVis Derived(const FLinearColor& LinearColor)
	{
		// three.js Color.getRGB(_, SRGBColorSpace) hands visLum the sRGB-encoded
		// components (08-burst-optics.js:162), so encode first.
		const FColor S = LinearColor.ToFColorSRGB();
		const float R = S.R / 255.f, G = S.G / 255.f, B = S.B / 255.f;
		const float Lum = FMath::Clamp(0.2126f * R + 0.7152f * G + 0.0722f * B, 0.f, 1.f);
		return FShitfireVis(Lum, 0.05f + 0.13f * Lum);
	}

	void Apply(UMaterialInstanceDynamic* Mid, const FShitfireVis& Vis)
	{
		if (!Mid) { return; }
		Mid->SetScalarParameterValue(FName(ParamNvg), Vis.Nvg);
		Mid->SetVectorParameterValue(FName(ParamThermal), Vis.Thermal);
	}

	UMaterialInstanceDynamic* MakeTagged(UMaterialInterface* Base, UObject* Outer,
	                                     const FShitfireVis& Vis)
	{
		if (!Base) { return nullptr; }
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
		Apply(Mid, Vis);
		return Mid;
	}

	bool IsTagged(const UMaterialInterface* Mat, FString* OutWhy)
	{
		if (!Mat)
		{
			if (OutWhy) { *OutWhy = TEXT("no material (engine default)"); }
			return false;
		}
		const bool bNvg = HasScalar(Mat, ParamNvg);
		const bool bTh = HasVector(Mat, ParamThermal);
		if (bNvg && bTh) { return true; }
		if (OutWhy)
		{
			*OutWhy = FString::Printf(TEXT("%s lacks %s%s%s"), *Mat->GetPathName(),
				bNvg ? TEXT("") : ParamNvg,
				(!bNvg && !bTh) ? TEXT(" and ") : TEXT(""),
				bTh ? TEXT("") : ParamThermal);
		}
		return false;
	}

	int32 AuditWorld(UWorld* World, TArray<FString>* OutOffenders)
	{
		if (!World) { return 0; }
		int32 Components = 0, Slots = 0, Offenders = 0;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			UPrimitiveComponent* C = *It;
			if (!C || C->GetWorld() != World || !C->IsRegistered()) { continue; }
			// Editor-only visuals never reach the optics: the camera
			// component's proxy mesh (UCameraComponent::OnRegister sets it as
			// a visualization component, hidden in game) carries an engine
			// editor material and was the audit's one false catch (2026-10-28
			// bench). Sky, fog and lights carry no mesh slots and drop out
			// on N == 0 below.
			if (C->IsVisualizationComponent() || C->IsEditorOnly() || C->bHiddenInGame)
			{
				continue;
			}
			const int32 N = C->GetNumMaterials();
			if (N == 0) { continue; }
			Components++;
			for (int32 i = 0; i < N; i++)
			{
				Slots++;
				FString Why;
				if (!IsTagged(C->GetMaterial(i), &Why))
				{
					Offenders++;
					const FString Line = FString::Printf(TEXT("%s/%s[%d]: %s"),
						C->GetOwner() ? *C->GetOwner()->GetName() : TEXT("<no owner>"),
						*C->GetName(), i, *Why);
					UE_LOG(LogShitfire, Error, TEXT("vis audit: untagged %s"), *Line);
					if (OutOffenders) { OutOffenders->Add(Line); }
				}
			}
		}
		UE_LOG(LogShitfire, Log, TEXT("vis audit: %d components, %d slots, %d untagged"),
			Components, Slots, Offenders);
		return Offenders;
	}

	void ApplyPreviewMode(UWorld* World)
	{
		if (!World) { return; }
		UMaterialParameterCollection* MPC =
			LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
		if (!MPC)
		{
			UE_LOG(LogShitfire, Warning, TEXT("MPC_Vision missing - run Tools/gen_assets.py"));
			return;
		}
		const int32 Mode = FMath::Clamp(CVarVisMode.GetValueOnGameThread(), 0, 2);
		UKismetMaterialLibrary::SetScalarParameterValue(World, MPC, FName(ModeParam),
			static_cast<float>(Mode));
		if (Mode != 0)
		{
			UE_LOG(LogShitfire, Log, TEXT("vis preview mode %d (%s)"), Mode,
				Mode == 1 ? TEXT("NVG luminance") : TEXT("thermal signature"));
		}
	}
}
