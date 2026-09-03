// Observer-side constants ported from the browser's CONFIG blocks. These are
// PRESENTATION/input constants (camera feel, readout timing) - nothing here
// feeds parity math, which is why plain constexpr in engine code is fine.
// Every value cites its src/js/00-config.js line (oracle-v1).
#pragma once

#include "CoreMinimal.h"

namespace ShitfireObserver
{
	// CONFIG.CAMERA (00-config.js:22-40)
	constexpr double NakedFovDeg = 60.0;      // :22 fov - VERTICAL, three.js convention
	constexpr int32 BinoZooms[3] = { 4, 7, 14 }; // :36 binoZooms
	constexpr int32 DefaultBinoZoomIdx = 1;   // :36 binoZoom (7X, "the M22 the doctrine assumes")
	constexpr double SwayMilDefault = 0.65;   // :39 swayMil - hand-held sway amplitude, mils
	constexpr double MouseSens = 0.0022;      // :40 mouseSens - radians per mouse count
	constexpr double PitchClampRad = 1.35;    // :40 pitchClamp - radians

	// CONFIG.NAV (00-config.js:162) - "7 in every chapter"
	constexpr double DeclEastDeg = 7.0;

	// CONFIG.UI (00-config.js:194) - seconds the lase readout stays up
	constexpr double LaseHoldSec = 6.0;

	// doLase internals (09-camera.js:205-221)
	constexpr double LaseSteadySec = 1.4;     // :205 - "the squeeze" sway damp window
	constexpr double LaseDelaySec = 0.55;     // :208 - trigger-pull to return
	constexpr double LaseMaxDistM = 14000.0;  // :211 - groundHit maxD

	// drawReticle's mils-per-degree literal (09-camera.js:60 "17.7778"). The
	// browser uses this ROUNDED literal, not 6400/360 - ported verbatim so
	// px-per-mil agrees digit-for-digit.
	constexpr double MilsPerDegLiteral = 17.7778;
}
