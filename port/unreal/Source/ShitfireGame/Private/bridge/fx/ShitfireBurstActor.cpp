#include "bridge/fx/ShitfireBurstActor.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* ConePath = TEXT("/Engine/BasicShapes/Cone.Cone");

	// Engine basic shapes are 100 cm across (radius 50 cm), pivot centered.
	constexpr double ShapeRadiusCm = 50.0;
	constexpr double ShapeHeightCm = 100.0;

	constexpr int32 NumSmokeSegments = 3;
}

AShitfireBurstActor::AShitfireBurstActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AShitfireBurstActor::MakePart(const TCHAR* Name,
	const TCHAR* MeshPath, const TCHAR* MaterialPath,
	TObjectPtr<UMaterialInstanceDynamic>& OutMid)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogShitfire, Error, TEXT("burst FX: engine mesh missing: %s"), MeshPath);
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);   // translucent columns; shadows would double cost
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
	{
		OutMid = UMaterialInstanceDynamic::Create(M, this);
		C->SetMaterial(0, OutMid);
	}
	else
	{
		UE_LOG(LogShitfire, Warning,
			TEXT("burst FX: %s missing - run Tools/gen_fx_assets.py"), MaterialPath);
		OutMid = nullptr;
	}
	C->RegisterComponent();
	C->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	return C;
}

void AShitfireBurstActor::InitBurst(const FVector& DownwindUE, double WindSpeedMps,
                                    double StartAgeS)
{
	Downwind = DownwindUE.GetSafeNormal2D();
	WindSpeed = WindSpeedMps;
	AgeS = StartAgeS;
}

void AShitfireBurstActor::BeginPlay()
{
	Super::BeginPlay();

	Flash = MakePart(TEXT("Flash"), SpherePath,
		TEXT("/Game/FX/M_FxFlash.M_FxFlash"), FlashMid);
	Dirt = MakePart(TEXT("Dirt"), ConePath,
		TEXT("/Game/FX/M_FxDirt.M_FxDirt"), DirtMid);
	for (int32 i = 0; i < NumSmokeSegments; i++)
	{
		TObjectPtr<UMaterialInstanceDynamic> Mid = nullptr;
		UStaticMeshComponent* Seg = MakePart(
			*FString::Printf(TEXT("Smoke%d"), i), CylinderPath,
			TEXT("/Game/FX/M_FxSmoke.M_FxSmoke"), Mid);
		SmokeSegments.Add(Seg);
		SmokeMids.Add(Mid);
	}

	SetLifeSpan(static_cast<float>(SmokeLifeS + 1.0));
}

void AShitfireBurstActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeS += DeltaSeconds;
	const double M = ShitfireCoords::MetersToUE;

	// ---- flash: full size instantly, gone by FlashLifeS ------------------
	if (Flash)
	{
		if (AgeS >= FlashLifeS)
		{
			Flash->SetVisibility(false);
		}
		else
		{
			const double R = FlashRadiusM * (1.0 - 0.5 * AgeS / FlashLifeS);
			Flash->SetRelativeScale3D(FVector(R * M / ShapeRadiusCm));
			Flash->SetRelativeLocation(FVector(0, 0, R * M * 0.5));
		}
	}

	// ---- dirt column: grows over DirtGrowS, fades out by DirtLifeS -------
	if (Dirt)
	{
		if (AgeS >= DirtLifeS)
		{
			Dirt->SetVisibility(false);
		}
		else
		{
			const double Grow = FMath::Clamp(AgeS / DirtGrowS, 0.05, 1.0);
			const double H = DirtHeightM * Grow;
			// engine cone: 100 cm tall, base radius 50 cm, pivot centered
			Dirt->SetRelativeScale3D(FVector(
				DirtRadiusM * Grow * M / ShapeRadiusCm,
				DirtRadiusM * Grow * M / ShapeRadiusCm,
				H * M / ShapeHeightCm));
			Dirt->SetRelativeLocation(FVector(0, 0, H * M * 0.5));
			if (DirtMid)
			{
				const double Fade = FMath::Clamp((DirtLifeS - AgeS) / 2.5, 0.0, 1.0);
				DirtMid->SetScalarParameterValue(TEXT("Opacity"),
					static_cast<float>(0.65 * Fade));
			}
		}
	}

	// ---- smoke column: 3 stacked segments, leaning downwind --------------
	// Height ramps over SmokeGrowS; the lean at the top ramps with age and
	// wind speed, capped at SmokeMaxLeanM - subtle drift, not a gale.
	const double Grow = FMath::Clamp(AgeS / SmokeGrowS, 0.06, 1.0);
	const double TotalH = SmokeHeightM * Grow;
	// Segments overlap (x1.3 height) so the lean reads as one bending plume,
	// not three stacked blocks (2026-09-03 FX-probe screenshots).
	const double SegH = (TotalH / NumSmokeSegments) * 1.3;
	const double Lean = FMath::Min(SmokeMaxLeanM,
		0.08 * WindSpeed * AgeS) * Grow;   // meters at the column top
	const double Fade = (AgeS < SmokeFadeStartS)
		? 1.0
		: FMath::Clamp((SmokeLifeS - AgeS) / (SmokeLifeS - SmokeFadeStartS), 0.0, 1.0);

	for (int32 i = 0; i < SmokeSegments.Num(); i++)
	{
		UStaticMeshComponent* Seg = SmokeSegments[i];
		if (!Seg) { continue; }
		const double Frac = (i + 0.5) / NumSmokeSegments;        // segment center 0..1
		const double TopFrac = (i + 1.0) / NumSmokeSegments;
		const double R = SmokeRadiusM * (0.8 + 0.9 * Frac);      // widens upward
		const FVector LeanOff = Downwind * (Lean * Frac * M);    // linear = smooth bend
		Seg->SetRelativeScale3D(FVector(
			R * M / ShapeRadiusCm, R * M / ShapeRadiusCm, SegH * M / ShapeHeightCm));
		Seg->SetRelativeLocation(
			FVector(0, 0, (Frac * TotalH) * M) + LeanOff);
		if (SmokeMids[i])
		{
			// near-opaque body, slightly thinner at the top; whole column
			// fades out over the last minute of life
			SmokeMids[i]->SetScalarParameterValue(TEXT("Opacity"),
				static_cast<float>(Fade * (1.0 - 0.15 * TopFrac)));
		}
	}
}
