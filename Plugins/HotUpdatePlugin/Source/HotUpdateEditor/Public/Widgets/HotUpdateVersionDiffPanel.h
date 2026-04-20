// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdateVersionDiffTypes.h"

/**
 * 版本差异比较面板
 */
class HOTUPDATEEDITOR_API SHotUpdateVersionDiffPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHotUpdateVersionDiffPanel) {}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 设置差异报告 */
	void SetDiffReport(const FHotUpdateDiffReport& InReport);

	/** 设置基础版本路径 */
	void SetBaseVersionPath(const FString& Path) { BaseVersionPath = Path; }

	/** 设置目标版本路径 */
	void SetTargetVersionPath(const FString& Path) { TargetVersionPath = Path; }

private:
	/** 创建工具栏 */
	TSharedRef<SWidget> CreateToolbar();

	/** 创建目录选择区域 */
	TSharedRef<SWidget> CreateDirectorySelector();

	/** 创建差异树视图 */
	TSharedRef<SWidget> CreateDiffTreeView();

	/** 创建详情面板 */
	TSharedRef<SWidget> CreateDetailsPanel();

	/** 创建统计信息面板 */
	TSharedRef<SWidget> CreateStatisticsPanel();

	/** 加载版本按钮回调 */
	FReply OnLoadVersionsClicked();

	/** 刷新按钮回调 */
	FReply OnRefreshClicked();

	/** 导出报告按钮回调 */
	FReply OnExportReportClicked();

	/** 选择目录按钮回调 */
	FReply OnSelectBaseDirectoryClicked();
	FReply OnSelectTargetDirectoryClicked();

	/** 生成树节点 */
	void GenerateTreeNodes();

	/** 获取树节点图标 */
	const FSlateBrush* GetTreeNodeIcon(TSharedPtr<FDiffTreeNode> Node) const;

	/** 获取树节点文字颜色 */
	FSlateColor GetTreeNodeColor(TSharedPtr<FDiffTreeNode> Node) const;

	/** 树视图相关 */
	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FDiffTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetTreeChildren(TSharedPtr<FDiffTreeNode> InParent, TArray<TSharedPtr<FDiffTreeNode>>& OutChildren);
	void OnTreeSelectionChanged(TSharedPtr<FDiffTreeNode> SelectedItem, ESelectInfo::Type SelectInfo);
	void OnTreeViewDoubleClick(TSharedPtr<FDiffTreeNode> Item);

	/** 打开选中的资源 */
	void OpenSelectedAsset(TSharedPtr<FDiffTreeNode> Node);

	/** 更新详情面板 */
	void UpdateDetailsPanel(TSharedPtr<FDiffTreeNode> Node);

	/** 过滤树视图 */
	void ApplyFilter(const FString& FilterText);

private:
	/** 父窗口 */
	TSharedPtr<SWindow> ParentWindow;

	/** 差异报告 */
	FHotUpdateDiffReport DiffReport;

	/** 基础版本路径 */
	FString BaseVersionPath;

	/** 目标版本路径 */
	FString TargetVersionPath;

	/** 树视图 */
	TSharedPtr<STreeView<TSharedPtr<FDiffTreeNode>>> DiffTreeView;

	/** 树根节点 */
	TArray<TSharedPtr<FDiffTreeNode>> RootNodes;

	/** 所有节点缓存 */
	TMap<FString, TSharedPtr<FDiffTreeNode>> AllNodes;

	/** 当前选中的节点 */
	TSharedPtr<FDiffTreeNode> SelectedNode;

	/** 详情面板容器 */
	TSharedPtr<SVerticalBox> DetailsBox;

	/** 统计文本 */
	TSharedPtr<STextBlock> StatisticsText;

	/** 过滤文本框 */
	TSharedPtr<SEditableText> FilterTextBox;

	/** 基础版本路径输入框 */
	TSharedPtr<SEditableText> BasePathTextBox;

	/** 目标版本路径输入框 */
	TSharedPtr<SEditableText> TargetPathTextBox;

	/** 当前过滤文本 */
	FString CurrentFilter;

	/** 是否已加载 */
	bool bIsLoaded;
};