#include "units/ShitfireUnitsActor.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"

#include "Algo/Count.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

// Engine-free sim core - prebuilt /fp:precise code, read-only consumption.
#include "shitfire/scenario/place_units.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_config.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim/mission_machine.h"
#include "shitfire/sim_core.h"
#include "shitfire/terrain/heightfield.h"

namespace
{
	// sim yaw (radians, three.js rotation.y) -> UE yaw degrees (same mapping
	// as the world actor's file-local helper).
	float SimRotToUEYaw(double RotY)
	{
		return static_cast<float>(-FMath::RadiansToDegrees(RotY));
	}

	FLinearColor Hex(uint32 Rgb)
	{
		return FLinearColor::FromSRGBColor(
			FColor((Rgb >> 16) & 0xFF, (Rgb >> 8) & 0xFF, Rgb & 0xFF));
	}

	// Browser palette (05-units-scenario.js / 04-world.js / 03-op-battery.js)
	const FLinearColor ColEnemyBody = Hex(0x59180F);   // 05:5 oxblood
	const FLinearColor ColFriendly = Hex(0x22406E);    // 05:49 navy
	const FLinearColor ColCiv = Hex(0xEFE8D4);         // 04:70 bone
	const FLinearColor ColHut = Hex(0xC9B891);         // 05:15
	const FLinearColor ColBunker = Hex(0x6E665C);      // 05:56
	const FLinearColor ColBunkerScorch = Hex(0x3A352F);   // 10:1384
	const FLinearColor ColKaiju = Hex(0x8A3428);       // 05:1241
	const FLinearColor ColKaijuScorch = Hex(0x4A2B24); // 10:1385
	const FLinearColor ColVehDead = Hex(0x211D18);     // 05:19
	const FLinearColor ColFlagPole = Hex(0x8A8A80);    // 05:22
	const FLinearColor ColFlagBanner = Hex(0x2E5FA0);  // 05:24
	const FLinearColor ColKpPost = Hex(0xE8E4D8);      // 05:41
	const FLinearColor ColTowerLeg = Hex(0x6B5B43);    // 03:23
	const FLinearColor ColTowerDeck = Hex(0x7A6A50);   // 03:24
	const FLinearColor ColTent = Hex(0x6B6A52);        // 03:103
	const FLinearColor ColFlash = Hex(0xFFE9A0);       // 05:28
	const FLinearColor ColFlame = Hex(0xFF9A40);       // 05:61
	const FLinearColor ColDust(0.55f, 0.47f, 0.36f);   // sand (dust ribbon)

	// figScale (05:1293) - the legibility floor. CONFIG.GFX values 00:238-241.
	constexpr double LegFloorMil = 1.8;
	constexpr double LegMaxScale = 2.5;
	constexpr double LegOnsetM = 800.0;
	double FigScaleFor(double DistM, double BaseHM)
	{
		if (DistM < LegOnsetM) { return 1.0; }
		const double S = (LegFloorMil * DistM / 1000.0) / BaseHM;
		return FMath::Clamp(S, 1.0, LegMaxScale);
	}

	// Browser lie-over poses (see class header).
	constexpr double ProneRollRad = 1.2;    // 10:1261
	constexpr double DeadRollRad = 1.35;    // 10:1372/:1379
	constexpr double ProneLiftM = 0.3;      // 10:1260 "+0.3 y lift"

	// Muzzle-flash types (17-loop-boot.js:156-160 flashOn gate).
	bool TypeHasFlashes(const std::string& T)
	{
		return T == "strongpoint" || T == "assault" || T == "bunker" ||
			T == "raid" || T == "defense" || T == "callin" || T == "occupied";
	}

	// Dust ribbon tuning (presentation flavor; research 09 section 5).
	constexpr double DustEmitPeriodS = 0.35;
	constexpr double DustLifeS = 3.5;
	constexpr double DustMinSpeedMps = 1.0;
	constexpr int32 DustPoolSize = 40;
}

AShitfireUnitsActor::AShitfireUnitsActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

// ---- build ---------------------------------------------------------------

void AShitfireUnitsActor::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	Host = GI ? GI->GetSubsystem<UShitfireSimHost>() : nullptr;
	Sim = Host ? Host->GetSimCore() : nullptr;
	if (!Sim)
	{
		UE_LOG(LogShitfire, Error, TEXT("units: no live SimCore - nothing to present"));
		return;
	}

	Assets = FShitfireUnitAssets::Load();

	BuildFigures();
	BuildVehicles();
	BuildStructures();
	BuildTowerAndBattery();
	BuildEffects();
	ApplyInitialLayout();

	Host->OnPresentationCue.AddDynamic(this, &AShitfireUnitsActor::HandlePresentationCue);

	if (FParse::Param(FCommandLine::Get(), TEXT("ShitfireUnitsProbe")))
	{
		StartUnitsProbe();
	}
}

void AShitfireUnitsActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Host)
	{
		Host->OnPresentationCue.RemoveDynamic(this, &AShitfireUnitsActor::HandlePresentationCue);
	}
	Super::EndPlay(Reason);
}

UStaticMeshComponent* AShitfireUnitsActor::MakeStatic(const TCHAR* Name,
	UStaticMesh* Mesh, UMaterialInterface* BaseMat, const FLinearColor& Tint,
	UMaterialInstanceDynamic** OutMid)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	if (Mesh) { C->SetStaticMesh(Mesh); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetMobility(EComponentMobility::Movable);
	if (BaseMat)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMat, this);
		// M_Placeholder takes "Color"; M_UnitBody takes "Tint"; the FX
		// materials take "Color" - set both, extra params are ignored.
		Mid->SetVectorParameterValue(TEXT("Color"), Tint);
		Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
		for (int32 i = 0; i < FMath::Max(1, C->GetNumMaterials()); i++)
		{
			C->SetMaterial(i, Mid);
		}
		Keep.Add(Mid);
		if (OutMid) { *OutMid = Mid; }
	}
	C->RegisterComponent();
	C->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	C->SetVisibility(false);
	Keep.Add(C);
	return C;
}

void AShitfireUnitsActor::MakeFigure(FFigure& F, const TCHAR* Name,
	const FLinearColor& Tint, bool bTintBody, double BaseHeightM)
{
	F.BaseHeightM = BaseHeightM;
	if (Assets.bHumanoidReal)
	{
		USkeletalMeshComponent* S = NewObject<USkeletalMeshComponent>(this, Name);
		S->SetSkeletalMesh(Assets.SoldierMesh);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetMobility(EComponentMobility::Movable);
		S->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		// 7-20 px at mission ranges: never tick pose off-screen, and let the
		// engine thin distant update rates (research 09 "anim ticking LOD'd").
		S->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		S->bEnableUpdateRateOptimizations = true;
		if (bTintBody && Assets.UnitBodyMat)
		{
			UMaterialInstanceDynamic* Mid =
				UMaterialInstanceDynamic::Create(Assets.UnitBodyMat, this);
			Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
			for (int32 i = 0; i < S->GetNumMaterials(); i++)
			{
				S->SetMaterial(i, Mid);
			}
			Keep.Add(Mid);
		}
		S->RegisterComponent();
		S->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		S->SetVisibility(false);
		Keep.Add(S);
		F.Skel = S;

		// Scale from the measured native rig height - NEVER GetBounds(), the
		// asset-claimed box is prop-grid junk (see SoldierNativeHeightCm).
		F.MeshScale = (BaseHeightM * ShitfireCoords::MetersToUE) /
			FShitfireUnitAssets::SoldierNativeHeightCm;

		// Rigid head part (Quaternius modular body carries it separately).
		if (Assets.SoldierHead && S->GetSkeletalMeshAsset())
		{
			// The ANIMATED head bone is the exact-named "Head" (UNITCAL bone
			// dump); the hash-named sibling is an unanimated attachment stub
			// that idles at ground level - attaching there put giant
			// unscaled heads on the beach (2026-09-04 probe, take 1).
			const FReferenceSkeleton& Ref =
				S->GetSkeletalMeshAsset()->GetRefSkeleton();
			const int32 HeadIdx = Ref.FindBoneIndex(FName(TEXT("Head")));
			if (HeadIdx != INDEX_NONE)
			{
				UStaticMeshComponent* H = NewObject<UStaticMeshComponent>(this);
				H->SetStaticMesh(Assets.SoldierHead);
				H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				H->SetMobility(EComponentMobility::Movable);
				H->RegisterComponent();
				// INCLUDING scale: the head was authored against the same
				// 200 cm rig, so it must inherit the body's fit + legibility
				// scaling through the bone.
				H->AttachToComponent(S,
					FAttachmentTransformRules::SnapToTargetIncludingScale,
					Ref.GetBoneName(HeadIdx));
				// The head static mesh is authored in the body's feet-origin
				// (component) space at bind pose (box z 162..219 cm), not in
				// head-bone space. Snapping it to the bone put the helmet a
				// head-height above the skull once the re-import gave the
				// Head bone its true 157 cm position (2026-10-06 probe). So
				// offset it by the inverse of the bone's component-space
				// ref-pose transform: it then sits on the skull at bind pose
				// and follows the bone's animation from there.
				FTransform HeadRefCS = FTransform::Identity;
				for (int32 B = HeadIdx; B != INDEX_NONE; B = Ref.GetParentIndex(B))
				{
					HeadRefCS = HeadRefCS * Ref.GetRefBonePose()[B];
				}
				H->SetRelativeTransform(HeadRefCS.Inverse());
				Keep.Add(H);
				F.Head = H;
			}
		}
	}
	else
	{
		// Box fallback: the browser body silhouette (04:68, 0.8 x 2.0 x 0.8).
		F.Box = MakeStatic(Name, Assets.Cube, Assets.PlaceholderMat, Tint);
		F.MeshScale = 1.0;
	}
	F.IdleRate = FMath::FRandRange(0.9, 1.1);   // sanctioned unseeded flavor
}

void AShitfireUnitsActor::BuildFigures()
{
	Troops.SetNum(8);   // units.troops (05:16)
	for (int32 i = 0; i < 8; i++)
	{
		MakeFigure(Troops[i], *FString::Printf(TEXT("Troop%d"), i),
			ColEnemyBody, /*bTintBody=*/false, FShitfireUnitAssets::TroopHeightM);
	}
	Squad.SetNum(6);    // units.fSquad (05:48)
	for (int32 i = 0; i < 6; i++)
	{
		MakeFigure(Squad[i], *FString::Printf(TEXT("Squad%d"), i),
			ColFriendly, /*bTintBody=*/true, FShitfireUnitAssets::TroopHeightM);
	}
	const shitfire::SimCore::Parts P = Sim->parts();
	Civs.SetNum(static_cast<int32>(P.world->civs.size()));
	for (int32 i = 0; i < Civs.Num(); i++)
	{
		MakeFigure(Civs[i], *FString::Printf(TEXT("Civ%d"), i),
			ColCiv, /*bTintBody=*/true, FShitfireUnitAssets::CivHeightM);
	}
}

void AShitfireUnitsActor::FitVehicleMesh(FVehicle& V, UStaticMesh* Mesh,
                                         double TargetLengthM)
{
	if (!V.Mesh || !Mesh) { return; }
	V.Mesh->SetStaticMesh(Mesh);
	const FBox B = Mesh->GetBoundingBox();
	const FVector Size = B.GetSize();
	// Long horizontal axis -> the browser box's X (heading axis). If the
	// model was authored long-in-Y, yaw it -90 so +X is forward.
	const double LongUE = FMath::Max(Size.X, Size.Y);
	V.YawOffsetDeg = (Size.Y > Size.X) ? -90.0 : 0.0;
	V.FitScale = (TargetLengthM * ShitfireCoords::MetersToUE) / FMath::Max(LongUE, 1.0);
	V.ZLiftUE = -B.Min.Z * V.FitScale;
}

void AShitfireUnitsActor::BuildVehicles()
{
	Vehicles.SetNum(4);   // units.vehicles (05:17)
	for (int32 i = 0; i < 4; i++)
	{
		FVehicle& V = Vehicles[i];
		V.Mesh = MakeStatic(*FString::Printf(TEXT("Vehicle%d"), i),
			Assets.Cube, nullptr, FLinearColor::White);
		// Flicker flame over a dead hull (05:60-67, 05:853).
		V.Flame = MakeStatic(*FString::Printf(TEXT("VehFlame%d"), i),
			Assets.Sphere, Assets.FxFlashMat, ColFlame);
		if (UMaterialInstanceDynamic* Mid =
			Cast<UMaterialInstanceDynamic>(V.Flame->GetMaterial(0)))
		{
			Mid->SetScalarParameterValue(TEXT("Intensity"), 30000.0);
		}
		V.Flame->SetWorldScale3D(FVector(3.5));   // browser sprite scale 3.5
	}
	// Dust puff pool (all vehicles share it).
	DustPool.SetNum(DustPoolSize);
	for (int32 i = 0; i < DustPoolSize; i++)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		DustPool[i].Mesh = MakeStatic(*FString::Printf(TEXT("Dust%d"), i),
			Assets.Sphere, Assets.FxDirtMat, ColDust, &Mid);
		DustPool[i].Mid = Mid;
		if (Mid) { Mid->SetScalarParameterValue(TEXT("NoiseScaleM"), 3.0); }
	}
}

void AShitfireUnitsActor::BuildStructures()
{
	Huts.SetNum(3);       // units.huts (05:20) - scenario target/compound huts
	for (int32 i = 0; i < 3; i++)
	{
		Huts[i] = MakeStatic(*FString::Printf(TEXT("UnitHut%d"), i),
			Assets.Cube, Assets.PlaceholderMat, ColHut);
	}
	FlagPole = MakeStatic(TEXT("FlagPole"), Assets.Cube, Assets.PlaceholderMat, ColFlagPole);
	FlagBanner = MakeStatic(TEXT("FlagBanner"), Assets.Cube, Assets.PlaceholderMat, ColFlagBanner);
	UMaterialInstanceDynamic* Mid = nullptr;
	Bunker = MakeStatic(TEXT("UnitBunker"), Assets.Cube, Assets.PlaceholderMat,
		ColBunker, &Mid);
	BunkerMid = Mid;
	KpPosts.SetNum(2);    // units.kpPosts (05:40)
	for (int32 i = 0; i < 2; i++)
	{
		KpPosts[i] = MakeStatic(*FString::Printf(TEXT("KpPost%d"), i),
			Assets.Cube, Assets.PlaceholderMat, ColKpPost);
	}
}

void AShitfireUnitsActor::BuildTowerAndBattery()
{
	const shitfire::SimCore::Parts P = Sim->parts();
	const double TH = shitfire::scenario::CameraConstants{}.towerHeight;   // 300

	// OP watchtower: 4 legs + 2 braces + deck, NO RAILING (03-op-battery.js
	// 22-62; the measured no-parapet rationale lives there - do not add one).
	const double OpX = P.op->x, OpZ = P.op->z;
	const double OpH = GroundH(OpX, OpZ);
	auto TowerPart = [&](const TCHAR* Name, const FLinearColor& C,
	                     double Ox, double Oz, double Oy,
	                     double W, double H, double D, double PitchDeg)
	{
		UStaticMeshComponent* M = MakeStatic(Name, Assets.Cube,
			Assets.PlaceholderMat, C);
		M->SetWorldLocation(ShitfireCoords::SimToUE(OpX + Ox, OpH + Oy, OpZ + Oz));
		M->SetWorldScale3D(FVector(W, D, H));   // sim W/D -> UE X/Y, H -> Z
		M->SetWorldRotation(FRotator(PitchDeg, 0, 0));
		M->SetVisibility(true);
		TowerParts.Add(M);
	};
	int32 L = 0;
	for (const auto& Off : { FVector2D(-1.6, -1.6), FVector2D(1.6, -1.6),
	                         FVector2D(-1.6, 1.6), FVector2D(1.6, 1.6) })
	{
		TowerPart(*FString::Printf(TEXT("TowerLeg%d"), L++), ColTowerLeg,
			Off.X, Off.Y, TH / 2, 0.35, TH, 0.35, 0);
	}
	for (int32 i = 0; i < 2; i++)   // braces lean +/-0.42 rad (03:32)
	{
		TowerPart(*FString::Printf(TEXT("TowerBrace%d"), i), ColTowerLeg,
			0, (i ? 1 : -1) * 1.6, TH * 0.4, 0.18, TH * 0.55, 0.18,
			(i ? 1 : -1) * FMath::RadiansToDegrees(0.42));
	}
	TowerPart(TEXT("TowerDeck"), ColTowerDeck, 0, 0, TH, 4.6, 0.3, 4.6, 0);

	// Friendly battery: 2 guns + tent (03:94-107). Cannon models at the gun
	// spots (the mandate's "Cannon models for the battery emplacement");
	// browser gun Box(1.6, 1.4, 5.8) - long axis 5.8 m preserved.
	const double BX = P.battery->x, BZ = P.battery->z;
	for (int32 i = 0; i < 2; i++)
	{
		const double Ox = i * 16 - 8, Oz = i * 7;
		const double GX = BX + Ox, GZ = BZ + Oz;
		const double G = GroundH(GX, GZ);
		UStaticMeshComponent* Gun = MakeStatic(
			*FString::Printf(TEXT("BatteryGun%d"), i), Assets.Cube,
			Assets.CannonMesh ? nullptr : Assets.PlaceholderMat, Hex(0x3E4A34));
		if (Assets.CannonMesh)
		{
			FVehicle Fit;
			Fit.Mesh = Gun;
			FitVehicleMesh(Fit, Assets.CannonMesh, FShitfireUnitAssets::BatteryGunLengthM);
			Gun->SetWorldScale3D(FVector(Fit.FitScale));
			Gun->SetWorldLocation(ShitfireCoords::SimToUE(GX, G, GZ)
				+ FVector(0, 0, Fit.ZLiftUE));
			Gun->SetWorldRotation(FRotator(0,
				SimRotToUEYaw(0.5 + i * 0.15) + Fit.YawOffsetDeg + 90.0, 0));
			// Wheels part shares the GLB origin - same-transform sibling.
			if (Assets.CannonWheels)
			{
				UStaticMeshComponent* W = MakeStatic(
					*FString::Printf(TEXT("BatteryGunWheels%d"), i),
					Assets.CannonWheels, nullptr, FLinearColor::White);
				W->SetWorldTransform(Gun->GetComponentTransform());
				W->SetVisibility(true);
				BatteryParts.Add(W);
			}
		}
		else
		{
			Gun->SetWorldScale3D(FVector(1.6, 5.8, 1.4));
			Gun->SetWorldLocation(ShitfireCoords::SimToUE(GX, G + 0.8, GZ));
			Gun->SetWorldRotation(FRotator(0, SimRotToUEYaw(0.5 + i * 0.15), 0));
		}
		Gun->SetVisibility(true);
		BatteryParts.Add(Gun);
	}
	{
		const double TX = BX - 14, TZ = BZ + 10;
		UStaticMeshComponent* Tent = MakeStatic(TEXT("BatteryTent"),
			Assets.Cube, Assets.PlaceholderMat, ColTent);
		Tent->SetWorldScale3D(FVector(6, 4.5, 2.6));
		Tent->SetWorldLocation(ShitfireCoords::SimToUE(TX, GroundH(TX, TZ) + 1.3, TZ));
		Tent->SetVisibility(true);
		BatteryParts.Add(Tent);
	}
}

void AShitfireUnitsActor::BuildEffects()
{
	Flashes.SetNum(6);   // units.flashes (05:27-32), sprite scale 4
	for (int32 i = 0; i < 6; i++)
	{
		Flashes[i] = MakeStatic(*FString::Printf(TEXT("MuzzleFlash%d"), i),
			Assets.Sphere, Assets.FxFlashMat, ColFlash);
		if (UMaterialInstanceDynamic* Mid =
			Cast<UMaterialInstanceDynamic>(Flashes[i]->GetMaterial(0)))
		{
			Mid->SetScalarParameterValue(TEXT("Intensity"), 60000.0);
		}
		Flashes[i]->SetWorldScale3D(FVector(4.0));
	}
	Puffs.SetNum(4);     // units.smokePuffs (05:33-38)
	PuffMids.SetNum(4);
	for (int32 i = 0; i < 4; i++)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		Puffs[i] = MakeStatic(*FString::Printf(TEXT("SmokePuff%d"), i),
			Assets.Sphere, Assets.FxSmokeMat, Hex(0x8A877E), &Mid);
		PuffMids[i] = Mid;
	}
}

void AShitfireUnitsActor::ApplyInitialLayout()
{
	using namespace shitfire::scenario;
	const shitfire::SimCore::Parts P = Sim->parts();
	const ScenarioState& S = *P.scenario;
	const UnitLayout& U = *P.units;

	// Troops / squad from the ported placeUnits layout (stream #7 already
	// drawn core-side; positions are data here).
	for (int32 i = 0; i < 8; i++)
	{
		FFigure& F = Troops[i];
		F.bVisible = U.troops[i].visible;
		F.SimX = U.troops[i].x;
		F.SimZ = U.troops[i].z;
		F.YawSimRad = U.troops[i].rotY;
		F.LastX = F.SimX; F.LastZ = F.SimZ;
		if (S.type == "chow")   // gull scale (05:1250), sim (x,y,z) -> UE (X,Y,Z)
		{
			F.GullScale = FVector(0.5, 0.5, 0.42);
		}
	}
	for (int32 i = 0; i < 6; i++)
	{
		FFigure& F = Squad[i];
		F.bVisible = U.fSquadVisible[i];
		F.SimX = U.fSquadX[i];
		F.SimZ = U.fSquadZ[i];
		F.LastX = F.SimX; F.LastZ = F.SimZ;
	}

	// Vehicles: mesh choice per scenario type (mandate: convoy Humvee/M939,
	// strongpoint statics Tank; every long axis pinned to the browser's 5 m
	// mil-relation box).
	UStaticMesh* PerSlot[4] = { nullptr, nullptr, nullptr, nullptr };
	if (S.type == "convoy")
	{
		PerSlot[0] = Assets.HumveeMesh;
		PerSlot[1] = PerSlot[2] = PerSlot[3] = Assets.TruckMesh;
	}
	else if (S.type == "strongpoint")
	{
		PerSlot[0] = PerSlot[1] = Assets.TankMesh;
	}
	else if (S.type == "raid")
	{
		PerSlot[0] = Assets.HumveeMesh;
	}
	else   // occupied / battery / wreck and any future truck user
	{
		PerSlot[0] = PerSlot[1] = PerSlot[2] = PerSlot[3] = Assets.TruckMesh;
	}
	for (int32 i = 0; i < 4; i++)
	{
		FVehicle& V = Vehicles[i];
		V.bVisible = U.vehicles[i].visible;
		V.SimX = U.vehicles[i].x;
		V.SimZ = U.vehicles[i].z;
		V.YawSimRad = U.vehicles[i].rotY;
		V.RotZSimRad = U.vehicles[i].rotZ;
		V.LayoutScale = FVector(U.vehicles[i].sx, U.vehicles[i].sz, U.vehicles[i].sy);
		UStaticMesh* Mesh = PerSlot[i] ? PerSlot[i] : Assets.Cube;
		FitVehicleMesh(V, Mesh, FShitfireUnitAssets::VehicleLengthM);
		if (!PerSlot[i])   // cube fallback: browser box 5.0 x 2.3 x 2.1
		{
			V.FitScale = 1.0;
			V.YawOffsetDeg = 0.0;
			V.ZLiftUE = 0.0;
			V.Mesh->SetWorldScale3D(FVector(5.0, 2.3, 2.1));
		}
		if (U.vehicles[i].deadMat) { V.bDeadShown = false; /* applied in tick */ }
		// Strongpoint statics get the Tank turret same-origin.
		if (Mesh == Assets.TankMesh && Assets.TankTurret)
		{
			UStaticMeshComponent* T = MakeStatic(
				*FString::Printf(TEXT("VehTurret%d"), i),
				Assets.TankTurret, nullptr, FLinearColor::White);
			T->AttachToComponent(V.Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			T->SetVisibility(V.bVisible);
		}
	}

	// Scenario huts + flag (strongpoint compound / battery + qfp fireBase).
	for (int32 i = 0; i < 3; i++)
	{
		const UnitHut& H = U.huts[i];
		Huts[i]->SetVisibility(H.visible);
		if (H.visible)
		{
			// browser: Box(7, 3.0, 5) centred at H + 1.6 (05:15, 05:1110)
			Huts[i]->SetWorldScale3D(FVector(7, 5, 3));
			Huts[i]->SetWorldLocation(
				ShitfireCoords::SimToUE(H.x, GroundH(H.x, H.z) + 1.6, H.z));
			Huts[i]->SetWorldRotation(FRotator(0, SimRotToUEYaw(H.rotY), 0));
		}
	}
	if (U.flagVisible)   // pole + banner at the friendly/compound marker (05:1114)
	{
		const double FX = U.flagX, FZ = U.flagZ;
		const double FH = GroundH(FX, FZ);
		FlagPole->SetWorldScale3D(FVector(0.3, 0.3, 7));
		FlagPole->SetWorldLocation(ShitfireCoords::SimToUE(FX, FH + 3.5, FZ));
		FlagPole->SetVisibility(true);
		FlagBanner->SetWorldScale3D(FVector(2.4, 0.15, 1.4));
		FlagBanner->SetWorldLocation(ShitfireCoords::SimToUE(FX + 1.2, FH + 6.3, FZ));
		FlagBanner->SetVisibility(true);
	}

	// Bunker (bunker / battery emplacement / kaiju crab, 05:55-58, 05:1240).
	if (U.bunkerVisible)
	{
		Bunker->SetVisibility(true);
		Bunker->SetWorldScale3D(FVector(7.0 * U.bunkerSx, 7.0 * U.bunkerSz,
			2.6 * U.bunkerSy));
		if (S.type == "kaiju")
		{
			if (BunkerMid) { BunkerMid->SetVectorParameterValue(TEXT("Color"), ColKaiju); }
			Bunker->SetWorldLocation(ShitfireCoords::SimToUE(S.enemy.x, 4.0, S.enemy.z));
		}
		else
		{
			Bunker->SetWorldLocation(ShitfireCoords::SimToUE(
				U.bunkerX, GroundH(U.bunkerX, U.bunkerZ) + 1.3, U.bunkerZ));
		}
	}

	// KP posts (05:1270-1274): Box(0.5, 2.8, 0.5) at H + 1.4.
	for (int32 i = 0; i < 2; i++)
	{
		const bool bHas = i < static_cast<int32>(S.kps.size());
		KpPosts[i]->SetVisibility(bHas);
		if (bHas)
		{
			KpPosts[i]->SetWorldScale3D(FVector(0.5, 0.5, 2.8));
			KpPosts[i]->SetWorldLocation(ShitfireCoords::SimToUE(
				S.kps[i].x, GroundH(S.kps[i].x, S.kps[i].z) + 1.4, S.kps[i].z));
		}
	}

	// Smoke-puff anchors (05:1118 strongpoint compound, 05:1263 chow bbq).
	if (S.type == "strongpoint" && S.compound.has_value())
	{
		bPuffsOn = true;
		PuffBaseX = S.compound->x - 6;
		PuffBaseZ = S.compound->z + 3;
		PuffBaseY = GroundH(PuffBaseX, PuffBaseZ);
	}
	else if (S.type == "chow" && S.bbq.has_value())
	{
		bPuffsOn = true;
		PuffBaseX = S.bbq->x;
		PuffBaseZ = S.bbq->z;
		PuffBaseY = FMath::Max(GroundH(PuffBaseX, PuffBaseZ), 0.0);
	}

	UE_LOG(LogShitfire, Log,
		TEXT("units: layout applied for '%hs' (troops %d, squad %d, civs %d, vehicles %d)"),
		S.type.c_str(),
		static_cast<int32>(Algo::CountIf(Troops, [](const FFigure& F) { return F.bVisible; })),
		static_cast<int32>(Algo::CountIf(Squad, [](const FFigure& F) { return F.bVisible; })),
		Civs.Num(),
		static_cast<int32>(Algo::CountIf(Vehicles, [](const FVehicle& V) { return V.bVisible; })));
}

// ---- cues ----------------------------------------------------------------

void AShitfireUnitsActor::HandlePresentationCue(EShitfirePresentation Kind,
	double SimT, FVector UEPos, double Num0)
{
	switch (Kind)
	{
	case EShitfirePresentation::DisperseTroops: OnDisperseTroops(); break;
	case EShitfirePresentation::TroopsDown:     OnTroopsDown(false); break;
	case EShitfirePresentation::TwinTroopsDown: OnTroopsDown(true); break;
	case EShitfirePresentation::VehiclesTipped: OnVehiclesTipped(); break;
	case EShitfirePresentation::BunkerScorched:
		bBunkerScorched = true;
		if (BunkerMid) { BunkerMid->SetVectorParameterValue(TEXT("Color"), ColBunkerScorch); }
		break;
	case EShitfirePresentation::KaijuScorched:
		bKaijuDead = true;
		bEnemyAliveVisual = false;
		if (BunkerMid) { BunkerMid->SetVectorParameterValue(TEXT("Color"), ColKaijuScorch); }
		break;
	default:
		break;   // burst/flare/... belong to the FX subsystem
	}
}

void AShitfireUnitsActor::OnDisperseTroops()
{
	// Deterministic endpoints: stream #11 via the core's disperseOffsets,
	// one delta per VISIBLE figure in mesh order (sim/events.h contract).
	// The browser teleports (10:1258-1261); the ~1.6 s slide to the same
	// endpoints is sanctioned presentation flavor.
	if (bDispersed) { return; }
	bDispersed = true;
	std::size_t VisibleCount = 0;
	for (const FFigure& F : Troops) { if (F.bVisible) { VisibleCount++; } }
	const shitfire::SimCore::Parts P = Sim->parts();
	const std::vector<shitfire::sim::Vec2> Deltas =
		shitfire::sim::disperseOffsets(P.scenario->seed, VisibleCount);
	std::size_t k = 0;
	for (FFigure& F : Troops)
	{
		if (!F.bVisible || k >= Deltas.size()) { continue; }
		F.bLerping = true;
		F.LerpFromX = F.SimX; F.LerpFromZ = F.SimZ;
		F.LerpToX = F.SimX + Deltas[k].x;
		F.LerpToZ = F.SimZ + Deltas[k].z;
		F.LerpT = 0;
		k++;
	}
}

void AShitfireUnitsActor::OnTroopsDown(bool bTwinElement)
{
	// 10:1369-1379 - twin element = figures 4-7; main element = 0-3 when a
	// twin exists, else every visible figure. Idempotent, dead overrides prone.
	const shitfire::SimCore::Parts P = Sim->parts();
	const bool bHasTwin = P.scenario->twinTgt.has_value();
	if (bTwinElement) { bTwinDown = true; }
	else { bEnemyAliveVisual = false; }
	for (int32 i = 0; i < Troops.Num(); i++)
	{
		const bool bInElement = bTwinElement ? (i >= 4)
			: (bHasTwin ? (i < 4) : true);
		if (bInElement && Troops[i].bVisible)
		{
			Troops[i].Pose = EShitfireFigurePose::Dead;
			Troops[i].bLerping = false;
		}
	}
}

void AShitfireUnitsActor::OnVehiclesTipped()
{
	// 10:1382-1383 - strongpoint statics only, DESTROYED only.
	for (FVehicle& V : Vehicles)
	{
		if (V.bVisible) { V.bTipped = true; }
	}
}

// ---- per-frame -----------------------------------------------------------

void AShitfireUnitsActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Sim || !Host || !Host->IsMissionLive()) { return; }
	const double SimNow = Host->GetSimNow();
	const double Dt = DeltaSeconds;

	// One-shot calibration log (probe only): the ASSET-claimed soldier
	// bounds are junk (272 x 222 x 69 m box, prop/bone grid in the GLB), so
	// the numbers that matter are the RENDERED component's bounds and where
	// the head/root bones actually sit - logged once from a live on-screen
	// figure so scale/upright constants come from data, not guesses.
	if (bProbe && !bDiagLogged && GetWorld()->GetTimeSeconds() > 2.5 &&
		Troops.Num() > 0 && Troops[0].Skel && Troops[0].bVisible)
	{
		bDiagLogged = true;
		USkeletalMeshComponent* S0 = Troops[0].Skel;
		const FBoxSphereBounds B = S0->Bounds;
		const FVector Sc = S0->GetComponentScale();
		const FVector Base = S0->GetComponentLocation();
		UE_LOG(LogShitfire, Display,
			TEXT("UNITCAL comp-bounds ext=(%.1f,%.1f,%.1f)cm scale=%.4f base-z=%.1f org-z=%.1f"),
			B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z, Sc.X, Base.Z, B.Origin.Z);
		if (const USkeletalMesh* SM = S0->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& Ref = SM->GetRefSkeleton();
			FString Names;
			for (int32 b = 0; b < FMath::Min(Ref.GetNum(), 40); b++)
			{
				Names += Ref.GetBoneName(b).ToString() + TEXT(" ");
			}
			UE_LOG(LogShitfire, Display, TEXT("UNITCAL bones(%d): %s"),
				Ref.GetNum(), *Names);
			for (int32 b = 0; b < Ref.GetNum(); b++)
			{
				const FString N = Ref.GetBoneName(b).ToString();
				if (N.Contains(TEXT("Head")) || b == 0 || N.Contains(TEXT("Foot")) ||
					N.Contains(TEXT("Root")))
				{
					const FVector W = S0->GetBoneLocation(Ref.GetBoneName(b));
					UE_LOG(LogShitfire, Display,
						TEXT("UNITCAL bone %s world=(%.1f,%.1f,%.1f) rel-z=%.1f"),
						*N, W.X, W.Y, W.Z, W.Z - Base.Z);
				}
			}
		}
	}

	UpdateTroops(SimNow, Dt);
	UpdateSquad(SimNow, Dt);
	UpdateCivs(Dt);
	if (ProbeConvoyT >= 0) { ProbeConvoyT += Dt; ForceConvoyDemo(ProbeConvoyT); }
	else { UpdateVehicles(SimNow, Dt); }
	UpdateBunker(SimNow);
	UpdateFlashes(SimNow);
	UpdatePuffs(SimNow);
	UpdateDust(Dt);
	LegibilityPass();
}

void AShitfireUnitsActor::UpdateTroops(double SimNow, double Dt)
{
	using namespace shitfire::scenario;
	const shitfire::SimCore::Parts P = Sim->parts();
	const ScenarioState& S = *P.scenario;

	// defense / qfp-post-H: line abreast on the advance azimuth at the feet
	// of S.enemy (05:984-1020 / 05:942-983); the core advances S.enemy.
	const bool bQfpLive = (S.type == "qfp" && S.hStarted.value_or(false));
	if (S.type == "defense" || bQfpLive)
	{
		const double TgtX = (S.type == "defense") ? P.op->x : S.fireBase->x;
		const double TgtZ = (S.type == "defense") ? P.op->z : S.fireBase->z;
		const double AAz = FMath::Atan2(TgtX - S.enemy.x, -(TgtZ - S.enemy.z));
		const double Rx = FMath::Cos(AAz), Rz = FMath::Sin(AAz);
		for (int32 i = 0; i < 8; i++)
		{
			FFigure& F = Troops[i];
			if (bQfpLive && !F.bVisible)
			{
				// H-hour install: the core consumed the position/yaw draws to
				// keep stream #6 aligned and dropped the values (the flagged
				// seam); the line-abreast drive owns positions from the next
				// frame anyway, so figures appear straight into formation,
				// facing the objective.
				F.bVisible = true;
				F.YawSimRad = AAz;
			}
			if (!F.bVisible) { continue; }
			const double L = (i - 3.5) * 9;
			F.SimX = S.enemy.x + Rx * L;
			F.SimZ = S.enemy.z + Rz * L;
			F.YawSimRad = AAz;
		}
	}

	// disperse scatter lerp -> prone at the deterministic endpoint.
	for (FFigure& F : Troops)
	{
		if (F.bLerping)
		{
			F.LerpT += Dt / 1.6;
			const double A = FMath::Clamp(F.LerpT, 0.0, 1.0);
			F.SimX = FMath::Lerp(F.LerpFromX, F.LerpToX, A);
			F.SimZ = FMath::Lerp(F.LerpFromZ, F.LerpToZ, A);
			if (A >= 1.0)
			{
				F.bLerping = false;
				if (F.Pose == EShitfireFigurePose::Standing)
				{
					F.Pose = EShitfireFigurePose::Prone;   // 10:1261/:1267
				}
			}
		}
		ApplyFigure(F, Dt);
	}
}

void AShitfireUnitsActor::UpdateSquad(double SimNow, double Dt)
{
	using namespace shitfire::scenario;
	const ScenarioState& S = *Sim->parts().scenario;

	// assault: six friendlies line abreast advancing on the objective
	// (05:906-915); pure S-field math, browser-verbatim.
	if (S.type == "assault" && S.fStart.has_value())
	{
		const double Adv = FMath::Min(*S.fSpeed * (SimNow - *S.ft0), *S.fAdvMax);
		const double Fx = FMath::Sin(*S.fAdvAz), Fz = -FMath::Cos(*S.fAdvAz);
		const double Rx = FMath::Cos(*S.fAdvAz), Rz = FMath::Sin(*S.fAdvAz);
		for (int32 i = 0; i < 6; i++)
		{
			const double L = (i - 2.5) * 8;
			Squad[i].SimX = S.fStart->x + Fx * Adv + Rx * L;
			Squad[i].SimZ = S.fStart->z + Fz * Adv + Rz * L;
			Squad[i].YawSimRad = *S.fAdvAz;
		}
	}
	for (FFigure& F : Squad) { ApplyFigure(F, Dt); }
}

void AShitfireUnitsActor::UpdateCivs(double Dt)
{
	// Positions are core truth (updateCivilians runs inside SimCore::advance;
	// 04:1135-1145) - hidden civs keep updating in the collateral model but
	// vanish from view (WORLD2).
	const shitfire::SimCore::Parts P = Sim->parts();
	for (int32 i = 0; i < Civs.Num(); i++)
	{
		const shitfire::scenario::Civ& C = P.world->civs[static_cast<size_t>(i)];
		FFigure& F = Civs[i];
		F.bVisible = !C.hidden;
		F.SimX = C.x;
		F.SimZ = C.z;
		ApplyFigure(F, Dt);
	}
}

void AShitfireUnitsActor::UpdateVehicles(double SimNow, double Dt)
{
	using namespace shitfire::scenario;
	const ScenarioState& S = *Sim->parts().scenario;

	const bool bConvoy = (S.type == "convoy" && S.veh.has_value() && S.path.has_value());
	for (int32 i = 0; i < 4; i++)
	{
		FVehicle& V = Vehicles[i];
		if (!V.Mesh) { continue; }
		V.Mesh->SetVisibility(V.bVisible);
		if (!V.bVisible)
		{
			if (V.Flame) { V.Flame->SetVisibility(false); }
			continue;
		}

		bool bDead = false;
		double SpeedMps = 0;
		if (bConvoy && i < static_cast<int32>(S.veh->size()))
		{
			const Veh& Cv = (*S.veh)[static_cast<size_t>(i)];
			SpeedMps = (Dt > 0)
				? FMath::Sqrt(FMath::Square(Cv.x - V.SimX) + FMath::Square(Cv.z - V.SimZ)) / Dt
				: 0;
			V.SimX = Cv.x;
			V.SimZ = Cv.z;
			bDead = Cv.dead;
			// heading from the path tangent at this hull's own distance
			// (05:848-855); dead hulls keep their last d = last heading.
			const shitfire::WorldXZ Dir = pathDir(*S.path, Cv.d);
			V.YawSimRad = 0;   // unused on the convoy path
			V.Mesh->SetWorldRotation(FRotator(0,
				FMath::RadiansToDegrees(FMath::Atan2(Dir.z, Dir.x)) + V.YawOffsetDeg, 0));
		}
		else
		{
			bDead = Sim->parts().units->vehicles[i].deadMat;
			FQuat Q = FRotator(0, SimRotToUEYaw(V.YawSimRad) + V.YawOffsetDeg, 0).Quaternion();
			const double Tip = V.bTipped ? 0.25 : V.RotZSimRad;   // 10:1383 / wreck 05:1207
			if (Tip != 0)
			{
				Q = Q * FQuat(FVector::ForwardVector, Tip);
			}
			V.Mesh->SetWorldRotation(Q);
		}

		const double G = GroundH(V.SimX, V.SimZ);
		V.Mesh->SetWorldLocation(ShitfireCoords::SimToUE(V.SimX, G, V.SimZ)
			+ FVector(0, 0, V.ZLiftUE));
		V.Mesh->SetWorldScale3D(FVector(V.FitScale) * V.LayoutScale);

		// Dead hull: blackened, never tipped (10:1183; visSetMat vehMatDead),
		// plus the flicker flame at H + 2.6 (05:853-854).
		if (bDead && !V.bDeadShown)
		{
			V.bDeadShown = true;
			UMaterialInterface* Base =
				Assets.UnitBodyMat ? Assets.UnitBodyMat : Assets.PlaceholderMat;
			if (Base)
			{
				UMaterialInstanceDynamic* Dead = UMaterialInstanceDynamic::Create(Base, this);
				Dead->SetVectorParameterValue(TEXT("Tint"), ColVehDead);
				Dead->SetVectorParameterValue(TEXT("Color"), ColVehDead);
				for (int32 m = 0; m < V.Mesh->GetNumMaterials(); m++)
				{
					V.Mesh->SetMaterial(m, Dead);
				}
				Keep.Add(Dead);
			}
		}
		if (V.Flame)
		{
			const bool bFlame = bDead && FMath::Fmod(SimNow * 7.0 + i, 1.0) < 0.6;
			V.Flame->SetVisibility(bFlame);
			if (bFlame)
			{
				V.Flame->SetWorldLocation(ShitfireCoords::SimToUE(V.SimX, G + 2.6, V.SimZ));
			}
		}

		// Dust ribbon behind a moving hull (research 09: dust sells motion).
		if (SpeedMps > DustMinSpeedMps)
		{
			V.DustCooldown -= Dt;
			if (V.DustCooldown <= 0)
			{
				V.DustCooldown = DustEmitPeriodS;
				SpawnDustPuff(V);
			}
		}
	}
}

void AShitfireUnitsActor::UpdateBunker(double SimNow)
{
	using namespace shitfire::scenario;
	const ScenarioState& S = *Sim->parts().scenario;
	if (!Bunker || !Bunker->IsVisible() || S.type != "kaiju") { return; }

	// The crab wades ashore on the core's S.enemy (05:867-890). bob is
	// sanctioned time-driven flavor (sim.now expression, no randomness).
	if (!bKaijuDead)
	{
		const double Bob = FMath::Sin(SimNow * 1.6) * 0.9;
		const double Y = FMath::Max(GroundH(S.enemy.x, S.enemy.z), 0.0) + 4.4 + Bob;
		Bunker->SetWorldLocation(ShitfireCoords::SimToUE(S.enemy.x, Y, S.enemy.z));
		if (S.path.has_value() && S.speed.has_value() && S.t0.has_value())
		{
			const double D = FMath::Clamp(*S.speed * (SimNow - *S.t0), 0.0, S.path->len);
			const shitfire::WorldXZ Kd = pathDir(*S.path, D);
			Bunker->SetWorldRotation(FRotator(0,
				FMath::RadiansToDegrees(FMath::Atan2(Kd.z, Kd.x)), 0));
		}
	}
	else
	{
		// settle (05:869-872): rotation.z = 0.6, y = max(H,0) + 2.2
		const double Y = FMath::Max(GroundH(S.enemy.x, S.enemy.z), 0.0) + 2.2;
		Bunker->SetWorldLocation(ShitfireCoords::SimToUE(S.enemy.x, Y, S.enemy.z));
		FQuat Q = Bunker->GetComponentQuat();
		Q = FQuat(FVector::ForwardVector, 0.6);
		Bunker->SetWorldRotation(Q);
	}
}

void AShitfireUnitsActor::UpdateFlashes(double SimNow)
{
	using namespace shitfire::scenario;
	const ScenarioState& S = *Sim->parts().scenario;

	// Suppression's ONE visual (17-loop-boot.js:156-168): flashes run only
	// while the element is alive, un-suppressed, and of a shooting type.
	const bool bFlashOn = bEnemyAliveVisual && SimNow >= S.suppressedUntil &&
		TypeHasFlashes(S.type);
	const bool bBunkerType = (S.type == "bunker");
	for (int32 i = 0; i < 6; i++)
	{
		if (!Flashes[i]) { continue; }
		const double Phase = i * 0.31;   // 05:31
		const bool bWindow = FMath::Fmod(SimNow * 0.53 + Phase, 1.9) < 0.06;
		const bool bVis = bFlashOn && (!bBunkerType || i == 0) && bWindow;
		Flashes[i]->SetVisibility(bVis);
		if (!bVis) { continue; }
		if (bBunkerType)
		{
			// bunker: one firing port on the structure (05:1199)
			const double BX = Sim->parts().units->bunkerX;
			const double BZ = Sim->parts().units->bunkerZ;
			Flashes[i]->SetWorldLocation(
				ShitfireCoords::SimToUE(BX, GroundH(BX, BZ) + 2.4, BZ));
		}
		else if (i < Troops.Num() && Troops[i].bVisible)
		{
			// flashes ride their figure (troopCluster 05:1090; defense/qfp
			// reposition per-frame at the same offset, 05:960/:1005)
			Flashes[i]->SetWorldLocation(ShitfireCoords::SimToUE(
				Troops[i].SimX, GroundH(Troops[i].SimX, Troops[i].SimZ) + 1.6,
				Troops[i].SimZ));
		}
		else
		{
			Flashes[i]->SetVisibility(false);
		}
	}
}

void AShitfireUnitsActor::UpdatePuffs(double SimNow)
{
	// 17-loop-boot.js:169-176, time-driven, no randomness: 10 s cycle,
	// rises 78 m, drifts (26, 6), scale 4 + ct*17, opacity 0.42 * (1 - ct).
	for (int32 i = 0; i < 4; i++)
	{
		if (!Puffs[i]) { continue; }
		if (!bPuffsOn) { Puffs[i]->SetVisibility(false); continue; }
		const double Off = i * 2.5;   // 05:37
		const double Ct = FMath::Fmod(SimNow + Off, 10.0) / 10.0;
		Puffs[i]->SetVisibility(true);
		Puffs[i]->SetWorldLocation(ShitfireCoords::SimToUE(
			PuffBaseX + Ct * 26, PuffBaseY + 2 + Ct * 78, PuffBaseZ + Ct * 6));
		// browser icosa radius (4 + ct*17) m; engine sphere is 1 m diameter
		Puffs[i]->SetWorldScale3D(FVector(2.0 * (4.0 + Ct * 17.0)));
		if (PuffMids[i])
		{
			PuffMids[i]->SetScalarParameterValue(TEXT("Opacity"), 0.42 * (1.0 - Ct));
		}
	}
}

void AShitfireUnitsActor::SpawnDustPuff(const FVehicle& V)
{
	for (FDustPuff& D : DustPool)
	{
		if (D.Age >= 0) { continue; }
		D.Age = 0;
		// unseeded scatter behind the hull - sanctioned presentation flavor
		const double Jx = FMath::FRandRange(-1.2, 1.2);
		const double Jz = FMath::FRandRange(-1.2, 1.2);
		const double G = GroundH(V.SimX + Jx, V.SimZ + Jz);
		D.BaseUE = ShitfireCoords::SimToUE(V.SimX + Jx, G + 0.6, V.SimZ + Jz);
		// gentle downwind drift + rise (wind = the sim's own brief wind)
		const shitfire::scenario::Wind& W = Sim->parts().world->wind;
		D.DriftUE = ShitfireCoords::SimDirToUE(W.dx, 0, W.dz).GetSafeNormal2D()
			* (0.35 * W.speed * ShitfireCoords::MetersToUE)
			+ FVector(0, 0, 0.5 * ShitfireCoords::MetersToUE);
		if (D.Mesh) { D.Mesh->SetVisibility(true); }
		return;
	}
}

void AShitfireUnitsActor::UpdateDust(double Dt)
{
	for (FDustPuff& D : DustPool)
	{
		if (D.Age < 0 || !D.Mesh) { continue; }
		D.Age += Dt;
		if (D.Age >= DustLifeS)
		{
			D.Age = -1;
			D.Mesh->SetVisibility(false);
			continue;
		}
		const double T = D.Age / DustLifeS;
		D.Mesh->SetWorldLocation(D.BaseUE + D.DriftUE * D.Age);
		D.Mesh->SetWorldScale3D(FVector(FMath::Lerp(1.5, 5.5, T)));
		if (D.Mid)
		{
			D.Mid->SetScalarParameterValue(TEXT("Opacity"), 0.8 * (1.0 - T));
		}
	}
}

void AShitfireUnitsActor::SetFigureClip(FFigure& F, int32 Clip)
{
	if (!F.Skel || !Assets.bAnimsReal || F.CurrentClip == Clip) { return; }
	// Clip swaps only on components the renderer is actually drawing. At
	// 7-20 px nothing else is visible anyway, and PlayAnimation on a
	// long-unrendered component (URO + OnlyTickPoseWhenRendered) crashed
	// in-engine: 2026-09-04 probe, AV 0x0 at this call site, first
	// OFF-SCREEN civilian clip change after 74 s while every on-screen
	// troop had transitioned fine. Skip WITHOUT latching CurrentClip so
	// the swap lands on the first rendered frame instead.
	if (F.CurrentClip >= 0 && !F.Skel->WasRecentlyRendered(0.2f)) { return; }
	F.CurrentClip = Clip;
	UAnimSequence* Seq = (Clip == 0) ? Assets.AnimIdle
		: (Clip == 1) ? Assets.AnimRun : Assets.AnimDeath;
	if (!Seq) { return; }
	F.Skel->PlayAnimation(Seq, /*bLooping=*/Clip != 2);
	if (Clip == 0)
	{
		F.Skel->SetPlayRate(static_cast<float>(F.IdleRate));
		// unseeded phase offset so a cluster never idles in lockstep
		F.Skel->SetPosition(FMath::FRandRange(0.f, Seq->GetPlayLength()), false);
	}
	else
	{
		F.Skel->SetPlayRate(1.0f);
	}
}

void AShitfireUnitsActor::ApplyFigure(FFigure& F, double Dt)
{
	USceneComponent* C = F.Skel ? static_cast<USceneComponent*>(F.Skel)
	                            : static_cast<USceneComponent*>(F.Box);
	if (!C) { return; }
	C->SetVisibility(F.bVisible, /*bPropagateToChildren=*/true);
	if (!F.bVisible) { return; }

	// movement estimate -> clip choice
	const double Speed = (Dt > 0)
		? FMath::Sqrt(FMath::Square(F.SimX - F.LastX) + FMath::Square(F.SimZ - F.LastZ)) / Dt
		: 0;
	F.LastX = F.SimX;
	F.LastZ = F.SimZ;

	const bool bDeadAnim = (F.Pose == EShitfireFigurePose::Dead && Assets.bAnimsReal && F.Skel);
	double LiftM = 0;
	FQuat Q = FRotator(0, SimRotToUEYaw(F.YawSimRad), 0).Quaternion();
	if (F.Pose == EShitfireFigurePose::Prone)
	{
		// browser prone: lie-over 1.2 rad about the local axis + 0.3 m lift
		// (10:1258-1261); feet-origin figure lays over about its boots.
		Q = Q * FQuat(FVector::ForwardVector, ProneRollRad);
		LiftM = ProneLiftM;
	}
	else if (F.Pose == EShitfireFigurePose::Dead && !bDeadAnim)
	{
		Q = Q * FQuat(FVector::ForwardVector, DeadRollRad);   // 10:1379
	}

	const double G = GroundH(F.SimX, F.SimZ);
	FVector Loc = ShitfireCoords::SimToUE(F.SimX, G + LiftM, F.SimZ);
	if (F.Box)
	{
		// center-pivot cube: browser body Box(0.8, 2.0, 0.8) lifted to feet
		Loc.Z += ((F.Pose == EShitfireFigurePose::Standing) ? 1.0 : 0.45)
			* ShitfireCoords::MetersToUE;
	}
	C->SetWorldLocation(Loc);
	C->SetWorldRotation(Q);

	if (F.Skel)
	{
		if (bDeadAnim) { SetFigureClip(F, 2); }
		else if (F.Pose == EShitfireFigurePose::Prone) { SetFigureClip(F, 0); }
		else
		{
			// Hysteresis so wandering civs do not thrash Idle<->Run at the
			// threshold (fewer PlayAnimation calls; see the crash note above).
			const bool bRun = F.bLerping ||
				Speed > ((F.CurrentClip == 1) ? 0.2 : 0.45);
			SetFigureClip(F, bRun ? 1 : 0);
		}
	}
	else if (F.Box)
	{
		F.Box->SetWorldScale3D(FVector(0.8, 0.8, 2.0));
	}
}

void AShitfireUnitsActor::LegibilityPass()
{
	// figScale (05:1293-1313): apparent-size floor so the observer can COUNT
	// bodies through the binos; never touches anything scored.
	const FVector Eye = EyeUE();
	auto Leg = [&](FFigure& F)
	{
		USceneComponent* C = F.Skel ? static_cast<USceneComponent*>(F.Skel)
		                            : static_cast<USceneComponent*>(F.Box);
		if (!C || !F.bVisible) { return; }
		const double DistM =
			FVector::Dist(C->GetComponentLocation(), Eye) / ShitfireCoords::MetersToUE;
		const double LegS = FigScaleFor(DistM, F.BaseHeightM);
		if (F.Skel)
		{
			C->SetWorldScale3D(FVector(F.MeshScale * LegS) * F.GullScale);
		}
		else
		{
			C->SetWorldScale3D(FVector(0.8, 0.8, 2.0) * LegS * F.GullScale);
		}
	};
	for (FFigure& F : Troops) { Leg(F); }
	for (FFigure& F : Squad) { Leg(F); }
	for (FFigure& F : Civs) { Leg(F); }
}

// ---- helpers -------------------------------------------------------------

double AShitfireUnitsActor::GroundH(double X, double Z) const
{
	return Sim->parts().heightfield->H(X, Z);
}

FVector AShitfireUnitsActor::EyeUE() const
{
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	return FVector::ZeroVector;
}

// ---- probe ---------------------------------------------------------------

void AShitfireUnitsActor::StartUnitsProbe()
{
	bProbe = true;
	ProbeStage = 0;
	UE_LOG(LogShitfire, Display,
		TEXT("units probe: state screenshots through 8.6/4.3 deg binos FOVs"));
	GetWorld()->GetTimerManager().SetTimer(ProbeTimer,
		FTimerDelegate::CreateUObject(this, &AShitfireUnitsActor::ProbeStep),
		3.0f, /*bLoop=*/true, /*FirstDelay=*/5.0f);
}

void AShitfireUnitsActor::ProbeStep()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) { return; }

	auto SetVFov = [&](double VDeg)
	{
		const double H = 2.0 * FMath::RadiansToDegrees(FMath::Atan(
			FMath::Tan(FMath::DegreesToRadians(VDeg) * 0.5) * 16.0 / 9.0));
		PC->PlayerCameraManager->SetFOV(static_cast<float>(H));
	};
	auto Shot = [&](const TCHAR* Name)
	{
		PC->ConsoleCommand(FString::Printf(
			TEXT("HighResShot 1920x1080 filename=%s"), Name));
	};
	auto AimAt = [&](const FVector& TargetUE)
	{
		const APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			PC->SetControlRotation((TargetUE - Pawn->GetActorLocation()).Rotation());
		}
	};
	auto AimTroops = [&]()
	{
		const shitfire::scenario::ScenarioState& S = *Sim->parts().scenario;
		// aim at the live troop centroid (dispersal moves them off S.enemy)
		double CX = S.enemy.x, CZ = S.enemy.z;
		int32 N = 0;
		double AX = 0, AZ = 0;
		for (const FFigure& F : Troops)
		{
			if (F.bVisible) { AX += F.SimX; AZ += F.SimZ; N++; }
		}
		if (N > 0) { CX = AX / N; CZ = AZ / N; }
		AimAt(ShitfireCoords::SimToUE(CX, GroundH(CX, CZ), CZ));
	};

	switch (ProbeStage)
	{
	case 0: AimTroops(); SetVFov(8.6); break;   // settle a frame before shooting
	case 1: Shot(TEXT("UnitsProbe_standing_7x")); break;
	case 2: SetVFov(4.3); AimTroops(); break;
	case 3: Shot(TEXT("UnitsProbe_standing_14x")); break;
	case 4: OnDisperseTroops(); SetVFov(8.6); break;
	case 5: Shot(TEXT("UnitsProbe_dispersing_7x")); break;   // ~mid/settled scatter
	case 6: AimTroops(); break;                              // settle on prone
	case 7: Shot(TEXT("UnitsProbe_prone_7x")); break;
	case 8: SetVFov(4.3); AimTroops(); break;
	case 9: Shot(TEXT("UnitsProbe_prone_14x")); break;
	case 10: OnTroopsDown(false); SetVFov(8.6); AimTroops(); break;
	case 11: Shot(TEXT("UnitsProbe_dead_7x")); break;
	case 12: SetVFov(4.3); break;
	case 13: Shot(TEXT("UnitsProbe_dead_14x")); break;
	case 14:
		// forced convoy demo on the real road polyline near the target area
		// (presentation forcing only - the sim is untouched; labeled as such)
		ProbeConvoyT = 0;
		SetVFov(8.6);
		break;
	case 15: AimAt(ProbeAimUE); break;   // let dust build, aim at the column
	case 16: Shot(TEXT("UnitsProbe_convoy_7x")); break;
	case 17: SetVFov(4.3); AimAt(ProbeAimUE); break;
	case 18: Shot(TEXT("UnitsProbe_convoy_14x")); break;
	case 19:
	{
		// the friendly battery (Cannon emplacement)
		const shitfire::SimCore::Parts P = Sim->parts();
		SetVFov(8.6);
		AimAt(ShitfireCoords::SimToUE(P.battery->x,
			GroundH(P.battery->x, P.battery->z), P.battery->z));
		break;
	}
	case 20: Shot(TEXT("UnitsProbe_battery_7x")); break;
	default:
		PC->ConsoleCommand(TEXT("quit"));
		break;
	}
	ProbeStage++;
}

void AShitfireUnitsActor::ForceConvoyDemo(double T)
{
	// Drive the 4 vehicle slots down the first road polyline at the browser
	// convoy speed (3.5 m/s, 40 m spacing - 05:406) purely as presentation.
	const shitfire::SimCore::Parts P = Sim->parts();
	if (P.world->roads.empty()) { return; }
	const shitfire::scenario::Polyline& Road = P.world->roads[0];
	if (Road.size() < 2) { return; }

	// cumulative arc length
	TArray<double> Cum;
	Cum.Add(0);
	for (size_t i = 1; i < Road.size(); i++)
	{
		Cum.Add(Cum.Last() + FMath::Sqrt(
			FMath::Square(Road[i].x - Road[i - 1].x) +
			FMath::Square(Road[i].z - Road[i - 1].z)));
	}
	const double Len = Cum.Last();
	auto Sample = [&](double D, double& X, double& Z, double& Dx, double& Dz)
	{
		D = FMath::Clamp(D, 0.0, Len);
		int32 Seg = 1;
		while (Seg < Cum.Num() - 1 && Cum[Seg] < D) { Seg++; }
		const double T0 = Cum[Seg - 1], T1 = Cum[Seg];
		const double A = (T1 > T0) ? (D - T0) / (T1 - T0) : 0;
		X = FMath::Lerp(Road[Seg - 1].x, Road[Seg].x, A);
		Z = FMath::Lerp(Road[Seg - 1].z, Road[Seg].z, A);
		Dx = Road[Seg].x - Road[Seg - 1].x;
		Dz = Road[Seg].z - Road[Seg - 1].z;
	};

	const double Head = FMath::Min(Len * 0.35 + 3.5 * T, Len);
	double CentroidX = 0, CentroidZ = 0;
	for (int32 i = 0; i < 4; i++)
	{
		FVehicle& V = Vehicles[i];
		if (!V.Mesh) { continue; }
		if (V.Mesh->GetStaticMesh() == Assets.Cube || !V.bVisible)
		{
			// probe demo forces the convoy meshes onto every slot
			FitVehicleMesh(V, i == 0 ? Assets.HumveeMesh : Assets.TruckMesh,
				FShitfireUnitAssets::VehicleLengthM);
			V.bVisible = true;
			V.LayoutScale = FVector::OneVector;
		}
		double X, Z, Dx, Dz;
		Sample(Head - i * 40.0, X, Z, Dx, Dz);
		const double Speed = (T > 0.1) ? 3.5 : 0;
		V.SimX = X;
		V.SimZ = Z;
		CentroidX += X / 4;
		CentroidZ += Z / 4;
		const double G = GroundH(X, Z);
		V.Mesh->SetVisibility(true);
		V.Mesh->SetWorldLocation(ShitfireCoords::SimToUE(X, G, Z)
			+ FVector(0, 0, V.ZLiftUE));
		V.Mesh->SetWorldScale3D(FVector(V.FitScale));
		V.Mesh->SetWorldRotation(FRotator(0,
			FMath::RadiansToDegrees(FMath::Atan2(Dz, Dx)) + V.YawOffsetDeg, 0));
		if (Speed > DustMinSpeedMps)
		{
			V.DustCooldown -= GetWorld()->GetDeltaSeconds();
			if (V.DustCooldown <= 0)
			{
				V.DustCooldown = DustEmitPeriodS;
				SpawnDustPuff(V);
			}
		}
	}
	ProbeAimUE = ShitfireCoords::SimToUE(CentroidX,
		GroundH(CentroidX, CentroidZ), CentroidZ);
}
