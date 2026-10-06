// AObserverPawn - the P3/P4 observer: fixed on the OP tower (placement comes
// from AShitfireGameMode via AShitfireWorldActor::ObserverEyeUE(), which is
// core eyeFor = OP + towerHeight 300 + eyeHeight 2.2 - 03-op-battery.js:63-68),
// mouse look with pitch clamp, bino optics on three powers, presentation-only
// hold sway, the [L] laser, and the mil-reticle/mil-card HUD.
//
// Port of the camera-control half of src/js/09-camera.js plus the observer
// bindings of src/js/16-input.js (oracle-v1). The browser file is cited line
// by line at each member.
//
// Doctrine invariants owned here:
//   - vertical FOV = 60/power (15.0 / 8.57 / 4.29) and 60 naked-eye; the FOV
//     VALUE is what the reticle's px-per-mil math derives from, so it snaps
//     exactly like the browser (09-camera.js:136) - no eased transitions.
//   - the lase ray is CAMERA transform -> ShitfireCoords -> core groundHit.
//     NEVER a UE line trace (terrain has no collision, on purpose, forever -
//     see AShitfireWorldActor's header).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "observer/ObserverConfig.h"
#include "ObserverPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UObserverHudWidget;
class AShitfireWorldActor;
class UShitfireSimHost;
struct FInputActionValue;
namespace shitfire { class HeightField; }

/** One resolved lase - everything the readout and the gate test need. */
struct FObserverLaseHit
{
	bool bHit = false;
	double DistM = 0.0;          // core groundHit dist (meters)
	double SimX = 0.0, SimY = 0.0, SimZ = 0.0;
	FString Grid;                // core gridOf 6-digit string
	int32 OtFactor = 0;          // core sim::otFactor(dist)
};

UCLASS()
class SHITFIREGAME_API AObserverPawn : public APawn
{
	GENERATED_BODY()

public:
	AObserverPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---- optics (09-camera.js:133-175, 01-math-terrain.js:169-170) --------
	bool BinosUp() const { return bBinos; }
	int32 BinoPower() const;             // binoZooms[binoZoom] (01:169)
	double VerticalFovDeg() const;       // binos ? 60/power : 60 (01:170, 09:136)
	void SetBinos(bool bOn);             // 09:133 setBinos
	void SetBinoZoom(int32 Index);       // 09:151 setBinoZoom (wraps; pre-selects with binos down)
	void CycleBinoZoom(int32 Dir);       // 09:161 cycleBinoZoom (guards !binos)

	// ---- laser (09-camera.js:192-223) -------------------------------------
	void DoLase();

	/** The full lase path, synchronously: camera world transform -> coords
	    shim -> core groundHit -> grid/OT. Public so the LaseGate automation
	    test can run the EXACT production ray construction against the
	    raylos.json fixture. Presentation timing (0.55 s delay, readout hold)
	    lives in DoLase/Tick, not here. */
	FObserverLaseHit ComputeLase(const shitfire::HeightField& Hf, double MaxDistM) const;

	// ---- mil card (09-camera.js:182-191) ----------------------------------
	void ToggleMilCard();                // [R] - guards !binos like the browser
	void ForceMilCard(bool bOn);         // toggleMilCard(force) - the LRF-fault path

	/** BUILDER-UI HOOK: [ENTER] focuses the radio text box (16-input.js:181).
	    The box does not exist yet; the binding is live and broadcasts here.
	    BUILDER-UI: bind your comms/text widget's focus to this delegate -
	    nothing else about the pawn needs to change. */
	FSimpleMulticastDelegate OnFocusTextRequested;

	/** Presentation sway amplitude in mils (CONFIG.CAMERA.swayMil = 0.65,
	    00-config.js:39). 0 disables - the LaseGate test zeroes it so fixture
	    rays are exact; in play it rides the camera only (browser 09:34:
	    "yaw/pitch state is untouched - sway is presentation"). */
	UPROPERTY(EditAnywhere, Category = "Shitfire")
	double SwayMilAmplitude = ShitfireObserver::SwayMilDefault;

private:
	// input handlers (16-input.js keydown/wheel; Enhanced Input, C++-registered
	// per Config/DefaultInput.ini's stated plan - no input .uassets)
	void OnLook(const FInputActionValue& Value);      // 09:21-26 mousemove
	void OnBinosPressed();                            // 16:147 [B]
	void OnPowerPressed();                            // 16:148 [Z] (+shift = reverse)
	void OnWheel(const FInputActionValue& Value);     // 16:211-215 wheel while glassing
	void OnLasePressed();                             // 16:156 [L]
	void OnMilCardPressed();                          // 16:154 [R]
	void OnFocusPressed();                            // 16:181 [ENTER]

	// sim-frame view state <-> UE control rotation (the ONE source of truth,
	// so the GameMode bench sweep's SetControlRotation keeps working)
	void GetSimYawPitch(double& OutYaw, double& OutPitch) const;
	void SetSimYawPitch(double Yaw, double Pitch);

	void ApplyVerticalFov();             // exact vertical fov via MaintainYFOV
	void ResolveLase();                  // the scheduled half of doLase (09:208-222)
	void UpdateHeading();                // 17-loop-boot.js:121-130 MAG readout
	void Hint(const FString& Msg);       // stand-in for the browser's log(...,'sys')

	AShitfireWorldActor* FindWorldActor();          // lazy - see .cpp
	const shitfire::HeightField* Heightfield();

	/** The lase scheduler's clock: SIM seconds from UShitfireSimHost. The
	    browser schedules both halves of doLase on sim.now (09-camera.js:208
	    +0.55, :221 +CONFIG.UI.laseHold; schedule() is the sim-clock event
	    queue, 06-clock-comms.js:4-12), so the delay and the hold scale with
	    TimeScale and freeze with a paused sim. World seconds only when no
	    host exists at all. */
	double LaseClockNow();

	// -ShitfireObserverDemo: scripted PIE sanity pass (binos, 3 powers,
	// 3 lases, screenshots) so the observer is inspectable headlessly.
	void TickDemo(double Now);

	UPROPERTY(VisibleAnywhere, Category = "Shitfire")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UObserverHudWidget> Hud;

	UPROPERTY()
	TObjectPtr<AShitfireWorldActor> WorldActor;

	UPROPERTY()
	TObjectPtr<UShitfireSimHost> SimHost;        // lazy - LaseClockNow()

	// runtime-constructed Enhanced Input objects
	UPROPERTY() TObjectPtr<UInputMappingContext> Imc;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> BinosAction;
	UPROPERTY() TObjectPtr<UInputAction> PowerAction;
	UPROPERTY() TObjectPtr<UInputAction> WheelAction;
	UPROPERTY() TObjectPtr<UInputAction> LaseAction;
	UPROPERTY() TObjectPtr<UInputAction> MilCardAction;
	UPROPERTY() TObjectPtr<UInputAction> FocusAction;

	// optics state (09-camera.js:2, CONFIG.CAMERA.binoZoom 00-config.js:36)
	bool bBinos = false;
	int32 BinoZoomIdx = ShitfireObserver::DefaultBinoZoomIdx;

	// 13i hold-sway state (09-camera.js:35)
	double SwayT0 = -9e9;                // raise transient start
	double SwaySteadyUntil = -9e9;       // the lase squeeze

	// doLase state (09-camera.js:178,202-221)
	int32 LaseToken = 0;
	int32 PendingLaseToken = -1;
	double LaseFireAt = -1.0;            // SIM seconds (LaseClockNow); <0 = idle
	int32 ClearLaseToken = -1;
	double LaseClearAt = -1.0;           // SIM seconds (LaseClockNow); <0 = idle

	bool bNoLaserChapter = false;        // activeChapter.noLaser (09:196)

	// heading readout change-gate (17-loop-boot.js:125 lastHdg)
	double LastHeadingMils = -1.0;

	// demo driver
	bool bDemo = false;
	double DemoT0 = -1.0;
	int32 DemoStage = 0;
};
