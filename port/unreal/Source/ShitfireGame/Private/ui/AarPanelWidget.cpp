#include "ui/AarPanelWidget.h"
#include "ShitfireGame.h"
#include "ui/NetHudStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/review/tlog.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/sim/mission_effects.h"
#include "shitfire/sim/mission_machine.h"
#include "shitfire/sim_core.h"

namespace
{
	// fmtTime (13-aar.js:235-238)
	FString FmtTime(double S)
	{
		const int32 M = FMath::FloorToInt32(S / 60.0);
		const int32 R = FMath::FloorToInt32(S - M * 60.0);
		return FString::Printf(TEXT("%d:%02d"), M, R);
	}

	// starStr (15-campaign-menu.js:239): n filled + (5-n) empty
	FString StarStr(int32 N)
	{
		FString Out;
		for (int32 i = 0; i < 5; i++)
		{
			Out += (i < N) ? TEXT("★") : TEXT("☆");
		}
		return Out;
	}
}

void UShitfireAarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
}

TSharedRef<SWidget> UShitfireAarWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UShitfireAarWidget::BuildTree()
{
	if (bTreeBuilt)
	{
		return;
	}
	bTreeBuilt = true;
	SetIsFocusable(true);   // ESC/ENTER land here while shown

	// #aar scrim (head.html:184: rgba(0,0,0,.72), UX8)
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Scrim"));
	Scrim->SetBrushColor(ShitfireUi::AarScrim);
	Scrim->SetHorizontalAlignment(HAlign_Center);
	Scrim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Scrim;

	// #aarbox clamp(560px, 46vw, 980px) -> 860 at 1080p; max-height 94vh -> 1000
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Box"));
	Box->SetWidthOverride(860.f);
	Box->SetMaxDesiredHeight(1000.f);
	Scrim->SetContent(Box);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(ShitfireUi::PanelBorder);
	Frame->SetPadding(FMargin(1.f));
	Box->AddChild(Frame);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(ShitfireUi::PanelBg);
	Panel->SetPadding(FMargin(0.f));
	Frame->SetContent(Panel);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);   // UX3: SAYS it scrolls
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Scroll))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UBorder* ContentPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContentPad"));
	ContentPad->SetBrushColor(FLinearColor::Transparent);
	ContentPad->SetPadding(FMargin(26.f, 20.f, 26.f, 0.f));   // #aarbox padding
	Scroll->AddChild(ContentPad);
	Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	ContentPad->SetContent(Content);

	// .foot - sticky exit-keys strip (UX3, head.html:215-217)
	UBorder* FootSep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FootSep"));
	FootSep->SetBrushColor(ShitfireUi::PanelBorder);
	FootSep->SetPadding(FMargin(0.f));
	USizeBox* FootSepBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FootSepBox"));
	FootSepBox->SetHeightOverride(1.f);
	FootSepBox->AddChild(FootSep);
	Col->AddChildToVerticalBox(FootSepBox);

	UBorder* Foot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Foot"));
	Foot->SetBrushColor(ShitfireUi::AarFootBg);
	Foot->SetPadding(FMargin(26.f, 9.f));
	FootText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FootText"));
	FootText->SetFont(ShitfireUi::Mono(9));
	FootText->SetColorAndOpacity(FSlateColor(ShitfireUi::FootText));
	Foot->SetContent(FootText);
	Col->AddChildToVerticalBox(Foot);

	SetVisibility(ESlateVisibility::Collapsed);
}

UTextBlock* UShitfireAarWidget::AddText(const FString& S, int32 Size, const FLinearColor& Color,
                                        int32 LetterSpacing, const FMargin& Pad, bool bWrap)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetFont(ShitfireUi::Mono(Size, LetterSpacing));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetAutoWrapText(bWrap);
	T->SetText(FText::FromString(S));
	if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(T))
	{
		BoxSlot->SetPadding(Pad);
	}
	return T;
}

void UShitfireAarWidget::AddLabel(const FString& S)
{
	// .mlabel (head.html:244)
	AddText(S, 8, ShitfireUi::FootText, 140, FMargin(0.f, 8.f, 0.f, 4.f));
}

void UShitfireAarWidget::AddBullets(const TArray<FString>& Lines, const FLinearColor& Color)
{
	for (const FString& L : Lines)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Dot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Dot->SetFont(ShitfireUi::Mono(10));
		Dot->SetColorAndOpacity(FSlateColor(Color));
		Dot->SetText(FText::FromString(TEXT("•")));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Dot))
		{
			S->SetPadding(FMargin(6.f, 0.f, 8.f, 0.f));
		}
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetFont(ShitfireUi::Mono(10));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		T->SetText(FText::FromString(L));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(T))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(Row))
		{
			S->SetPadding(FMargin(0.f, 1.f));
		}
	}
}

void UShitfireAarWidget::AddKV(const FString& Label, const FString& Value, bool bFirst)
{
	// #aarbox table rows (head.html:208-213): 44% label column, right-aligned
	// amber values, 1px hairline between rows.
	if (!bFirst)
	{
		USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SepBox->SetHeightOverride(1.f);
		UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Sep->SetBrushColor(ShitfireUi::RowSep);
		SepBox->AddChild(Sep);
		Content->AddChildToVerticalBox(SepBox);
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	L->SetFont(ShitfireUi::Mono(10));
	L->SetColorAndOpacity(FSlateColor(ShitfireUi::BodyText));
	L->SetAutoWrapText(true);
	L->SetText(FText::FromString(Label));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L))
	{
		FSlateChildSize LabelSize(ESlateSizeRule::Fill);
		LabelSize.Value = 0.44f;   // td:first-child width 44% (head.html:211)
		S->SetSize(LabelSize);
		S->SetPadding(FMargin(0.f, 3.f, 16.f, 3.f));
	}
	UTextBlock* V = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	V->SetFont(ShitfireUi::Mono(10));
	V->SetColorAndOpacity(FSlateColor(ShitfireUi::StarGold));
	V->SetAutoWrapText(true);
	V->SetJustification(ETextJustify::Right);
	V->SetText(FText::FromString(Value));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(V))
	{
		FSlateChildSize ValueSize(ESlateSizeRule::Fill);
		ValueSize.Value = 0.56f;
		S->SetSize(ValueSize);
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 3.f));
	}
	Content->AddChildToVerticalBox(Row);
}

void UShitfireAarWidget::ShowFromHost(UShitfireSimHost* Host, const FShitfireAarSummary& Summary,
                                      const FString& ChapterId)
{
	namespace sf = shitfire;
	BuildTree();
	if (!Content)
	{
		return;
	}
	Content->ClearChildren();
	VerdictText = nullptr;
	StarsText = nullptr;

	// ---- gather ----------------------------------------------------------
	sf::SimCore* Sim = (Host && Host->IsMissionLive()) ? Host->GetSimCore() : nullptr;
	const sf::review::TlogEntry* AarRow = nullptr;
	const sf::sim::Mission* M = nullptr;
	const sf::scenario::ScenarioState* Scn = nullptr;
	sf::sim::MissionMachine* Machine = nullptr;
	if (Sim)
	{
		const std::vector<sf::review::TlogEntry>& Es = Sim->tlog().entries();
		for (auto It = Es.rbegin(); It != Es.rend(); ++It)
		{
			if (It->kind == "aar") { AarRow = &*It; break; }
		}
		Machine = Sim->parts().machine;
		if (Machine) { M = Machine->activeMission(); }
		Scn = Sim->parts().scenario;
	}
	const sf::campaign::ChapterSpec* Ch =
		sf::campaign::findChapter(TCHAR_TO_UTF8(*ChapterId));

	const bool bPass = Summary.Verdict.StartsWith(TEXT("PASS"));
	const FString Difficulty = Scn ? UTF8_TO_TCHAR(Scn->difficulty.c_str()) : TEXT("normal");
	const FString ScnType = Scn ? UTF8_TO_TCHAR(Scn->type.c_str()) : TEXT("");
	const bool bConvoy = ScnType == TEXT("convoy");
	const bool bNoDisp = AarRow && AarRow->noDisp.value_or(false);
	const int32 NotesLen = AarRow ? static_cast<int32>(AarRow->notes.value_or(0.0)) : 0;
	const double TInit = AarRow ? AarRow->tInit.value_or(0.0) : 0.0;
	const bool bHaveFirstMiss = AarRow && AarRow->firstMiss.present && AarRow->firstMiss.value.has_value();
	const double FirstMiss = bHaveFirstMiss ? *AarRow->firstMiss.value : 0.0;
	const bool bHaveAimErr = AarRow && AarRow->aimErr0.present && AarRow->aimErr0.value.has_value();
	const double AimErr0 = bHaveAimErr ? *AarRow->aimErr0.value : 0.0;
	const bool bHaveCorrEff = AarRow && AarRow->corrEff.present && AarRow->corrEff.value.has_value();
	const double CorrEffPct = bHaveCorrEff ? *AarRow->corrEff.value : 0.0;   // already 0-100
	TArray<double> Trace;
	if (AarRow && AarRow->trace.has_value())
	{
		for (double D : *AarRow->trace) { Trace.Add(D); }
	}
	const int32 AdjRounds = static_cast<int32>(Summary.Rounds);
	const double Dur = Summary.DurSeconds;
	const double ParC = (Ch && Ch->par.has_value()) ? *Ch->par : 300.0;   // 13-aar.js:352
	const int32 Wasted = M ? M->wasted : 0;
	const double Seed = AarRow ? AarRow->seed : 0.0;

	// effect bands for the assessed-effect row (10-mission.js:926-934)
	bool bHaveBands = false;
	sf::sim::EffBandSet Bands{};
	FString TgtClassStr = Scn ? UTF8_TO_TCHAR(Scn->tgtClass.c_str()) : TEXT("personnel");
	if (Machine && Scn)
	{
		const sf::sim::AssetKind Asset =
			(Ch && Ch->asset == "mortar60") ? sf::sim::AssetKind::Mortar60 :
			(Ch && Ch->asset == "sunlamp") ? sf::sim::AssetKind::Sunlamp :
			sf::sim::AssetKind::Battery155;
		const sf::sim::TargetClass Cls = (Scn->tgtClass == "point")
			? sf::sim::TargetClass::Point : sf::sim::TargetClass::Personnel;
		Bands = sf::sim::effBands(Asset, Cls, Scn->effScale, Machine->tunables().effects);
		bHaveBands = true;
	}

	// ---- header (13-aar.js:406-408) --------------------------------------
	const FString Header = Ch
		? FString::Printf(TEXT("AAR — CHAPTER %s: %s (%s)"),
			*ChapterId, UTF8_TO_TCHAR(Ch->title.c_str()), *Difficulty.ToUpper())
		: FString::Printf(TEXT("AAR — %s (%s)"), *ScnType.ToUpper(), *Difficulty.ToUpper());
	AddText(Header, 12, ShitfireUi::TopbarText, 140, FMargin(0.f, 0.f, 0.f, 4.f));

	// ---- stars (13-aar.js:381-404) ---------------------------------------
	if (bNoDisp)
	{
		AddText(TEXT("NOT GRADED — round dispersion was disabled (SHIFT+D) during this mission. No stars recorded."),
			10, ShitfireUi::WarnRed, 0, FMargin(0.f, 0.f, 0.f, 8.f));
	}
	else if (Summary.Stars >= 0)
	{
		StarsText = AddText(StarStr(Summary.Stars), 16, ShitfireUi::StarGold, 250,
			FMargin(0.f, 6.f, 0.f, 10.f), false);
		const int32 Cap = Difficulty == TEXT("easy") ? 3 : Difficulty == TEXT("hard") ? 5 : 4;
		if (Summary.Stars > 0 && Summary.Stars == Cap && Cap < 5)
		{
			AddText(FString::Printf(TEXT("Capped at %d★ on %s — full five need HARD."),
				Cap, *Difficulty.ToUpper()), 10, ShitfireUi::HeadText, 0, FMargin(0.f, 0.f, 0.f, 8.f));
		}
	}

	// ---- verdict (13-aar.js:410) -----------------------------------------
	VerdictText = AddText(Summary.Verdict, 15,
		bPass ? ShitfireUi::AarPass : ShitfireUi::AarFail, 200, FMargin(0.f, 8.f, 0.f, 12.f));

	// why - reconstructed for the outcome-driven verdicts (13-aar.js:283-319;
	// the fail-reason exotics carry their story in the verdict itself)
	FString Why;
	if (bHaveBands && Summary.Verdict.Contains(TEXT("NEUTRALIZED")) && bPass)
	{
		Why = FString::Printf(TEXT("Assessed %.0f%% casualties — combat-ineffective (>=%.0f%%), short of destruction (%.0f%%)."),
			Summary.EffPct, Bands.neutralizePct, Bands.destroyPct);
	}
	else if (bHaveBands && Summary.Verdict.Contains(TEXT("DESTROYED")) && bPass && !bConvoy)
	{
		Why = FString::Printf(TEXT("Assessed %.0f%% casualties — past the %.0f%% that takes a unit off the books permanently."),
			Summary.EffPct, Bands.destroyPct);
	}
	else if (Summary.Verdict.Contains(TEXT("TOO MANY ADJUSTING")))
	{
		Why = FString::Printf(TEXT("Target %s, but %d adjusting rounds used (pass <= %.0f)."),
			*Summary.Outcome, AdjRounds,
			Machine ? Machine->tunables().mission.passMaxAdjustRounds : 4.0);
	}
	else if (bHaveBands && Summary.Verdict.Contains(TEXT("SUPPRESSED ONLY")))
	{
		Why = FString::Printf(TEXT("Heads went down while the rounds fell (%.0f%% casualties — neutralization needs %.0f%%), and suppression wears off. They are back on their guns."),
			Summary.EffPct, Bands.neutralizePct);
	}
	if (!Why.IsEmpty())
	{
		AddText(Why, 10, ShitfireUi::BodyText, 0, FMargin(0.f, 0.f, 0.f, 10.f));
	}

	// ---- commendations (13-aar.js:353-362, UX3: they lead) ----------------
	TArray<FString> Medals;
	if (bPass)
	{
		if (AdjRounds <= 1) { Medals.Add(TEXT("HELL OF A HOLE IN ONE — effect after a single adjusting round")); }
		if (bHaveAimErr && AimErr0 <= 50) { Medals.Add(TEXT("EAGLE EYE — initial location within 50 m")); }
		if (NotesLen == 0) { Medals.Add(TEXT("ACTUALLY READ THE MANUAL — clean call format, no coaching")); }
		if (Dur <= ParC * 0.6) { Medals.Add(TEXT("FAST MOVER — done in well under par")); }
		if (Ch && Ch->strict) { Medals.Add(TEXT("IRON NET — passed under strict doctrine")); }
		if (M && (M->refined || M->recorded)) { Medals.Add(TEXT("BY THE BOOK — closed the mission with refinement / record as target (RREMS)")); }
		if (M && M->otDirSent) { Medals.Add(TEXT("ORIENTED — OT direction passed to the FDC")); }
		if (Ch && Ch->asset == "mortar60") { Medals.Add(TEXT("TEN-METER MAN — precision work with the sixty")); }
	}
	if (Medals.Num() > 0)
	{
		AddLabel(TEXT("COMMENDATIONS"));
		AddBullets(Medals, ShitfireUi::DiagText);
	}

	// ---- metrics table (13-aar.js:419-447 + P4's explicit wasted row) -----
	AddText(TEXT(""), 4, ShitfireUi::BodyText, 0, FMargin(0.f, 4.f, 0.f, 0.f), false);
	bool bFirst = true;
	auto KV = [&](const FString& K, const FString& V) { AddKV(K, V, bFirst); bFirst = false; };
	KV(TEXT("Adjusting rounds"), FString::Printf(TEXT("%d"), AdjRounds));
	KV(TEXT("First-round miss"), bHaveFirstMiss
		? FString::Printf(TEXT("%.0f m"), FirstMiss) : FString(TEXT("—")));
	KV(TEXT("Initial location error"), bHaveAimErr
		? FString::Printf(TEXT("%.0f m (JFO standard <= 200 m)"), AimErr0) : FString(TEXT("—")));
	if (!bConvoy && bHaveBands)
	{
		KV(TEXT("Assessed effect"), FString::Printf(TEXT("%.0f%% %s — %s (neutralize >=%.0f%%, destroy >=%.0f%%)"),
			Summary.EffPct,
			TgtClassStr == TEXT("point") ? TEXT("structural damage") : TEXT("casualties"),
			Summary.Outcome == TEXT("none") ? TEXT("NO EFFECT") : *Summary.Outcome.ToUpper(),
			Bands.neutralizePct, Bands.destroyPct));
		if (M)
		{
			KV(TEXT("Effect rounds in the outer band"), FString::Printf(TEXT("%d/%d within %.0f m"),
				M->hits, static_cast<int32>(M->ffeRounds.size()), Bands.rSupp));
		}
	}
	if (!Summary.BdaClaim.IsEmpty())
	{
		KV(TEXT("Surveillance sent"), FString::Printf(TEXT("%s%s"), *Summary.BdaClaim.ToUpper(),
			Summary.BdaClaim == Summary.Outcome
				? TEXT(" — matches the assessment")
				: *FString::Printf(TEXT(" — assessed: %s"),
					Summary.Outcome == TEXT("none") ? TEXT("NO EFFECT") : *Summary.Outcome.ToUpper())));
	}
	if (M && M->sheaf.kind != sf::sim::SheafKind::None)
	{
		// `${kind.toUpperCase()} (${source}) — ${why}` (13-aar.js:435)
		const TCHAR* SheafKindStr =
			M->sheaf.kind == sf::sim::SheafKind::Converged ? TEXT("CONVERGED") :
			M->sheaf.kind == sf::sim::SheafKind::Circular ? TEXT("CIRCULAR") :
			M->sheaf.kind == sf::sim::SheafKind::Open ? TEXT("OPEN") : TEXT("LINEAR");
		KV(TEXT("Sheaf"), FString::Printf(TEXT("%s (%s) — %s"), SheafKindStr,
			UTF8_TO_TCHAR(M->sheaf.source.c_str()), UTF8_TO_TCHAR(M->sheaf.why.c_str())));
	}
	if (M && M->fuze.kind != sf::sim::FuzeKind::None)
	{
		const TCHAR* FuzeKindStr =
			M->fuze.kind == sf::sim::FuzeKind::Pd ? TEXT("PD") :
			M->fuze.kind == sf::sim::FuzeKind::Vt ? TEXT("VT") :
			M->fuze.kind == sf::sim::FuzeKind::Time ? TEXT("TIME") : TEXT("DELAY");
		KV(TEXT("Fuze"), FString::Printf(TEXT("%s (%s) — %s"), FuzeKindStr,
			UTF8_TO_TCHAR(M->fuze.source.c_str()), UTF8_TO_TCHAR(M->fuze.why.c_str())));
	}
	if (AarRow && AarRow->shell.has_value() && *AarRow->shell != "he")
	{
		KV(TEXT("Shell"), *AarRow->shell == "illum"
			? TEXT("ILLUMINATION — light, not fires") : TEXT("SMOKE — a screen, not casualties"));
	}
	KV(TEXT("Time to initiate CFF"), FString::Printf(TEXT("%s (standard <= 2:00)"), *FmtTime(TInit)));
	KV(TEXT("Mission time"), FmtTime(Dur));
	KV(TEXT("Call format grade"), FString(TEXT("ABCD")).Mid(FMath::Min(NotesLen, 3), 1));
	if (bNoDisp) { KV(TEXT("Round dispersion"), TEXT("DISABLED — test mode, results not representative")); }
	if (M && M->usedCheckFire) { KV(TEXT("Safety stop"), TEXT("CHECK FIRING / CEASE LOADING called during this mission")); }
	if (M && M->mto.has_value())
	{
		KV(TEXT("MTO readback"), M->mto->read
			? TEXT("READ BACK")
			: TEXT("NOT read back — the FDC never got confirmation you copied the target number"));
	}
	if (Trace.Num() > 0)
	{
		FString TraceStr;
		for (int32 i = 0; i < Trace.Num(); i++)
		{
			TraceStr += FString::Printf(TEXT("%s%.0f"), i ? TEXT(" -> ") : TEXT(""), Trace[i]);
		}
		KV(TEXT("Adjusting round trace"), TraceStr + TEXT(" m"));
	}
	if (bHaveCorrEff)
	{
		KV(TEXT("Correction efficiency"), FString::Printf(
			TEXT("%.0f%% miss reduction/round (doctrine ~50%%)"), CorrEffPct));
	}
	if (Trace.Num() >= 2)
	{
		KV(TEXT("Bracket established"), Summary.bBracketed
			? TEXT("YES") : TEXT("NO — all rounds one side"));
	}
	KV(TEXT("Wasted rounds (miss changed < 15%)"), FString::Printf(TEXT("%d"), Wasted));

	// ---- adjustment diagnosis (13-aar.js:330-344) -------------------------
	TArray<FString> Diag;
	if (TInit > 120)
	{
		Diag.Add(FString::Printf(TEXT("Call for fire took %s to initiate (standard: within 2:00 of identifying the target). Locate faster — the target is not waiting for you."), *FmtTime(TInit)));
	}
	if (bHaveAimErr && AimErr0 > 200)
	{
		Diag.Add(FString::Printf(TEXT("Initial target location was %.0f m off (standard: <= 200 m). Everything after that was recovery — work the map, or lase and read the grid, before you transmit."), AimErr0));
	}
	if (bHaveCorrEff && CorrEffPct < 30 && Trace.Num() >= 3)
	{
		Diag.Add(FString::Printf(TEXT("Correction efficiency %.0f%% per round — doctrine's successive bracketing roughly halves the miss each time (~50%%). Your corrections were too small for the error you were seeing."), CorrEffPct));
	}
	if (Trace.Num() >= 3 && !Summary.bBracketed)
	{
		Diag.Add(TEXT("You never bracketed the target — every adjusting round landed on the same side of it. Fire a correction big enough to cross the target, then split the difference."));
	}
	if (Wasted >= 2)
	{
		Diag.Add(FString::Printf(TEXT("%d adjusting rounds changed the miss by less than 15%% — rounds and time spent for nothing."), Wasted));
	}
	if (Diag.Num() == 0 && bPass && AdjRounds <= 3)
	{
		Diag.Add(TEXT("Clean adjustment: bold first correction, bracket, split, effect. That is the method."));
	}
	if (Diag.Num() > 0)
	{
		AddLabel(TEXT("ADJUSTMENT DIAGNOSIS"));
		AddBullets(Diag, ShitfireUi::DiagText);
	}

	// ---- format coaching (13-aar.js:449 - the note TEXT lives on Mission) --
	if (M && !M->notes.empty())
	{
		AddText(TEXT("Format coaching:"), 10, ShitfireUi::BodyText, 0, FMargin(0.f, 8.f, 0.f, 2.f));
		TArray<FString> Notes;
		for (const std::string& N : M->notes) { Notes.Add(UTF8_TO_TCHAR(N.c_str())); }
		AddBullets(Notes, ShitfireUi::DiagText);
	}

	// ---- outro + replay hint (13-aar.js:450-452) --------------------------
	if (Ch && !Ch->outro.empty() && bPass)
	{
		AddText(UTF8_TO_TCHAR(Ch->outro.c_str()), 10, ShitfireUi::HeadText, 0, FMargin(0.f, 8.f));
	}
	if (Ch)
	{
		AddText(TEXT("Replay from the mission menu to improve your stars."),
			10, ShitfireUi::HeadText, 0, FMargin(0.f, 0.f, 0.f, 6.f));
	}
	AddText(TEXT(""), 6, ShitfireUi::BodyText, 0, FMargin(0.f), false);   // bottom breathing room

	if (FootText)
	{
		FootText->SetText(FText::FromString(FString::Printf(
			TEXT("[ESC] or [ENTER] — back to the OP · seed %.0f"), Seed)));
	}

	SetVisibility(ESlateVisibility::Visible);
	if (Scroll) { Scroll->ScrollToStart(); }
}

void UShitfireAarWidget::TakeKeyboardFocus()
{
	const TSharedPtr<SWidget> S = GetCachedWidget();
	if (S.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(S, EFocusCause::SetDirectly);
	}
}

void UShitfireAarWidget::Dismiss()
{
	if (IsShowing())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		OnDismissed.ExecuteIfBound();
	}
}

bool UShitfireAarWidget::IsShowing() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

FString UShitfireAarWidget::VerdictLineText() const
{
	return VerdictText ? VerdictText->GetText().ToString() : FString();
}

FString UShitfireAarWidget::StarsLineText() const
{
	return StarsText ? StarsText->GetText().ToString() : FString();
}

FReply UShitfireAarWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsShowing() &&
		(InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Enter))
	{
		Dismiss();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
