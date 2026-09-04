#include "bridge/fx/ShitfireFlareActor.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AShitfireFlareActor::AShitfireFlareActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AShitfireFlareActor::InitFlare(double BurnSeconds)
{
	BurnS = FMath::Max(1.0, BurnSeconds);
}

void AShitfireFlareActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnUE = GetActorLocation();
	const double M = ShitfireCoords::MetersToUE;

	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr,
			TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		Glow = NewObject<UStaticMeshComponent>(this, TEXT("Glow"));
		Glow->SetStaticMesh(Sphere);
		Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glow->SetCastShadow(false);
		Glow->SetRelativeScale3D(FVector(GlowRadiusM * M / 50.0));   // 50 cm base radius
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/FX/M_FxFlare.M_FxFlare")))
		{
			GlowMid = UMaterialInstanceDynamic::Create(Mat, this);
			Glow->SetMaterial(0, GlowMid);
		}
		else
		{
			UE_LOG(LogShitfire, Warning,
				TEXT("flare FX: M_FxFlare missing - run Tools/gen_fx_assets.py"));
		}
		Glow->RegisterComponent();
		Glow->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}

	Light = NewObject<UPointLightComponent>(this, TEXT("Light"));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(static_cast<float>(LightIntensityCd));
	Light->SetAttenuationRadius(static_cast<float>(LightRangeM * M));
	Light->SetLightColor(FLinearColor(1.0f, 0.92f, 0.78f));   // burning magnesium, warmish
	Light->SetCastShadows(false);   // one big movable shadow light = perf trap
	Light->RegisterComponent();
	Light->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	SetLifeSpan(static_cast<float>(BurnS + 2.0));
}

void AShitfireFlareActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeS += DeltaSeconds;
	const double M = ShitfireCoords::MetersToUE;

	// descend + slow pendulum sway
	const double Sway = SwayAmpM * FMath::Sin(2.0 * PI * AgeS / SwayPeriodS);
	const double Sway2 = SwayAmpM * 0.6 * FMath::Sin(2.0 * PI * AgeS / (SwayPeriodS * 1.61));
	SetActorLocation(SpawnUE + FVector(Sway * M, Sway2 * M, -DescentMps * AgeS * M));

	// last 15% of burn: gutter out
	const double Frac = AgeS / BurnS;
	if (Frac > 1.0)
	{
		if (Glow) { Glow->SetVisibility(false); }
		if (Light) { Light->SetVisibility(false); }
		return;
	}
	const double Gutter = (Frac > 0.85)
		? FMath::Clamp((1.0 - Frac) / 0.15, 0.0, 1.0)
		: 1.0;
	// flicker: two incommensurate sines, never fully dark
	const double Flicker = 0.85 + 0.15 * FMath::Sin(AgeS * 37.0) * FMath::Sin(AgeS * 11.3);
	if (Light)
	{
		Light->SetIntensity(static_cast<float>(LightIntensityCd * Gutter * Flicker));
	}
	if (GlowMid)
	{
		// absolute nits - must sit in the sunlit-scene range under the fixed
		// EV100 ~13 exposure (see gen_fx_assets.py unlit_additive_glow note)
		GlowMid->SetScalarParameterValue(TEXT("Intensity"),
			static_cast<float>(25000.0 * Gutter * Flicker));
	}
}
