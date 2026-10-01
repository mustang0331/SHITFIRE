#include "ui/CommsLogWidget.h"
#include "ShitfireGame.h"
#include "ui/NetHudStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

namespace
{
	FLinearColor RoleColor(EShitfireLogRole Role)
	{
		switch (Role)
		{
		case EShitfireLogRole::Obs:    return ShitfireUi::ObsCol;
		case EShitfireLogRole::Fdc:    return ShitfireUi::FdcCol;
		case EShitfireLogRole::Sys:    return ShitfireUi::SysCol;
		case EShitfireLogRole::Caller: return ShitfireUi::CallerCol;
		case EShitfireLogRole::Note:   return ShitfireUi::NoteCol;
		default:                       return ShitfireUi::BodyText;
		}
	}

	/** #log rows carry no hover/selection chrome (head.html:58-66). */
	const FTableRowStyle& NullRowStyle()
	{
		static const FTableRowStyle Style = []() {
			FTableRowStyle S = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
			S.SetEvenRowBackgroundBrush(FSlateNoResource());
			S.SetEvenRowBackgroundHoveredBrush(FSlateNoResource());
			S.SetOddRowBackgroundBrush(FSlateNoResource());
			S.SetOddRowBackgroundHoveredBrush(FSlateNoResource());
			S.SetActiveBrush(FSlateNoResource());
			S.SetActiveHoveredBrush(FSlateNoResource());
			S.SetInactiveBrush(FSlateNoResource());
			S.SetInactiveHoveredBrush(FSlateNoResource());
			return S;
		}();
		return Style;
	}
}

void UShitfireCommsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
}

TSharedRef<SWidget> UShitfireCommsWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UShitfireCommsWidget::BuildTree()
{
	if (bTreeBuilt)
	{
		return;
	}
	bTreeBuilt = true;

	// 1px #39442e frame around the .panel background (head.html:13,:46-51):
	// outer border painted in the border color, inner panel inset by 1px.
	OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	OuterBorder->SetBrushColor(ShitfireUi::PanelBorder);
	OuterBorder->SetPadding(FMargin(1.f));
	WidgetTree->RootWidget = OuterBorder;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(ShitfireUi::PanelBg);
	Panel->SetPadding(FMargin(0.f));
	OuterBorder->SetContent(Panel);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	// ---- #commshead (head.html:52-56,:374): flat strip, callsign title ----
	{
		UBorder* Head = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Head"));
		Head->SetBrushColor(ShitfireUi::HeadBg);
		Head->SetPadding(FMargin(8.f, 3.f));
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeadTitle"));
		Title->SetFont(ShitfireUi::Mono(8, 90));   // 10.5px, .09em
		Title->SetColorAndOpacity(FSlateColor(ShitfireUi::HeadText));
		Title->SetText(FText::FromString(TEXT("RADIO NET — MUSTANG 12")));
		Head->SetContent(Title);
		Col->AddChildToVerticalBox(Head);

		USizeBox* Sep = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeadSep"));
		Sep->SetHeightOverride(1.f);
		UBorder* SepB = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeadSepB"));
		SepB->SetBrushColor(ShitfireUi::PanelBorder);
		Sep->AddChild(SepB);
		Col->AddChildToVerticalBox(Sep);
	}

	// ---- #log (head.html:58): the virtualized transcript, raw SListView ---
	{
		LogListView = SNew(SListView<TSharedPtr<FShitfireLogLine>>)
			.ListItemsSource(&Lines)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(SListView<TSharedPtr<FShitfireLogLine>>::FOnGenerateRow::CreateUObject(
				this, &UShitfireCommsWidget::GenerateLogRow))
			.OnListViewScrolled(FOnTableViewScrolled::CreateUObject(
				this, &UShitfireCommsWidget::HandleListScrolled));

		UBorder* LogPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LogPad"));
		LogPad->SetBrushColor(FLinearColor::Transparent);
		LogPad->SetPadding(FMargin(10.f, 8.f));   // padding: 8px 10px
		LogHost = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("LogHostW"));
		LogHost->SetContent(LogListView.ToSharedRef());
		LogPad->SetContent(LogHost);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(LogPad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));   // flex: 1 1 auto
		}
	}

	// ---- #txform (head.html:67-71,:376-391) --------------------------------
	{
		USizeBox* Sep = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TxSep"));
		Sep->SetHeightOverride(1.f);
		UBorder* SepB = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TxSepB"));
		SepB->SetBrushColor(ShitfireUi::PanelBorder);   // border-top: 1px #39442e
		Sep->AddChild(SepB);
		Col->AddChildToVerticalBox(Sep);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TxRow"));
		PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TxPrompt"));
		PromptText->SetFont(ShitfireUi::Mono(9));
		PromptText->SetColorAndOpacity(FSlateColor(FLinearColor(
			ShitfireUi::ObsCol.R, ShitfireUi::ObsCol.G, ShitfireUi::ObsCol.B, 0.7f)));   // #txprompt opacity .7
		PromptText->SetText(FText::FromString(TEXT("MUSTANG 12 >")));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PromptText))
		{
			S->SetPadding(FMargin(10.f, 9.f, 0.f, 9.f));
			S->SetVerticalAlignment(VAlign_Center);
		}

		InputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("TxInput"));
		FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		Style.SetBackgroundImageNormal(FSlateNoResource());
		Style.SetBackgroundImageHovered(FSlateNoResource());
		Style.SetBackgroundImageFocused(FSlateNoResource());
		Style.SetBackgroundImageReadOnly(FSlateNoResource());
		Style.SetBackgroundColor(FSlateColor(FLinearColor::Transparent));
		Style.SetForegroundColor(FSlateColor(ShitfireUi::ObsCol));           // #txinput color
		Style.SetFocusedForegroundColor(FSlateColor(ShitfireUi::ObsCol));
		Style.SetPadding(FMargin(10.f, 9.f));                                // padding: 9px 10px
		Style.TextStyle.SetFont(ShitfireUi::Mono(9));
		Style.TextStyle.SetColorAndOpacity(FSlateColor(ShitfireUi::ObsCol));
		InputBox->SetWidgetStyle(Style);
		InputBox->SetHintText(FText::FromString(
			TEXT("e.g. \"adjust fire, grid 123 456, infantry in the open, over\" — use YOUR target grid")));
		InputBox->SetIsCaretMovedWhenGainFocus(true);
		InputBox->OnTextChanged.AddDynamic(this, &UShitfireCommsWidget::HandleTextChanged);
		InputBox->OnTextCommitted.AddDynamic(this, &UShitfireCommsWidget::HandleTextCommitted);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(InputBox))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Col->AddChildToVerticalBox(Row);
	}
}

TSharedRef<ITableRow> UShitfireCommsWidget::GenerateLogRow(TSharedPtr<FShitfireLogLine> Line,
                                                           const TSharedRef<STableViewBase>& Owner)
{
	const FLinearColor Col = Line ? RoleColor(Line->Role) : ShitfireUi::BodyText;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	if (Line && !Line->Call.IsEmpty())
	{
		// .call span (head.html:66): same role color at 75% opacity, 6px gap
		Row->AddSlot()
			.AutoWidth()
			.Padding(0.f, 0.f, 6.f, 2.f)
			[
				SNew(STextBlock)
					.Font(ShitfireUi::Mono(9))
					.ColorAndOpacity(FSlateColor(FLinearColor(Col.R, Col.G, Col.B, 0.75f)))
					.Text(FText::FromString(Line->Call + TEXT(":")))
			];
	}
	Row->AddSlot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 0.f, 2.f)   // #log div margin-bottom 2px (:59)
		[
			SNew(STextBlock)
				.Font(ShitfireUi::Mono(9))
				.ColorAndOpacity(FSlateColor(Col))
				.AutoWrapText(true)
				.Text(FText::FromString(Line ? Line->Msg : FString()))
		];

	return SNew(STableRow<TSharedPtr<FShitfireLogLine>>, Owner)
		.Style(&NullRowStyle())
		.ShowSelection(false)
		[
			Row
		];
}

void UShitfireCommsWidget::AppendLine(EShitfireLogRole Role, double SimT,
                                      const FString& Call, const FString& Msg)
{
	BuildTree();
	TSharedPtr<FShitfireLogLine> Line = MakeShared<FShitfireLogLine>();
	Line->Role = Role;
	Line->SimT = SimT;
	Line->Call = Call;
	Line->Msg = Msg;
	Lines.Add(Line);

	// Sanity cap, far above the 2,000-line budget gate: the TLOG itself trims
	// at 2,000 entries (tlog.h maxEntries), so 4,096 on-screen rows means
	// something upstream is broken - trim the oldest quarter, once.
	if (Lines.Num() > 4096)
	{
		Lines.RemoveAt(0, 1024);
	}

	if (LogListView.IsValid())
	{
		LogListView->RequestListRefresh();
		if (bPinnedToBottom)
		{
			LogListView->ScrollToBottom();   // logEl.scrollTop = scrollHeight (:196)
		}
	}
}

void UShitfireCommsWidget::HandleListScrolled(double /*InScrollOffset*/)
{
	// Pinned while the view sits at (or within a line of) the bottom; a user
	// wheel-up unpins, wheeling back down repins. ScrollToBottom lands with
	// remaining 0, so programmatic scrolls keep the pin.
	if (LogListView.IsValid())
	{
		bPinnedToBottom = LogListView->GetScrollDistanceRemaining().Y <= 24.f;
	}
}

void UShitfireCommsWidget::FocusInput()
{
	BuildTree();
	bInputFocused = true;
	OnFocusChanged.ExecuteIfBound(true);
	if (InputBox)
	{
		const TSharedPtr<SWidget> S = InputBox->GetCachedWidget();
		if (S.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(S, EFocusCause::SetDirectly);
		}
	}
}

void UShitfireCommsWidget::BlurInput()
{
	bInputFocused = false;
	OnFocusChanged.ExecuteIfBound(false);
	if (FSlateApplication::IsInitialized() && InputBox && InputBox->HasKeyboardFocus())
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	}
}

bool UShitfireCommsWidget::IsInputFocused() const
{
	return InputBox && InputBox->HasKeyboardFocus();
}

void UShitfireCommsWidget::Flash(const FLinearColor& Color)
{
	FlashColor = Color;
	FlashUntil = NowSeconds + 0.8;
}

FString UShitfireCommsWidget::LineMsgAt(int32 Index) const
{
	return Lines.IsValidIndex(Index) && Lines[Index].IsValid() ? Lines[Index]->Msg : FString();
}

EShitfireLogRole UShitfireCommsWidget::LineRoleAt(int32 Index) const
{
	return Lines.IsValidIndex(Index) && Lines[Index].IsValid()
		? Lines[Index]->Role : EShitfireLogRole::Sys;
}

bool UShitfireCommsWidget::HasRowWidgetFor(int32 Index) const
{
	return LogListView.IsValid() && Lines.IsValidIndex(Index) &&
		LogListView->WidgetFromItem(Lines[Index]).IsValid();
}

void UShitfireCommsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	NowSeconds += InDeltaTime;

	if (OuterBorder && FlashUntil > 0.0)
	{
		const double Left = FlashUntil - NowSeconds;
		if (Left <= 0.0)
		{
			FlashUntil = -1.0;
			OuterBorder->SetBrushColor(ShitfireUi::PanelBorder);
		}
		else
		{
			const float A = static_cast<float>(FMath::Clamp(Left / 0.8, 0.0, 1.0));
			OuterBorder->SetBrushColor(FMath::Lerp(ShitfireUi::PanelBorder, FlashColor, A));
		}
	}
}

FReply UShitfireCommsWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape blurs the radio box (16-input.js:112 txInput.blur()) - handled in
	// preview so the deterministic blur beats SEditableText's own ESC path.
	if (InKeyEvent.GetKey() == EKeys::Escape && IsInputFocused())
	{
		BlurInput();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UShitfireCommsWidget::HandleTextChanged(const FText& Text)
{
	// maxlength=300 (BUILD4, head.html:386-389): no real transmission needs
	// more, and the parser's paste pathology started at ~15 kB.
	const FString S = Text.ToString();
	if (S.Len() > 300)
	{
		InputBox->SetText(FText::FromString(S.Left(300)));
	}
}

void UShitfireCommsWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		// txform submit (16-input.js:101-108): trim, clear, empty = no-op,
		// box KEEPS focus for the next transmission.
		const FString Raw = Text.ToString().TrimStartAndEnd();
		InputBox->SetText(FText::GetEmpty());
		if (!Raw.IsEmpty())
		{
			OnTx.ExecuteIfBound(Raw);
		}
		if (bInputFocused)
		{
			const TSharedPtr<SWidget> S = InputBox->GetCachedWidget();
			if (S.IsValid() && FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetKeyboardFocus(S, EFocusCause::SetDirectly);
			}
		}
	}
	else if (CommitMethod == ETextCommit::OnCleared)
	{
		BlurInput();
	}
	else if (CommitMethod == ETextCommit::OnUserMovedFocus && bInputFocused)
	{
		// clicked away - game input comes back without clearing Slate focus
		// out from under whatever took it
		bInputFocused = false;
		OnFocusChanged.ExecuteIfBound(false);
	}
}
