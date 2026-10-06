#include "observer/ObserverPawn.h"
#include "ShitfireGame.h"
#include "ShitfireWorldActor.h"
#include "bridge/ShitfireCoords.h"
#include "bridge/ShitfireSimHost.h"
#include "ui/ObserverHudWidget.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Blueprint/UserWidget.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/config/sim_config.h"
#include "shitfire/doctrine/fdc_rules.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/sim/spotburst_math.h"
#include "shitfire/sim_core.h"
#include "shitfire/terrain/heightfield.h"
#include "shitfire/terrain/raymarch.h"

namespace Cfg = ShitfireObserver;

AObserverPawn::AObserverPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	// The pawn drives the camera itself each Tick (control rotation + sway),
	// because bUsePawnControlRotation would stomp the sway offsets.
	Camera->bUsePawnControlRotation = false;

	// Vertical-FOV doctrine: UE's FieldOfView is HORIZONTAL; under
	// AspectRatio_MaintainYFOV the engine derives the vertical half-angle as
	// atan(tan(HalfXFOV)/AspectRatio) (CameraStackTypes.cpp:326-333, UE 5.8,
	// r.UseLegacyMaintainYFOVViewMatrix default false). ApplyVerticalFov()
	// feeds it the inverse so the on-screen vertical FOV equals the doctrine
	// value (60/power) exactly, at any window size.
	Camera->bConstrainAspectRatio = false;
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);

	// Exposure block replicated verbatim from the world agent's placeholder
	// pawn (ShitfireObserverPawn.cpp:20-29): manual metering, ISO 100, f/8,
	// 1/125 s = EV100 ~13 for the 75k-lux sun. Render doctrine, not mine to
	// change.
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 0.0f;
	Camera->PostProcessSettings.bOverride_CameraISO = true;
	Camera->PostProcessSettings.CameraISO = 100.0f;
	Camera->PostProcessSettings.bOverride_CameraShutterSpeed = true;
	Camera->PostProcessSettings.CameraShutterSpeed = 125.0f;
	Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
	Camera->PostProcessSettings.DepthOfFieldFstop = 8.0f;

	ApplyVerticalFov();
}

// ---------------------------------------------------------------- optics ----

int32 AObserverPawn::BinoPower() const
{
	// binoPower() = CONFIG.CAMERA.binoZooms[CONFIG.CAMERA.binoZoom]
	// (01-math-terrain.js:169)
	return Cfg::BinoZooms[BinoZoomIdx];
}

double AObserverPawn::VerticalFovDeg() const
{
	// binoFovNow() = CONFIG.CAMERA.fov / binoPower() (01-math-terrain.js:170);
	// naked eye = CONFIG.CAMERA.fov (09-camera.js:136). 60/4=15.0,
	// 60/7=8.571..., 60/14=4.2857... - the doctrine FOV values.
	return bBinos ? Cfg::NakedFovDeg / double(BinoPower()) : Cfg::NakedFovDeg;
}

void AObserverPawn::ApplyVerticalFov()
{
	const double VHalf = FMath::DegreesToRadians(VerticalFovDeg()) * 0.5;
	const double HHalf = FMath::Atan(FMath::Tan(VHalf) * double(Camera->AspectRatio));
	// SNAP, exactly like the browser (09-camera.js:136 sets camera.fov and
	// updates the projection in the same frame). No eased transition: the
	// reticle's px-per-mil derives from this value and must never disagree
	// with the projection mid-blend.
	Camera->SetFieldOfView(float(FMath::RadiansToDegrees(HHalf * 2.0)));
}

void AObserverPawn::SetBinos(bool bOn)
{
	// setBinos (09-camera.js:133-147). tutEvent('binos') is the tutorial
	// system (not P3 observer scope); applyQuality is the browser's perf
	// tier ladder, superseded by UE scalability.
	bBinos = bOn;
	if (bOn)
	{
		SwayT0 = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;   // 13i raise transient (:135)
	}
	ApplyVerticalFov();                                             // :136-137
	if (Hud)
	{
		Hud->SetBinoMode(bOn);                                      // :138-139 bino overlay/crosshair swap
		Hud->RefreshTool();                                         // :140
		if (bOn)
		{
			Hud->InvalidateReticle();                               // :141 drawReticle()
		}
		else
		{
			Hud->SetLaseText(FText::GetEmpty());                    // :142 laseEl cleared
			Hud->SetMilCardOn(false);                               // :142 milcard closed
		}
	}
}

void AObserverPawn::SetBinoZoom(int32 Index)
{
	// setBinoZoom (09-camera.js:151-160): wraps; cycling with binos down just
	// pre-selects a power.
	constexpr int32 N = UE_ARRAY_COUNT(Cfg::BinoZooms);
	BinoZoomIdx = ((Index % N) + N) % N;
	if (bBinos)
	{
		ApplyVerticalFov();
		if (Hud) { Hud->InvalidateReticle(); }
	}
	if (Hud) { Hud->RefreshTool(); }
}

void AObserverPawn::CycleBinoZoom(int32 Dir)
{
	// cycleBinoZoom (09-camera.js:161-167)
	if (!bBinos)
	{
		Hint(TEXT("Zoom requires binoculars up — press [B] first."));
		return;
	}
	SetBinoZoom(BinoZoomIdx + (Dir ? Dir : 1));
	const double Fov = VerticalFovDeg();
	Hint(FString::Printf(
		TEXT("Optics %dX — field of view %.1f° (%d mils across the vertical). Reticle graduations are true mils at every power."),
		BinoPower(), Fov, int32(shitfire::js::round(Fov * Cfg::MilsPerDegLiteral))));
}

// ----------------------------------------------------------------- laser ----

AShitfireWorldActor* AObserverPawn::FindWorldActor()
{
	// LAZY on purpose: the pawn's BeginPlay can run before the GameMode's
	// BeginPlay spawns the world actor (both are spawned pre-broadcast),
	// so an eager lookup would cache null forever - laser dead, silently.
	if (!WorldActor)
	{
		for (TActorIterator<AShitfireWorldActor> It(GetWorld()); It; ++It)
		{
			WorldActor = *It;
			// activeChapter.noLaser (09-camera.js:196) via the chapter table
			const shitfire::campaign::ChapterSpec* Ch =
				shitfire::campaign::findChapter(TCHAR_TO_UTF8(*WorldActor->ChapterId));
			bNoLaserChapter = Ch && Ch->noLaser;
			break;
		}
	}
	return WorldActor;
}

const shitfire::HeightField* AObserverPawn::Heightfield()
{
	AShitfireWorldActor* W = FindWorldActor();
	if (W && W->GetSimCore())
	{
		return W->GetSimCore()->parts().heightfield;
	}
	return nullptr;
}

double AObserverPawn::LaseClockNow()
{
	// Both scheduled halves of doLase ride schedule(sim.now + ...)
	// (09-camera.js:208 +0.55, :221 +CONFIG.UI.laseHold; schedule() is the
	// sim-clock event queue, 06-clock-comms.js:4-12), NOT wall time: at
	// TimeScale 1000 the hold is 6 SIM seconds, and a paused sim keeps the
	// readout up. GetSimNow() reads 0 with no mission live - the lase cannot
	// resolve then anyway (Heightfield() needs the live SimCore). Same lazy
	// lookup as the HUD's net layer (ObserverHudWidget.cpp:176-178).
	if (!SimHost)
	{
		if (UWorld* W = GetWorld())
		{
			if (UGameInstance* GI = W->GetGameInstance())
			{
				SimHost = GI->GetSubsystem<UShitfireSimHost>();
			}
		}
	}
	if (SimHost)
	{
		return SimHost->GetSimNow();
	}
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

FObserverLaseHit AObserverPawn::ComputeLase(const shitfire::HeightField& Hf, double MaxDistM) const
{
	// The browser's raycast half of doLase (09-camera.js:210-218):
	//   lookDir(_dir); hit = groundHit(eye, _dir, 14000)
	// Here the "lookDir" is the camera component's world transform - which
	// already carries the sway offsets Tick applied, exactly like the browser
	// where sway lives inside lookDir so view, reticle and laser agree
	// (09-camera.js:27-34). UE cm -> sim meters through the ONE coords shim.
	FObserverLaseHit Out;

	const FVector O = Camera->GetComponentLocation();
	const FVector D = Camera->GetForwardVector();

	double OX, OY, OZ;
	ShitfireCoords::UEToSim(O, OX, OY, OZ);
	// Direction: inverse axis shuffle of ShitfireCoords::SimDirToUE - no scale.
	const double DX = D.X, DY = D.Z, DZ = D.Y;

	const std::optional<shitfire::GroundHitResult> Hit =
		shitfire::groundHit(Hf, OX, OY, OZ, DX, DY, DZ, MaxDistM);
	if (Hit)
	{
		Out.bHit = true;
		Out.DistM = Hit->dist;
		Out.SimX = Hit->x; Out.SimY = Hit->y; Out.SimZ = Hit->z;
		Out.Grid = UTF8_TO_TCHAR(shitfire::gridOf(Hit->x, Hit->z, shitfire::MapConfig{}).c_str());
		Out.OtFactor = int32(shitfire::sim::otFactor(Hit->dist));
	}
	return Out;
}

void AObserverPawn::DoLase()
{
	// doLase (09-camera.js:192-223)
	const double Now = GetWorld()->GetTimeSeconds();   // world clock - the sway squeeze only

	if (!bBinos)
	{
		Hint(TEXT("Lasing requires binoculars up — press [B] first."));     // :193
		return;
	}
	FindWorldActor();   // resolves bNoLaserChapter on first use
	if (bNoLaserChapter)
	{
		// degraded-optics drill (:196-201)
		if (Hud) { Hud->SetLaseText(FText::FromString(TEXT("LRF FAULT — NO RETURN"))); }
		Hint(TEXT("LASER RANGEFINDER IS DEAD (battery). Estimate range by MIL RELATION: press [R] for the card — measure a known object in the reticle, then SIZE ÷ MILS × 1000 = range."));
		ForceMilCard(true);
		return;
	}

	++LaseToken;                                                            // :202
	// 13i - the squeeze: steady BEFORE the scheduled raycast so the ray
	// itself fires steady (:204-205)
	SwaySteadyUntil = Now + Cfg::LaseSteadySec;
	// laseTick() (:206) is the LRF click - audio is BUILDER-BRIDGE's; hook
	// point documented here.
	if (Hud) { Hud->SetLaseText(FText::FromString(TEXT("LASING…"))); }      // :207
	PendingLaseToken = LaseToken;                                           // :208 schedule(+0.55)
	LaseFireAt = LaseClockNow() + Cfg::LaseDelaySec;   // SIM clock: schedule(sim.now + 0.55)
}

void AObserverPawn::ResolveLase()
{
	// the scheduled body of doLase (09-camera.js:209-222)
	const shitfire::HeightField* Hf = Heightfield();
	if (!Hf)
	{
		UE_LOG(LogShitfire, Warning, TEXT("lase with no heightfield - world actor missing"));
		return;
	}

	const FObserverLaseHit Hit = ComputeLase(*Hf, Cfg::LaseMaxDistM);
	FString Text;
	if (Hit.bHit)
	{
		// `RNG ${Math.round(hit.dist)} M   GRID ${gridOf(...)}` + OT factor
		// (:212-218). Math.round = js::round (floor(x+0.5)).
		Text = FString::Printf(TEXT("RNG %d M   GRID %s   OT FACTOR %d"),
			int32(shitfire::js::round(Hit.DistM)), *Hit.Grid, Hit.OtFactor);
		// lastLase (:217) feeds the browser's OT-factor memory for spotting -
		// the mission-side consumer is P4 bridge scope; the readout is ours.
		UE_LOG(LogShitfire, Log, TEXT("lase: %s"), *Text);
	}
	else
	{
		Text = TEXT("NO RETURN");                                           // :214
		UE_LOG(LogShitfire, Log, TEXT("lase: NO RETURN"));
	}
	if (Hud) { Hud->SetLaseText(FText::FromString(Text)); }

	// clear after CONFIG.UI.laseHold SIM seconds, token-guarded (:221)
	ClearLaseToken = LaseToken;
	LaseClearAt = LaseClockNow() + Cfg::LaseHoldSec;
}

// -------------------------------------------------------------- mil card ----

void AObserverPawn::ToggleMilCard()
{
	// toggleMilCard() (09-camera.js:182-191): [R] with the naked eye guards
	// with a hint instead of a silent no-op (UX10a).
	const bool bOn = Hud ? !Hud->IsMilCardOn() : false;
	if (bOn && !bBinos)
	{
		Hint(TEXT("The mil-relation card is a reticle aid — press [B] to raise the binos first."));
		return;
	}
	if (Hud) { Hud->SetMilCardOn(bOn && bBinos); }
}

void AObserverPawn::ForceMilCard(bool bOn)
{
	// toggleMilCard(force) (09-camera.js:183): the forced path still requires
	// binos for ON (:186-190) - callers on the LRF-fault path have them up.
	if (bOn && !bBinos) { return; }
	if (Hud) { Hud->SetMilCardOn(bOn); }
}

// ------------------------------------------------------- view state plumbing ----

void AObserverPawn::GetSimYawPitch(double& OutYaw, double& OutPitch) const
{
	// Sim frame (09-camera.js:46-47): yaw 0 = grid north (-z sim = -Y UE),
	// positive east; pitch positive up. UE yaw 0 = +X east, so
	// simYaw = UEYaw + 90deg (checked: simYaw 0 -> dir (0,-1,0) UE -> UE yaw -90).
	const FRotator Rot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	OutYaw = FMath::DegreesToRadians(FRotator::NormalizeAxis(Rot.Yaw) + 90.0);
	OutPitch = FMath::DegreesToRadians(FRotator::NormalizeAxis(Rot.Pitch));
}

void AObserverPawn::SetSimYawPitch(double Yaw, double Pitch)
{
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(
			FMath::RadiansToDegrees(Pitch), FMath::RadiansToDegrees(Yaw) - 90.0, 0.0));
	}
}

// ----------------------------------------------------------------- input ----

void AObserverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// C++-registered Enhanced Input (Config/DefaultInput.ini: "will be
	// C++-registered, not asset-authored") - no binary input assets exist.
	Imc = NewObject<UInputMappingContext>(this, TEXT("IMC_Observer"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) {
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	LookAction    = MakeAction(TEXT("IA_Look"),    EInputActionValueType::Axis2D);
	BinosAction   = MakeAction(TEXT("IA_Binos"),   EInputActionValueType::Boolean);
	PowerAction   = MakeAction(TEXT("IA_Power"),   EInputActionValueType::Boolean);
	WheelAction   = MakeAction(TEXT("IA_Wheel"),   EInputActionValueType::Axis1D);
	LaseAction    = MakeAction(TEXT("IA_Lase"),    EInputActionValueType::Boolean);
	MilCardAction = MakeAction(TEXT("IA_MilCard"), EInputActionValueType::Boolean);
	FocusAction   = MakeAction(TEXT("IA_Focus"),   EInputActionValueType::Boolean);

	// bindings mirror 16-input.js: [B] binos, [Z] power, wheel zoom while
	// glassing, [L] lase, [R] mil card, [ENTER] radio box focus.
	Imc->MapKey(LookAction, EKeys::Mouse2D);
	Imc->MapKey(BinosAction, EKeys::B);
	Imc->MapKey(PowerAction, EKeys::Z);
	Imc->MapKey(WheelAction, EKeys::MouseWheelAxis);
	Imc->MapKey(LaseAction, EKeys::L);
	Imc->MapKey(MilCardAction, EKeys::R);
	Imc->MapKey(FocusAction, EKeys::Enter);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogShitfire, Error, TEXT("observer input: no EnhancedInputComponent (DefaultInput.ini regressed?)"));
		return;
	}
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AObserverPawn::OnLook);
	EIC->BindAction(BinosAction, ETriggerEvent::Started, this, &AObserverPawn::OnBinosPressed);
	EIC->BindAction(PowerAction, ETriggerEvent::Started, this, &AObserverPawn::OnPowerPressed);
	EIC->BindAction(WheelAction, ETriggerEvent::Triggered, this, &AObserverPawn::OnWheel);
	EIC->BindAction(LaseAction, ETriggerEvent::Started, this, &AObserverPawn::OnLasePressed);
	EIC->BindAction(MilCardAction, ETriggerEvent::Started, this, &AObserverPawn::OnMilCardPressed);
	EIC->BindAction(FocusAction, ETriggerEvent::Started, this, &AObserverPawn::OnFocusPressed);
}

void AObserverPawn::PawnClientRestart()
{
	Super::PawnClientRestart();   // rebuilds the input stack -> SetupPlayerInputComponent

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Sub->ClearAllMappings();
		if (Imc) { Sub->AddMappingContext(Imc, 0); }
	}

	if (PC->IsLocalController())
	{
		// browser pitch clamp +/-1.35 rad (00-config.js:40) as the camera
		// manager's hard limit too, so bench/demo SetControlRotation writes
		// obey the same doctrine.
		if (PC->PlayerCameraManager)
		{
			const float ClampDeg = FMath::RadiansToDegrees(Cfg::PitchClampRad);
			PC->PlayerCameraManager->ViewPitchMin = -ClampDeg;
			PC->PlayerCameraManager->ViewPitchMax = ClampDeg;
		}
		if (!Hud)
		{
			Hud = CreateWidget<UObserverHudWidget>(PC, UObserverHudWidget::StaticClass());
			if (Hud)
			{
				Hud->SetObserver(this);
				Hud->AddToViewport(0);
				Hud->SetBinoMode(false);
				Hud->RefreshTool();
			}
		}
	}
}

void AObserverPawn::OnLook(const FInputActionValue& Value)
{
	// mousemove (09-camera.js:21-26): s = mouseSens * (binos ? fovNow/fov : 1);
	// yaw += dx*s; pitch = clamp(pitch - dy*s, +/-pitchClamp).
	// UE's MouseY is the NEGATED screen delta - FSceneViewport accumulates
	// MouseDelta.Y -= CursorDelta.Y (SceneViewport.cpp:931, UE 5.8) - so
	// mouse-forward is +Y here where the browser's movementY is negative:
	// the browser's subtraction becomes an addition. MouseX matches movementX.
	const FVector2D D = Value.Get<FVector2D>();
	const double S = Cfg::MouseSens * (bBinos ? VerticalFovDeg() / Cfg::NakedFovDeg : 1.0);
	double Yaw, Pitch;
	GetSimYawPitch(Yaw, Pitch);
	Yaw += D.X * S;
	Pitch = FMath::Clamp(Pitch + D.Y * S, -Cfg::PitchClampRad, Cfg::PitchClampRad);
	SetSimYawPitch(Yaw, Pitch);
}

void AObserverPawn::OnBinosPressed()
{
	SetBinos(!bBinos);                                        // 16-input.js:147
}

void AObserverPawn::OnPowerPressed()
{
	// [Z], shift reverses (16-input.js:148). Shift is read directly off the
	// PlayerController rather than a chorded trigger - one binding, checked
	// at press time, same result.
	const APlayerController* PC = Cast<APlayerController>(Controller);
	const bool bShift = PC &&
		(PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	CycleBinoZoom(bShift ? -1 : 1);
}

void AObserverPawn::OnWheel(const FInputActionValue& Value)
{
	// wheel zoom only while glassing (16-input.js:211-215); wheel up = next
	// power (browser deltaY<0 = +1; UE wheel up = positive axis).
	if (!bBinos) { return; }
	const double V = Value.Get<float>();
	if (V == 0.0) { return; }
	SetBinoZoom(BinoZoomIdx + (V > 0.0 ? 1 : -1));
}

void AObserverPawn::OnLasePressed()
{
	DoLase();                                                 // 16-input.js:156
}

void AObserverPawn::OnMilCardPressed()
{
	ToggleMilCard();                                          // 16-input.js:154
}

void AObserverPawn::OnFocusPressed()
{
	// [ENTER] focuses the radio text box (16-input.js:181). The box is
	// BUILDER-UI's; the delegate is the documented seam.
	UE_LOG(LogShitfire, Verbose, TEXT("[ENTER] focus-text requested (awaiting BUILDER-UI's box)"));
	OnFocusTextRequested.Broadcast();
}

// ------------------------------------------------------------- lifecycle ----

void AObserverPawn::BeginPlay()
{
	Super::BeginPlay();

	// World-actor discovery is lazy (FindWorldActor) - at this point the
	// GameMode's BeginPlay may not have spawned it yet. HUD creation waits
	// for possession (PawnClientRestart) - Controller is typically still
	// null here.
	bDemo = FParse::Param(FCommandLine::Get(), TEXT("ShitfireObserverDemo"));
	if (bDemo)
	{
		DemoT0 = GetWorld()->GetTimeSeconds();
		UE_LOG(LogShitfire, Display, TEXT("observer demo: scripted binos/power/lase pass with screenshots"));
	}
}

void AObserverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = GetWorld()->GetTimeSeconds();

	// ---- presentation sway (09-camera.js:35-49, 13i) ----------------------
	// Constant-angle two-frequency wobble on the CAMERA only - control
	// rotation (the browser's yaw/pitch state) is untouched. Clocked on
	// unseeded real time: presentation-only by design, nothing downstream
	// reads it back.
	double SwayYaw = 0.0, SwayPitch = 0.0;
	if (bBinos && SwayMilAmplitude > 0.0)
	{
		const double Settle = 1.0 + 2.4 * FMath::Exp(-(Now - SwayT0) / 0.45);   // raise transient ~1 s
		const double Steady = Now < SwaySteadyUntil ? 0.08 : 1.0;               // the lase squeeze
		const double A = SwayMilAmplitude * 0.00098 * Settle * Steady;          // mils -> rad (:42)
		SwayYaw   = A * (FMath::Sin(Now * 1.1) + 0.45 * FMath::Sin(Now * 2.7 + 1.3));
		SwayPitch = A * (FMath::Sin(Now * 0.83 + 0.7) + 0.45 * FMath::Sin(Now * 2.1 + 4.2));
	}

	double Yaw, Pitch;
	GetSimYawPitch(Yaw, Pitch);
	Camera->SetWorldRotation(FRotator(
		FMath::RadiansToDegrees(Pitch + SwayPitch),
		FMath::RadiansToDegrees(Yaw + SwayYaw) - 90.0, 0.0));

	// ---- scheduled lase halves (09-camera.js:208,221) - SIM clock ---------
	// LaseClockNow(), not the world Now above (that one is the sway only).
	const double LaseNow = LaseClockNow();
	if (LaseFireAt >= 0.0 && LaseNow >= LaseFireAt)
	{
		LaseFireAt = -1.0;
		if (PendingLaseToken == LaseToken) { ResolveLase(); }
	}
	if (LaseClearAt >= 0.0 && LaseNow >= LaseClearAt)
	{
		LaseClearAt = -1.0;
		if (ClearLaseToken == LaseToken && Hud) { Hud->SetLaseText(FText::GetEmpty()); }
	}

	UpdateHeading();

	if (bDemo) { TickDemo(Now); }
}

void AObserverPawn::UpdateHeading()
{
	// 17-loop-boot.js:121-130: the readout is MAGNETIC, labelled MAG -
	// "this is an instrument, and instruments read magnetic" (G2). There is
	// no grid/mag TOGGLE in the browser (checked all of 16-input.js); ported
	// as-is. hdg = Math.round(radToMils(yaw)), then MAG fmtMils(gridToMag(hdg)).
	double Yaw, Pitch;
	GetSimYawPitch(Yaw, Pitch);
	const double Hdg = shitfire::js::round(shitfire::radToMils(Yaw));
	if (Hdg != LastHeadingMils)
	{
		LastHeadingMils = Hdg;
		if (Hud)
		{
			const std::string S = shitfire::fmtMils(
				shitfire::doctrine::gridToMag(Hdg, Cfg::DeclEastDeg));
			Hud->SetHeadingText(FText::FromString(
				FString::Printf(TEXT("MAG %s"), UTF8_TO_TCHAR(S.c_str()))));
		}
	}
}

void AObserverPawn::Hint(const FString& Msg)
{
	// The browser logs these to the comms panel (log(...,'sys')). The comms
	// log is BUILDER-BRIDGE/UI scope; until it exists the HUD shows a
	// transient hint line so the guards aren't silent. BUILDER-UI: route
	// these into the real comms log and delete the HUD hint.
	UE_LOG(LogShitfire, Log, TEXT("sys hint: %s"), *Msg);
	if (Hud) { Hud->ShowHint(FText::FromString(Msg), 5.0); }
}

// ---------------------------------------------------------------- demo ----

void AObserverPawn::TickDemo(double Now)
{
	// -ShitfireObserverDemo: the headless stand-in for "look around, cycle
	// powers, lase 3 different targets, screenshot each". [B]/[L] go through
	// PlayerController::InputKey so the Enhanced Input wiring itself is
	// exercised, not just the functions behind it; slews go straight to the
	// control rotation (mouse deltas can't be faked as cleanly).
	const double T = Now - DemoT0;
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC) { return; }

	auto Key = [&](const FKey& K) {
		FViewport* VP = GetWorld()->GetGameViewport() ? GetWorld()->GetGameViewport()->Viewport : nullptr;
		// internal id 0 IS the default keyboard/mouse device (what
		// GetDefaultInputDevice returns); constructed directly because the
		// mapper's Get() would drag an ApplicationCore link dep into the
		// shared Build.cs for a demo-only convenience
		const FInputDeviceId Dev = FInputDeviceId::CreateFromInternalId(0);
		// release-then-press: repeated injections of the same key must show
		// Enhanced Input a fresh press edge or Started only fires once
		PC->InputKey(FInputKeyEventArgs(VP, Dev, K, EInputEvent::IE_Released, 0u));
		PC->InputKey(FInputKeyEventArgs(VP, Dev, K, EInputEvent::IE_Pressed, 0u));
	};
	auto Shot = [&](const TCHAR* Name) {
		FScreenshotRequest::RequestScreenshot(Name, /*bShowUI*/ true, /*bAddUniqueSuffix*/ false);
	};
	auto Slew = [&](double DYawDeg, double DPitchDeg) {
		double Y, P;
		GetSimYawPitch(Y, P);
		SetSimYawPitch(Y + FMath::DegreesToRadians(DYawDeg),
			FMath::Clamp(P + FMath::DegreesToRadians(DPitchDeg),
				-Cfg::PitchClampRad, Cfg::PitchClampRad));
	};

	switch (DemoStage)
	{
	case 0: if (T > 2.0) { Shot(TEXT("ObserverDemo_0_eyes")); DemoStage++; } break;
	case 1: if (T > 3.0) { Key(EKeys::B); DemoStage++; } break;                       // binos 7X via input stack
	case 2: if (T > 4.0)
		{
			if (!bBinos)
			{
				UE_LOG(LogShitfire, Warning, TEXT("demo: InputKey(B) did not raise binos - falling back to SetBinos"));
				SetBinos(true);
			}
			Key(EKeys::L); DemoStage++;                                               // lase target 1
		} break;
	case 3: if (T > 6.5) { Shot(TEXT("ObserverDemo_1_7x_lase")); DemoStage++; } break;
	case 4: if (T > 7.5) { Slew(6.0, -0.5); CycleBinoZoom(1); DemoStage++; } break;   // 14X, new target
	case 5: if (T > 8.5) { Key(EKeys::L); DemoStage++; } break;
	case 6: if (T > 11.0) { Shot(TEXT("ObserverDemo_2_14x_lase")); DemoStage++; } break;
	case 7: if (T > 12.0) { Slew(-14.0, -1.0); CycleBinoZoom(1); DemoStage++; } break; // wraps to 4X
	case 8: if (T > 13.0) { Key(EKeys::L); DemoStage++; } break;
	case 9: if (T > 15.5) { Shot(TEXT("ObserverDemo_3_4x_lase")); DemoStage++; } break;
	case 10: if (T > 16.5) { Key(EKeys::R); DemoStage++; } break;                     // mil card up
	case 11: if (T > 17.5) { Shot(TEXT("ObserverDemo_4_milcard")); DemoStage++; } break;
	case 12: if (T > 19.0) { PC->ConsoleCommand(TEXT("quit")); DemoStage++; } break;
	default: break;
	}
}
