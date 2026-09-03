// ShitfireCoords.h - THE one place sim-world coordinates meet UE coordinates.
//
//   sim core (port/core, shitfire/math/grid.h):  meters,  x = east,
//       z = south (north is -z), y = up, island centred on origin.
//   UE:                                          centimeters, X = east,
//       Y = south, Z = up (left-handed Z-up; east x south = up matches).
//
// The x100 meters->centimeters factor lives ONLY in this header. Nothing
// else in ShitfireGame may multiply by 100. Directions/normals convert with
// the same axis shuffle and NO scale.
//
// Sea level: the sim has no named sea-level constant - the ocean surface is
// y = 0 by contract (groundHit treats Math.max(H, 0) as the hittable
// surface, port/core/include/shitfire/terrain/raymarch.h). SeaLevelMeters
// restates that contract so engine code derives the ocean plane's Z from
// code, never by eye.
//
// These are pure unit/axis conversions (engine-side presentation math, no
// sim semantics), so inline here is fine - nothing here feeds parity.
#pragma once

#include "CoreMinimal.h"

namespace ShitfireCoords
{
	constexpr double MetersToUE = 100.0;   // cm per meter - lives ONLY here
	constexpr double SeaLevelMeters = 0.0; // raymarch.h ocean-surface contract

	/** Sim-world point (meters, x-east / y-up / z-south) -> UE (cm). */
	FORCEINLINE FVector SimToUE(double X, double Y, double Z)
	{
		return FVector(X * MetersToUE, Z * MetersToUE, Y * MetersToUE);
	}

	/** Sim-world direction/normal -> UE. Axis shuffle only, no scale. */
	FORCEINLINE FVector SimDirToUE(double DX, double DY, double DZ)
	{
		return FVector(DX, DZ, DY);
	}

	/** UE (cm) -> sim-world meters (x-east / y-up / z-south). */
	FORCEINLINE void UEToSim(const FVector& P, double& OutX, double& OutY, double& OutZ)
	{
		OutX = P.X / MetersToUE;
		OutY = P.Z / MetersToUE;
		OutZ = P.Y / MetersToUE;
	}
}
