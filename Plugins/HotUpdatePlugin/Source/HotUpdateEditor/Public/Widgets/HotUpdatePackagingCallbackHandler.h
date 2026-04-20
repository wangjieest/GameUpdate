// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdatePackagingCallbackHandler.generated.h"

/** 打包完成回调委托 */
DECLARE_DELEGATE_OneParam(FOnPackagingPanelComplete, const FHotUpdatePackageResult&);

/** 打包进度回调委托 */
DECLARE_DELEGATE_OneParam(FOnPackagingPanelProgress, const FHotUpdatePackageProgress&);

/** 自定义打包完成回调委托 */
DECLARE_DELEGATE_OneParam(FOnCustomPackagingPanelComplete, const FHotUpdatePackageResult&);

/**
 * 打包回调处理器 - 用于接收动态委托回调
 * 通过委托模式支持多种面板类型
 */
UCLASS()
class HOTUPDATEEDITOR_API UHotUpdatePackagingCallbackHandler : public UObject
{
	GENERATED_BODY()
public:
	/** 打包完成委托 */
	FOnPackagingPanelComplete OnCompleteDelegate;

	/** 打包进度委托 */
	FOnPackagingPanelProgress OnProgressDelegate;

	/** 自定义打包完成委托 */
	FOnCustomPackagingPanelComplete OnCustomCompleteDelegate;

	// 更新包完成回调
	UFUNCTION()
	void OnPatchPackageComplete(const FHotUpdatePatchPackageResult& Result);

	// 自定义打包完成回调
	UFUNCTION()
	void OnCustomPackageComplete(const FHotUpdateCustomPackageResult& Result);

	// 进度回调（通用）
	UFUNCTION()
	void OnPackagingProgress(const FHotUpdatePackageProgress& Progress);
};