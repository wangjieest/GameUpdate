// Copyright czm. All Rights Reserved.

#include "HotUpdateEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "EditorStyleSet.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

// ===== 颜色常量定义 =====

// 状态颜色
const FLinearColor FHotUpdateEditorStyle::FStatusColors::Success = FLinearColor(0.25f, 0.75f, 0.35f);
const FLinearColor FHotUpdateEditorStyle::FStatusColors::Warning = FLinearColor(0.95f, 0.7f, 0.15f);
const FLinearColor FHotUpdateEditorStyle::FStatusColors::Error = FLinearColor(0.9f, 0.25f, 0.2f);
const FLinearColor FHotUpdateEditorStyle::FStatusColors::Processing = FLinearColor(0.35f, 0.6f, 0.95f);
const FLinearColor FHotUpdateEditorStyle::FStatusColors::Cancelled = FLinearColor(0.55f, 0.55f, 0.55f);

// 差异颜色
const FLinearColor FHotUpdateEditorStyle::FDiffColors::Added = FLinearColor(0.25f, 0.75f, 0.35f);
const FLinearColor FHotUpdateEditorStyle::FDiffColors::Modified = FLinearColor(0.85f, 0.6f, 0.2f);
const FLinearColor FHotUpdateEditorStyle::FDiffColors::Deleted = FLinearColor(0.85f, 0.25f, 0.25f);
const FLinearColor FHotUpdateEditorStyle::FDiffColors::Unchanged = FLinearColor(0.55f, 0.55f, 0.55f);

// 主题颜色
const FLinearColor FHotUpdateEditorStyle::FThemeColors::Primary = FLinearColor(0.25f, 0.55f, 0.85f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::PrimaryLight = FLinearColor(0.45f, 0.7f, 0.95f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::PrimaryDark = FLinearColor(0.15f, 0.35f, 0.55f);

const FLinearColor FHotUpdateEditorStyle::FThemeColors::Background = FLinearColor(0.04f, 0.04f, 0.04f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::BackgroundDark = FLinearColor(0.02f, 0.02f, 0.02f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::BackgroundLight = FLinearColor(0.06f, 0.06f, 0.06f);

const FLinearColor FHotUpdateEditorStyle::FThemeColors::Border = FLinearColor(0.1f, 0.1f, 0.1f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::BorderHighlight = FLinearColor(0.15f, 0.15f, 0.15f);

const FLinearColor FHotUpdateEditorStyle::FThemeColors::TextPrimary = FLinearColor(0.95f, 0.95f, 0.95f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::TextSecondary = FLinearColor(0.7f, 0.7f, 0.7f);
const FLinearColor FHotUpdateEditorStyle::FThemeColors::TextDisabled = FLinearColor(0.4f, 0.4f, 0.4f);

TSharedPtr<FSlateStyleSet> FHotUpdateEditorStyle::StyleSet = nullptr;

FName FHotUpdateEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("HotUpdateEditorStyle"));
	return StyleSetName;
}

void FHotUpdateEditorStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FHotUpdateEditorStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

const FSlateBrush* FHotUpdateEditorStyle::GetFolderIcon()
{
	return FAppStyle::GetBrush("FolderIcon");
}

const FSlateBrush* FHotUpdateEditorStyle::GetAssetIcon(const FString& AssetPath)
{
	FString Extension = FPaths::GetExtension(AssetPath);

	if (Extension == TEXT("uasset"))
	{
		if (AssetPath.Contains(TEXT("Texture2D")) || AssetPath.Contains(TEXT("_T_")))
		{
			return FAppStyle::GetBrush("ClassIcon.Texture2D");
		}
		else if (AssetPath.Contains(TEXT("Material")) || AssetPath.Contains(TEXT("_M_")))
		{
			return FAppStyle::GetBrush("ClassIcon.Material");
		}
		else if (AssetPath.Contains(TEXT("StaticMesh")) || AssetPath.Contains(TEXT("_SM_")))
		{
			return FAppStyle::GetBrush("ClassIcon.StaticMesh");
		}
		else if (AssetPath.Contains(TEXT("SkeletalMesh")) || AssetPath.Contains(TEXT("_SK_")))
		{
			return FAppStyle::GetBrush("ClassIcon.SkeletalMesh");
		}
		else if (AssetPath.Contains(TEXT("Blueprint")) || AssetPath.Contains(TEXT("_BP_")))
		{
			return FAppStyle::GetBrush("ClassIcon.Blueprint");
		}
		else if (AssetPath.Contains(TEXT("WidgetBlueprint")) || AssetPath.Contains(TEXT("_WBP_")))
		{
			return FAppStyle::GetBrush("ClassIcon.WidgetBlueprint");
		}
		else if (AssetPath.Contains(TEXT("SoundWave")) || AssetPath.Contains(TEXT("_S_")))
		{
			return FAppStyle::GetBrush("ClassIcon.SoundWave");
		}
		else if (AssetPath.Contains(TEXT("Level")) || AssetPath.Contains(TEXT("_L_")))
		{
			return FAppStyle::GetBrush("ClassIcon.World");
		}
	}

	return FAppStyle::GetBrush("ClassIcon.Object");
}

const FSlateBrush* FHotUpdateEditorStyle::GetStatusIcon(bool bSuccess)
{
	if (bSuccess)
	{
		return FAppStyle::GetBrush("Icons.SuccessWithColor");
	}
	return FAppStyle::GetBrush("Icons.ErrorWithColor");
}

const FSlateBrush* FHotUpdateEditorStyle::GetGroupBorder()
{
	return FAppStyle::GetBrush("ToolPanel.GroupBorder");
}

const FSlateBrush* FHotUpdateEditorStyle::GetDarkGroupBorder()
{
	return FAppStyle::GetBrush("ToolPanel.DarkGroupBorder");
}

const FSlateBrush* FHotUpdateEditorStyle::GetHighlightBorder()
{
	return FAppStyle::GetBrush("Border.Highlight");
}

FSlateFontInfo FHotUpdateEditorStyle::GetTitleFont()
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 16);
}

FSlateFontInfo FHotUpdateEditorStyle::GetSubtitleFont()
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 13);
}

FSlateFontInfo FHotUpdateEditorStyle::GetNormalFont()
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 10);
}

FSlateFontInfo FHotUpdateEditorStyle::GetSmallFont()
{
	return FCoreStyle::GetDefaultFontStyle("Regular", 9);
}

FSlateFontInfo FHotUpdateEditorStyle::GetBoldFont()
{
	return FCoreStyle::GetDefaultFontStyle("Bold", 10);
}

TSharedRef<SWidget> FHotUpdateEditorStyle::CreateSettingRow(
	const FText& Label,
	TSharedRef<SWidget> ValueWidget,
	float LabelWidth)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 4)
		[
			SNew(SBox)
			.WidthOverride(LabelWidth)
			[
				SNew(STextBlock)
				.Text(Label)
				.ColorAndOpacity(FThemeColors::TextSecondary)
				.Font(GetNormalFont())
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(8, 4)
		.VAlign(VAlign_Center)
		[
			ValueWidget
		];
}

TSharedRef<SWidget> FHotUpdateEditorStyle::CreateSectionHeader(
	const FText& Title,
	const FText& Tooltip)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Title)
			.ToolTipText(Tooltip)
			.Font(GetSubtitleFont())
			.ColorAndOpacity(FThemeColors::TextPrimary)
		];
}

TSharedRef<SWidget> FHotUpdateEditorStyle::CreateSeparator(float Thickness, float Padding)
{
	return SNew(SBox)
		.Padding(FMargin(0, Padding))
		[
			SNew(SSeparator)
			.Thickness(Thickness)
			.Orientation(Orient_Horizontal)
		];
}