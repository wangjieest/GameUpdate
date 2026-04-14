// Copyright czm. All Rights Reserved.

#include "Widgets/HotUpdatePackagingCallbackHandler.h"
#include "Widgets/HotUpdatePackagingPanel.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdateEditor.h"
#include "Async/Async.h"

void UHotUpdatePackagingCallbackHandler::OnBasePackageComplete(const FHotUpdateBasePackageResult& Result)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("OnBasePackageComplete 回调被触发"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bSuccess: %s"), Result.bSuccess ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  ErrorMessage: %s"), *Result.ErrorMessage);

	// 按值捕获结果结构体，在 GameThread 上使用
	FHotUpdateBasePackageResult ResultCopy = Result;
	AsyncTask(ENamedThreads::GameThread, [this, ResultCopy = MoveTemp(ResultCopy)]()
	{
		// 通知 Panel
		if (TSharedPtr<SHotUpdatePackagingPanel> Panel = OwnerPanel.Pin())
		{
			// 转换为通用结果格式
			FHotUpdatePackageResult CommonResult;
			CommonResult.bSuccess = ResultCopy.bSuccess;
			CommonResult.ErrorMessage = ResultCopy.ErrorMessage;
			CommonResult.VersionString = ResultCopy.VersionString;
			CommonResult.OutputFilePath = ResultCopy.ChunkUtocPaths.Num() > 0 ? ResultCopy.ChunkUtocPaths[0] : TEXT("");
			CommonResult.FileSize = ResultCopy.TotalSize;
			CommonResult.AssetCount = ResultCopy.TotalAssetCount;
			CommonResult.ManifestFilePath = ResultCopy.ManifestFilePath;
			Panel->OnPackagingComplete(CommonResult);
		}
	});
}

void UHotUpdatePackagingCallbackHandler::OnPatchPackageComplete(const FHotUpdatePatchPackageResult& Result)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("OnPatchPackageComplete 回调被触发"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bSuccess: %s"), Result.bSuccess ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  ErrorMessage: %s"), *Result.ErrorMessage);

	// 按值捕获结果结构体，在 GameThread 上使用
	FHotUpdatePatchPackageResult ResultCopy = Result;
	AsyncTask(ENamedThreads::GameThread, [this, ResultCopy = MoveTemp(ResultCopy)]()
	{
		// 通知 Panel
		if (TSharedPtr<SHotUpdatePackagingPanel> Panel = OwnerPanel.Pin())
		{
			// 转换为通用结果格式
			FHotUpdatePackageResult CommonResult;
			CommonResult.bSuccess = ResultCopy.bSuccess;
			CommonResult.ErrorMessage = ResultCopy.ErrorMessage;
			CommonResult.VersionString = ResultCopy.PatchVersion;
			CommonResult.OutputFilePath = ResultCopy.PatchUtocPath;
			CommonResult.FileSize = ResultCopy.PatchSize;
			CommonResult.AssetCount = ResultCopy.ChangedAssetCount;
			CommonResult.ManifestFilePath = ResultCopy.ManifestFilePath;
			Panel->OnPackagingComplete(CommonResult);
		}
	});
}

void UHotUpdatePackagingCallbackHandler::OnPackagingProgress(const FHotUpdatePackageProgress& Progress)
{
	// 按值捕获进度结构体，在 GameThread 上使用
	FHotUpdatePackageProgress ProgressCopy = Progress;
	AsyncTask(ENamedThreads::GameThread, [this, ProgressCopy = MoveTemp(ProgressCopy)]()
	{
		// 通知 Panel
		if (TSharedPtr<SHotUpdatePackagingPanel> Panel = OwnerPanel.Pin())
		{
			Panel->OnPackagingProgress(ProgressCopy);
		}
	});
}