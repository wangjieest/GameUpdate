// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateImageBrush.h"

struct FSlateBrush;
class FSlateStyleSet;

/**
 * 热更新编辑器样式定义
 * 参考 UE5 官方编辑器 FAppStyle 的实现方式
 */
class HOTUPDATEEDITOR_API FHotUpdateEditorStyle
{
public:
	/** 初始化样式 */
	static void Initialize();

	/** 清理样式 */
	static void Shutdown();

	/** 获取样式名称 */
	static FName GetStyleSetName();

	// ===== 颜色常量 =====

	/** 状态颜色 */
	struct FStatusColors
	{
		static const FLinearColor Success;
		static const FLinearColor Warning;
		static const FLinearColor Error;
		static const FLinearColor Processing;
		static const FLinearColor Cancelled;
	};

	/** 差异颜色 */
	struct FDiffColors
	{
		static const FLinearColor Added;
		static const FLinearColor Modified;
		static const FLinearColor Deleted;
		static const FLinearColor Unchanged;
	};

	/** UI 主题颜色 */
	struct FThemeColors
	{
		// 主色调
		static const FLinearColor Primary;        // 主强调色
		static const FLinearColor PrimaryLight;   // 浅主色
		static const FLinearColor PrimaryDark;    // 深主色

		// 背景色
		static const FLinearColor Background;     // 主背景
		static const FLinearColor BackgroundDark; // 深色背景
		static const FLinearColor BackgroundLight;// 浅色背景

		// 边框色
		static const FLinearColor Border;         // 普通边框
		static const FLinearColor BorderHighlight;// 高亮边框

		// 文字色
		static const FLinearColor TextPrimary;    // 主要文字
		static const FLinearColor TextSecondary;  // 次要文字
		static const FLinearColor TextDisabled;   // 禁用文字
	};

	// ===== Slate 颜色获取 =====

	static FSlateColor GetSuccessColor() { return FStatusColors::Success; }
	static FSlateColor GetWarningColor() { return FStatusColors::Warning; }
	static FSlateColor GetErrorColor() { return FStatusColors::Error; }
	static FSlateColor GetProcessingColor() { return FStatusColors::Processing; }

	static FSlateColor GetAddedColor() { return FDiffColors::Added; }
	static FSlateColor GetModifiedColor() { return FDiffColors::Modified; }
	static FSlateColor GetDeletedColor() { return FDiffColors::Deleted; }

	// 主题颜色获取
	static FSlateColor GetPrimaryColor() { return FThemeColors::Primary; }
	static FSlateColor GetBackgroundColor() { return FThemeColors::Background; }
	static FSlateColor GetBackgroundDarkColor() { return FThemeColors::BackgroundDark; }
	static FSlateColor GetTextPrimaryColor() { return FThemeColors::TextPrimary; }
	static FSlateColor GetTextSecondaryColor() { return FThemeColors::TextSecondary; }

	// ===== 常用画刷获取 =====

	/** 获取文件夹图标 */
	static const FSlateBrush* GetFolderIcon();

	/** 获取资源图标 */
	static const FSlateBrush* GetAssetIcon(const FString& AssetPath);

	/** 获取状态图标 */
	static const FSlateBrush* GetStatusIcon(bool bSuccess);

	// ===== 边框画刷 =====

	/** 获取分组边框 */
	static const FSlateBrush* GetGroupBorder();

	/** 获取深色分组边框 */
	static const FSlateBrush* GetDarkGroupBorder();

	/** 获取高亮边框 */
	static const FSlateBrush* GetHighlightBorder();

	// ===== 字体获取 =====

	/** 获取标题字体 (大) */
	static FSlateFontInfo GetTitleFont();

	/** 获取子标题字体 (中) */
	static FSlateFontInfo GetSubtitleFont();

	/** 获取正文字体 */
	static FSlateFontInfo GetNormalFont();

	/** 获取小字体 */
	static FSlateFontInfo GetSmallFont();

	/** 获取粗体正文字体 */
	static FSlateFontInfo GetBoldFont();

	// ===== 样式辅助方法 =====

	/** 创建标准设置行 */
	static TSharedRef<class SWidget> CreateSettingRow(
		const FText& Label,
		TSharedRef<SWidget> ValueWidget,
		float LabelWidth = 100.0f
	);

	/** 创建分组标题 */
	static TSharedRef<class SWidget> CreateSectionHeader(
		const FText& Title,
		const FText& Tooltip = FText::GetEmpty()
	);

	/** 创建分隔线 */
	static TSharedRef<class SWidget> CreateSeparator(
		float Thickness = 1.0f,
		float Padding = 8.0f
	);

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};