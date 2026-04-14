// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"
#include "HotUpdateEditorTypes.h"

class SHotUpdateDiffTreeItem;

/**
 * 差异树节点数据
 */
struct FDiffTreeNode
{
	FString Name;
	FString FullPath;
	bool bIsFolder;
	EHotUpdateFileChangeType ChangeType;
	TSharedPtr<FDiffTreeNode> Parent;
	TArray<TSharedPtr<FDiffTreeNode>> Children;
	FHotUpdateAssetDiff DiffInfo;

	FDiffTreeNode()
		: bIsFolder(false)
		, ChangeType(EHotUpdateFileChangeType::Unchanged)
	{}

	bool HasChildren() const { return Children.Num() > 0; }
};

/**
 * 差异树节点行控件
 */
class SHotUpdateDiffTreeItem : public SMultiColumnTableRow<TSharedPtr<FDiffTreeNode>>
{
public:
	SLATE_BEGIN_ARGS(SHotUpdateDiffTreeItem) {}
		SLATE_ARGUMENT(TSharedPtr<FDiffTreeNode>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FDiffTreeNode> Item;

	const FSlateBrush* GetIcon() const;
	FSlateColor GetColorAndOpacity() const;
	FText GetDisplayText() const;
	FText GetChangeTypeText() const;
	FSlateColor GetChangeTypeColor() const;
	FText GetSizeText() const;
};