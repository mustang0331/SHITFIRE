#include "ShitfireWorldActor.h"
#include "ShitfireGame.h"
#include "ShitfireFixtures.h"
#include "ShitfireVis.h"
#include "bridge/ShitfireCoords.h"
#include "bridge/ShitfireSimHost.h"
#include "units/ShitfireUnitsActor.h"

#include "RealtimeMeshProcedural.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

// Engine-free sim core - every call below executes prebuilt /fp:precise code.
#include "shitfire/config/sim_config.h"
#include "shitfire/scenario/place_units.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_config.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim_core.h"
#include "shitfire/terrain/heightfield.h"
#include "shitfire/terrain/raymarch.h"

namespace
{
	// Day TOD lighting constants - port of src/js/02-scene.js:60 ('day' row)
	// and todDir() (02-scene.js:95): direction TO the sun =
	// (cos(el)*sin(az), sin(el), cos(el)*cos(az)) in sim axes (x-east, y-up,
	// z-south). Day only per the P3 ruling; the other TOD rows come with P4+.
	constexpr double SunElevDeg = 64.5;
	constexpr double SunAziDeg = 324.0;
	const FColor SunColorSRGB(0xFF, 0xF4, 0xE0);

	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* ConePath = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* PlanePath = TEXT("/Engine/BasicShapes/Plane.Plane");

	// Real vegetation meshes (BUILDER-ASSETS P3, Tools/import_assets.py +
	// asset_manifest.json - CC0, see port/CREDITS.md). SpawnVegetation falls
	// back to the original engine-cone placeholders when these are absent.
	const TCHAR* VegCanopyPath = TEXT("/Game/Vegetation/SM_PalmTreeA.SM_PalmTreeA");
	const TCHAR* VegScatterPath = TEXT("/Game/Vegetation/SM_DesertBush.SM_DesertBush");

	// sim yaw (radians around up, east->north positive; three.js rotation.y)
	// -> UE yaw degrees (east->south positive): sign flip.
	float SimRotToUEYaw(double RotY)
	{
		return static_cast<float>(-FMath::RadiansToDegrees(RotY));
	}
}

AShitfireWorldActor::AShitfireWorldActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AShitfireWorldActor::BeginPlay()
{
	Super::BeginPlay();

	const double T0 = FPlatformTime::Seconds();
	InitSimCore();
	if (!Sim)
	{
		return;   // no subsystem - already logged, nothing to assemble from
	}

	PlaceholderBaseMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Materials/M_Placeholder.M_Placeholder"));
	if (!PlaceholderBaseMat)
	{
		UE_LOG(LogShitfire, Warning,
			TEXT("M_Placeholder missing - run Tools/gen_assets.py; using default material"));
	}

	URealtimeMeshProcedural* Mesh =
		GetRealtimeMeshComponent()->InitializeRealtimeMesh<URealtimeMeshProcedural>();
	// The RMC never cooks collision (bCreateCollision=false on every section):
	// the sim ray-march is the only geometry authority (see class header).
	GetRealtimeMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The RMC component stays Movable: setting it Static after registration
	// stops its runtime-created sections from rendering at all (observed on
	// 5.8 + RMC v5.4). It never moves, so VSM still caches its pages. The
	// ISM/HISM/ocean components ARE Static - they get their mesh before
	// registration.

	BuildTerrain(Mesh);

	{
		// Road/path ribbons from the ported WorldLayout polylines.
		const shitfire::SimCore::Parts P = Sim->parts();
		const shitfire::scenario::WorldConfig W{};
		auto ToXZ = [](const shitfire::scenario::Polyline& Line)
		{
			TArray<FVector2D> Out;
			Out.Reserve(Line.size());
			for (const shitfire::WorldXZ& Pt : Line)
			{
				Out.Add(FVector2D(Pt.x, Pt.z));
			}
			return Out;
		};
		TArray<TArray<FVector2D>> Roads, Paths;
		for (const auto& L : P.world->roads) { Roads.Add(ToXZ(L)); }
		for (const auto& L : P.world->paths) { Paths.Add(ToXZ(L)); }
		// Signatures: roads are the browser's sealed `asph` (04-world.js:148),
		// paths its bare-soil `path` (:150) - one material, two instances.
		BuildRibbonSection(Mesh, 1, Roads, W.roadWidth, 0.18, ShitfireVis::Asphalt);
		BuildRibbonSection(Mesh, 2, Paths, W.pathWidth, 0.12, ShitfireVis::Path);
	}

	SpawnOcean();
	SpawnSkyAndLight();
	SpawnPlaceholders();
	SpawnVegetation();

#if !UE_BUILD_SHIPPING
	RunGoldenRaySelfTest();
#endif

	// ART-a gate, runtime half: every slot on every spawned primitive (this
	// actor, the units actor it spawned, their attachments) carries the
	// optic-signature parameters. An Error per offender, never a fallback.
	ShitfireVis::AuditWorld(GetWorld());
	ShitfireVis::ApplyPreviewMode(GetWorld());   // honours -ExecCmds="shitfire.VisMode n"

	UE_LOG(LogShitfire, Log, TEXT("world assembly took %.2f s"),
		FPlatformTime::Seconds() - T0);
}

void AShitfireWorldActor::InitSimCore()
{
	// BUILDER-BRIDGE refactor (was: this actor new'd its own SimCore +
	// campaign seed lookup, 587e630). The GameInstance subsystem owns the one
	// SimCore now; the seed resolution moved verbatim into
	// UShitfireSimHost::LaunchChapter. BeginPlay = fresh mission, as before.
	Sim = nullptr;
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UShitfireSimHost* Host = GI ? GI->GetSubsystem<UShitfireSimHost>() : nullptr;
	if (!Host)
	{
		UE_LOG(LogShitfire, Error,
			TEXT("no UShitfireSimHost subsystem (no GameInstance?) - world assembly aborted"));
		return;
	}
	Host->LaunchChapter(ChapterId, TerrainSeed);
	Sim = Host->GetSimCore();
}

double AShitfireWorldActor::GroundH(double X, double Z) const
{
	return Sim->parts().heightfield->H(X, Z);
}

FVector AShitfireWorldActor::ObserverEyeUE() const
{
	const shitfire::SimCore::Parts P = Sim->parts();
	const shitfire::scenario::EyePos Eye =
		shitfire::scenario::eyeFor(*P.op, shitfire::scenario::CameraConstants{});
	return ShitfireCoords::SimToUE(Eye.x, Eye.y, Eye.z);
}

FVector AShitfireWorldActor::TargetAreaUE() const
{
	const shitfire::SimCore::Parts P = Sim->parts();
	const double X = P.scenario->enemy.x;
	const double Z = P.scenario->enemy.z;
	return ShitfireCoords::SimToUE(X, GroundH(X, Z), Z);
}

void AShitfireWorldActor::BuildTerrain(URealtimeMeshProcedural* Mesh)
{
	// Base sheet: meshSegments (300) quads over the 10 km map, exactly the
	// browser's base-sheet density (CONFIG.TERRAIN.meshSegments). Vertex
	// heights AND normals come from the core H(x,z); the visual mesh is a
	// pure view of the sim heightfield ("one H, three consumers").
	const shitfire::TerrainConfig TC{};
	const shitfire::MapConfig MC{};
	const int32 Seg = static_cast<int32>(TC.meshSegments);
	const double Size = MC.size;
	const double Half = Size * 0.5;
	const double Step = Size / Seg;
	const int32 NV = Seg + 1;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FRealtimeMeshProceduralTangent> Tangents;   // empty = default tangents
	Verts.Reserve(NV * NV);
	Normals.Reserve(NV * NV);
	UVs.Reserve(NV * NV);
	Colors.Init(FColor::White, NV * NV);
	Tris.Reserve(Seg * Seg * 6);

	for (int32 j = 0; j < NV; j++)          // j walks south (+z)
	{
		const double Z = -Half + j * Step;
		for (int32 i = 0; i < NV; i++)      // i walks east (+x)
		{
			const double X = -Half + i * Step;
			const double H = GroundH(X, Z);
			Verts.Add(ShitfireCoords::SimToUE(X, H, Z));

			// Central-difference gradient of H -> vertex normal.
			const double DHdX = (GroundH(X + Step, Z) - GroundH(X - Step, Z)) / (2.0 * Step);
			const double DHdZ = (GroundH(X, Z + Step) - GroundH(X, Z - Step)) / (2.0 * Step);
			FVector N = ShitfireCoords::SimDirToUE(-DHdX, 1.0, -DHdZ);
			N.Normalize();
			Normals.Add(N);

			UVs.Add(FVector2D(double(i) / Seg, double(j) / Seg));
		}
	}

	for (int32 j = 0; j < Seg; j++)
	{
		for (int32 i = 0; i < Seg; i++)
		{
			const int32 V00 = j * NV + i;
			const int32 V10 = V00 + 1;        // east
			const int32 V01 = V00 + NV;       // south
			const int32 V11 = V01 + 1;
			// Up-facing winding in UE's left-handed frame (derived from the
			// RMC box generator's +Z face: front faces wind clockwise seen
			// from the normal side).
			Tris.Add(V00); Tris.Add(V01); Tris.Add(V10);
			Tris.Add(V10); Tris.Add(V01); Tris.Add(V11);
		}
	}

	Mesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents,
	                        /*bCreateCollision=*/false);

	if (UMaterialInterface* TerrainMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Materials/M_Terrain.M_Terrain")))
	{
		GetRealtimeMeshComponent()->SetMaterial(0,
			ShitfireVis::MakeTagged(TerrainMat, this, ShitfireVis::Terrain));
	}
	else
	{
		UE_LOG(LogShitfire, Warning, TEXT("M_Terrain missing - run Tools/gen_assets.py"));
	}

	UE_LOG(LogShitfire, Log, TEXT("terrain sheet: %d verts, %d tris, no collision"),
		Verts.Num(), Tris.Num() / 3);
}

void AShitfireWorldActor::BuildRibbonSection(URealtimeMeshProcedural* Mesh,
                                             int32 SectionIndex,
                                             const TArray<TArray<FVector2D>>& PolylinesXZ,
                                             double WidthMeters, double LiftMeters,
                                             const FShitfireVis& Vis)
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FRealtimeMeshProceduralTangent> Tangents;

	const double HalfW = WidthMeters * 0.5;
	for (const TArray<FVector2D>& Line : PolylinesXZ)
	{
		if (Line.Num() < 2)
		{
			continue;
		}
		const int32 Base = Verts.Num();
		for (int32 k = 0; k < Line.Num(); k++)
		{
			const FVector2D& P = Line[k];
			const FVector2D& Prev = Line[FMath::Max(k - 1, 0)];
			const FVector2D& Next = Line[FMath::Min(k + 1, Line.Num() - 1)];
			FVector2D Dir = Next - Prev;
			if (!Dir.Normalize())
			{
				Dir = FVector2D(1, 0);
			}
			const FVector2D Perp(-Dir.Y, Dir.X);   // sideways in the x/z plane
			const FVector2D L = P + Perp * HalfW;
			const FVector2D R = P - Perp * HalfW;
			// Drape on the core heightfield, lifted slightly above the sheet.
			Verts.Add(ShitfireCoords::SimToUE(L.X, GroundH(L.X, L.Y) + LiftMeters, L.Y));
			Verts.Add(ShitfireCoords::SimToUE(R.X, GroundH(R.X, R.Y) + LiftMeters, R.Y));
			Normals.Add(FVector::UpVector);
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(0.0, k * 0.25));
			UVs.Add(FVector2D(1.0, k * 0.25));
		}
		for (int32 k = 0; k + 1 < Line.Num(); k++)
		{
			const int32 A = Base + k * 2;       // left k
			const int32 B = A + 1;              // right k
			const int32 C = A + 2;              // left k+1
			const int32 D = A + 3;              // right k+1
			// M_Road is two-sided, so winding only affects normal-facing.
			Tris.Add(A); Tris.Add(C); Tris.Add(B);
			Tris.Add(B); Tris.Add(C); Tris.Add(D);
		}
	}
	Colors.Init(FColor::White, Verts.Num());

	if (Verts.Num() == 0)
	{
		return;
	}
	Mesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors,
	                        Tangents, /*bCreateCollision=*/false);

	if (UMaterialInterface* RoadMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Materials/M_Road.M_Road")))
	{
		GetRealtimeMeshComponent()->SetMaterial(SectionIndex,
			ShitfireVis::MakeTagged(RoadMat, this, Vis));
	}
	UE_LOG(LogShitfire, Log, TEXT("ribbon section %d: %d verts"), SectionIndex, Verts.Num());
}

void AShitfireWorldActor::SpawnOcean()
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlanePath);
	if (!Plane)
	{
		UE_LOG(LogShitfire, Error, TEXT("engine Plane mesh missing"));
		return;
	}
	OceanPlane = NewObject<UStaticMeshComponent>(this, TEXT("OceanPlane"));
	OceanPlane->SetStaticMesh(Plane);
	OceanPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OceanPlane->SetMobility(EComponentMobility::Static);
	// Z derives from the sim's sea-level contract IN CODE (ShitfireCoords.h),
	// never eyeballed. 100 cm plane scaled to a 22 km sheet.
	OceanPlane->SetWorldLocation(FVector(0, 0,
		ShitfireCoords::SeaLevelMeters * ShitfireCoords::MetersToUE));
	OceanPlane->SetWorldScale3D(FVector(22000.0, 22000.0, 1.0));
	if (UMaterialInterface* OceanMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Materials/M_Ocean.M_Ocean")))
	{
		OceanPlane->SetMaterial(0, ShitfireVis::MakeTagged(OceanMat, this, ShitfireVis::Ocean));
	}
	OceanPlane->RegisterComponent();
	OceanPlane->AttachToComponent(GetRootComponent(),
		FAttachmentTransformRules::KeepWorldTransform);
}

void AShitfireWorldActor::SpawnSkyAndLight()
{
	UWorld* World = GetWorld();

	// Sun: day TOD row, exact browser convention (see namespace constants).
	const double El = FMath::DegreesToRadians(SunElevDeg);
	const double Az = FMath::DegreesToRadians(SunAziDeg);
	const FVector ToSunSim(FMath::Cos(El) * FMath::Sin(Az),   // x east
	                       FMath::Sin(El),                    // y up
	                       FMath::Cos(El) * FMath::Cos(Az));  // z south
	const FVector TravelUE = -ShitfireCoords::SimDirToUE(ToSunSim.X, ToSunSim.Y, ToSunSim.Z);

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>();
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetActorRotation(FRotationMatrix::MakeFromX(TravelUE.GetSafeNormal()).Rotator());
	if (UDirectionalLightComponent* DL = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		// Physical daylight illuminance (mid-day sun ~75k lux) paired with
		// the pawn camera's fixed manual exposure (ISO 100, f/8, 1/125 s,
		// EV100 ~ 13) - correct by arithmetic, no auto-exposure anywhere.
		DL->SetIntensity(75000.0f);
		DL->SetLightColor(FLinearColor::FromSRGBColor(SunColorSRGB));
		DL->SetAtmosphereSunLight(true);
	}

	World->SpawnActor<ASkyAtmosphere>();

	ASkyLight* Sky = World->SpawnActor<ASkyLight>();
	if (USkyLightComponent* SL = Sky->GetLightComponent())
	{
		SL->SetMobility(EComponentMobility::Movable);
		SL->SetIntensity(1.0f);
		SL->SetRealTimeCaptureEnabled(true);   // tracks the atmosphere; cost measured in the P3 perf snapshot
	}

	AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>();
	if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
	{
		// Browser fog is a light haze from 4500 m (02-scene.js:16); this
		// density reads similar at 10 km. Volumetric ON per the P3 ruling.
		FC->SetFogDensity(0.006f);
		FC->SetVolumetricFog(true);
	}
}

UMaterialInstanceDynamic* AShitfireWorldActor::MakePlaceholderMID(const FLinearColor& Color,
                                                                  const FShitfireVis& Vis)
{
	UMaterialInstanceDynamic* MID = ShitfireVis::MakeTagged(PlaceholderBaseMat, this, Vis);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return MID;
}

UInstancedStaticMeshComponent* AShitfireWorldActor::MakeInstancedClass(
	const TCHAR* Name, const TCHAR* MeshPath, const FLinearColor& Color,
	const FShitfireVis& Vis)
{
	UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!MeshAsset)
	{
		UE_LOG(LogShitfire, Error, TEXT("engine mesh missing: %s"), MeshPath);
		return nullptr;
	}
	UInstancedStaticMeshComponent* ISM =
		NewObject<UInstancedStaticMeshComponent>(this, Name);
	ISM->SetStaticMesh(MeshAsset);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetMobility(EComponentMobility::Static);
	if (UMaterialInstanceDynamic* MID = MakePlaceholderMID(Color, Vis))
	{
		ISM->SetMaterial(0, MID);
	}
	ISM->RegisterComponent();
	ISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	InstancedClasses.Add(ISM);
	return ISM;
}

void AShitfireWorldActor::SpawnPlaceholders()
{
	// HONEST PLACEHOLDERS: colored boxes at the ported positions so scale and
	// placement are inspectable. BUILDER-CHARACTERS replaces every one.
	const shitfire::SimCore::Parts P = Sim->parts();

	auto BoxAt = [this](UInstancedStaticMeshComponent* ISM, double X, double Z,
	                    double WMeters, double DMeters, double HMeters,
	                    double YawDeg = 0.0, double GroundLift = 0.0)
	{
		if (!ISM) { return; }
		const double G = GroundH(X, Z) + GroundLift;
		const FVector Loc = ShitfireCoords::SimToUE(X, G + HMeters * 0.5, Z);
		// Engine cube is 100 cm: scale N = N meters. Sim W is x-extent (east),
		// D is z-extent (south) -> UE X / Y.
		const FVector Scale(WMeters, DMeters, HMeters);
		ISM->AddInstance(FTransform(FRotator(0, YawDeg, 0), Loc, Scale), true);
	};

	// -- world features ----------------------------------------------------
	// One ISM class serves village huts AND town buildings, so it takes the
	// hut signature (04-world.js:128); the town's masonry/roof split
	// (TownMasonry/TownRoof) waits for the ART-d building kit.
	UInstancedStaticMeshComponent* Huts =
		MakeInstancedClass(TEXT("PH_Huts"), CubePath, FLinearColor(0.35f, 0.22f, 0.12f),
			ShitfireVis::HutCiv);
	for (const auto& V : P.world->villages)
	{
		for (const auto& Hut : V.huts)
		{
			if (Hut.w.has_value() && Hut.d.has_value())
			{
				BoxAt(Huts, Hut.x, Hut.z, *Hut.w, *Hut.d, 6.0);   // town building
			}
			else
			{
				BoxAt(Huts, Hut.x, Hut.z, 5.2, 4.2, 3.0);          // village hut (04-world.js:624)
			}
		}
	}

	// Facilities/mast are untagged in the browser (_wMats.mil/strip,
	// 04-world.js:22-24): they take the same luminance-derived fallback.
	const FLinearColor ColFacility(0.45f, 0.45f, 0.48f);
	const FLinearColor ColMast(0.7f, 0.7f, 0.75f);
	UInstancedStaticMeshComponent* Facilities =
		MakeInstancedClass(TEXT("PH_Facilities"), CubePath, ColFacility,
			ShitfireVis::Derived(ColFacility));
	UInstancedStaticMeshComponent* Mast =
		MakeInstancedClass(TEXT("PH_Mast"), CylinderPath, ColMast,
			ShitfireVis::Derived(ColMast));
	for (const auto& F : P.world->facilities)
	{
		if (F.kind == "mast")
		{
			if (Mast)
			{
				const double G = GroundH(F.x, F.z);
				Mast->AddInstance(FTransform(FRotator::ZeroRotator,
					ShitfireCoords::SimToUE(F.x, G + 20.0, F.z),
					FVector(1.5, 1.5, 40.0)), true);
			}
		}
		else if (F.kind == "airfield" && F.len.has_value())
		{
			BoxAt(Facilities, F.x, F.z, *F.len, 20.0, 0.4,
				F.th.has_value() ? SimRotToUEYaw(*F.th) : 0.0);
		}
		else
		{
			BoxAt(Facilities, F.x, F.z, 10.0, 8.0, 5.0);
		}
	}

	UInstancedStaticMeshComponent* Rocks =
		MakeInstancedClass(TEXT("PH_Rocks"), CubePath, FLinearColor(0.30f, 0.29f, 0.28f),
			ShitfireVis::Rock);
	for (const auto& R : P.world->rocks)
	{
		BoxAt(Rocks, R.x, R.z, R.r, R.r, FMath::Max(R.h, 0.5));
	}

	// -- units handoff (BUILDER-CHARACTERS, P4) ----------------------------
	// The P3 unit-placeholder boxes (OP tower, battery guns, troops,
	// vehicles, squad, flag, scenario bunker, civilians) moved to
	// AShitfireUnitsActor: skeletal soldiers with pose states, real vehicle
	// meshes on the core convoy positions, the modeled watchtower and Cannon
	// battery, per-tick updates and presentation-cue reactions - none of
	// which fits a static one-shot ISM install. World FEATURES (village
	// huts, facilities, rocks, mast, vegetation) stay right here.
	GetWorld()->SpawnActor<AShitfireUnitsActor>();

	int32 Total = 0;
	for (const auto& ISM : InstancedClasses) { Total += ISM->GetInstanceCount(); }
	UE_LOG(LogShitfire, Log, TEXT("placeholders: %d instances across %d classes"),
		Total, InstancedClasses.Num());
}

void AShitfireWorldActor::SpawnVegetation()
{
	// HISM vegetation straight from the ported placement stream
	// (04-world.js:1051; type 0 canopy / 1 scatter, s in [0.55, 1.0],
	// h = ground height at placement). Meshes: real vegetation from
	// /Game/Vegetation when present (BUILDER-ASSETS, Tools/import_assets.py;
	// materials with the HISM usage flag are baked into the mesh assets),
	// else the original engine-cone placeholders. Wind comes later.
	const shitfire::SimCore::Parts P = Sim->parts();

	UStaticMesh* CanopyMesh = LoadObject<UStaticMesh>(nullptr, VegCanopyPath);
	UStaticMesh* ScatterMesh = LoadObject<UStaticMesh>(nullptr, VegScatterPath);
	const bool bRealVeg = (CanopyMesh != nullptr && ScatterMesh != nullptr);
	if (!bRealVeg)
	{
		UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, ConePath);
		if (!Cone)
		{
			UE_LOG(LogShitfire, Error, TEXT("engine Cone mesh missing"));
			return;
		}
		UE_LOG(LogShitfire, Warning,
			TEXT("vegetation meshes missing - run Tools/import_assets.py; using cone placeholders"));
		CanopyMesh = Cone;
		ScatterMesh = Cone;
	}

	auto MakeHISM = [this, bRealVeg](const TCHAR* Name, UStaticMesh* Mesh,
	                                 const FLinearColor& ConeTint,
	                                 const FShitfireVis& Vis)
	{
		UHierarchicalInstancedStaticMeshComponent* H =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
		H->SetStaticMesh(Mesh);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetMobility(EComponentMobility::Static);
		if (!bRealVeg)   // real meshes carry MI_* of M_ShitfireMaster (import_assets.py)
		{
			if (UMaterialInstanceDynamic* MID = MakePlaceholderMID(ConeTint, Vis))
			{
				H->SetMaterial(0, MID);
			}
		}
		H->RegisterComponent();
		H->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		InstancedClasses.Add(H);
		return H;
	};

	UHierarchicalInstancedStaticMeshComponent* Canopy =
		MakeHISM(TEXT("PH_VegCanopy"), CanopyMesh, FLinearColor(0.10f, 0.22f, 0.08f),
			ShitfireVis::Canopy);
	UHierarchicalInstancedStaticMeshComponent* Scatter =
		MakeHISM(TEXT("PH_VegScatter"), ScatterMesh, FLinearColor(0.25f, 0.30f, 0.12f),
			ShitfireVis::Scatter);

	// Real meshes: fit by bounds - uniform scale to the placement height,
	// base sat on the ground (glTF pivots are not the engine cone's centered
	// unit pivot, so bake each mesh's box into the per-instance transform).
	const FBox CanopyBox = CanopyMesh->GetBoundingBox();
	const FBox ScatterBox = ScatterMesh->GetBoundingBox();

	int32 NCanopy = 0, NScatter = 0;
	for (const auto& V : P.world->veg)
	{
		const bool bCanopy = (V.type == 0);
		const double TreeH = (bCanopy ? 6.0 : 2.2) * V.s;
		const double TreeR = (bCanopy ? 3.0 : 1.6) * V.s;
		FTransform T;
		if (bRealVeg)
		{
			// TreeR is the cone-era footprint; natural aspect looks better on
			// real meshes, so only TreeH drives the (uniform) scale.
			const FBox& B = bCanopy ? CanopyBox : ScatterBox;
			const double S = TreeH * ShitfireCoords::MetersToUE /
				FMath::Max(B.GetSize().Z, 1.0);
			FVector Loc = ShitfireCoords::SimToUE(V.x, V.h, V.z);
			Loc.Z -= B.Min.Z * S;
			T = FTransform(FRotator(0, SimRotToUEYaw(V.rot), 0), Loc, FVector(S));
		}
		else
		{
			// V.h is the cached ground height at (x, z); engine cone is
			// 100 cm, pivot centered, so lift by half the height.
			const FVector Loc = ShitfireCoords::SimToUE(V.x, V.h + TreeH * 0.5, V.z);
			T = FTransform(FRotator(0, SimRotToUEYaw(V.rot), 0), Loc,
			               FVector(TreeR, TreeR, TreeH));
		}
		if (bCanopy) { Canopy->AddInstance(T, true); NCanopy++; }
		else         { Scatter->AddInstance(T, true); NScatter++; }
	}
	UE_LOG(LogShitfire, Log, TEXT("vegetation: %d canopy + %d scatter (%s)"),
		NCanopy, NScatter, bRealVeg ? TEXT("real meshes") : TEXT("cones"));
}

void AShitfireWorldActor::RunGoldenRaySelfTest()
{
	// Boot invariant (UNREAL_RESEARCH.md 4): N rays from the committed oracle
	// fixture through the LINKED core groundHit must match recorded distances.
	// Terrain has no collision, so this is the only ray answer that exists -
	// if it drifts, someone recompiled core math under engine flags.
	constexpr int32 NumRays = 25;
	constexpr double DistTolM = 0.01;

	const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/raylos.json"));
	if (!J.IsValid())
	{
		UE_LOG(LogShitfire, Error, TEXT("GOLDEN RAYS: fixture unavailable - SELF-TEST SKIPPED"));
		return;
	}

	const double MaxDist = J->GetNumberField(TEXT("maxDist"));
	const shitfire::HeightField Hf(shitfire::TerrainConfig{},
	                               J->GetNumberField(TEXT("terrainSeed")));

	const TArray<TSharedPtr<FJsonValue>>& Rays = J->GetArrayField(TEXT("rays"));
	int32 Tested = 0, Failed = 0;
	double MaxErr = 0.0;
	for (int32 i = 0; i < Rays.Num() && Tested < NumRays; i++, Tested++)
	{
		const TSharedPtr<FJsonObject> Ray = Rays[i]->AsObject();
		const auto& O = Ray->GetArrayField(TEXT("o"));
		const auto& D = Ray->GetArrayField(TEXT("d"));
		const auto Res = shitfire::groundHit(Hf,
			O[0]->AsNumber(), O[1]->AsNumber(), O[2]->AsNumber(),
			D[0]->AsNumber(), D[1]->AsNumber(), D[2]->AsNumber(), MaxDist);

		const TSharedPtr<FJsonObject>* HitObj = nullptr;
		const bool bExpectHit = Ray->TryGetObjectField(TEXT("hit"), HitObj);
		if (Res.has_value() != bExpectHit)
		{
			Failed++;
			continue;
		}
		if (Res.has_value())
		{
			const double Err = FMath::Abs(Res->dist - (*HitObj)->GetNumberField(TEXT("dist")));
			MaxErr = FMath::Max(MaxErr, Err);
			if (Err > DistTolM) { Failed++; }
		}
	}

	bGoldenRaysPassed = (Failed == 0 && Tested == NumRays);
	const FString Msg = FString::Printf(
		TEXT("GOLDEN RAYS %s: %d/%d oracle rays match (max |dist err| %.2e m, tol %.0e)"),
		bGoldenRaysPassed ? TEXT("PASS") : TEXT("FAIL"),
		Tested - Failed, Tested, MaxErr, DistTolM);
	if (bGoldenRaysPassed)
	{
		UE_LOG(LogShitfire, Display, TEXT("%s"), *Msg);
	}
	else
	{
		UE_LOG(LogShitfire, Error, TEXT("%s"), *Msg);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 30.f,
			bGoldenRaysPassed ? FColor::Green : FColor::Red, Msg);
	}
}
