// Copyright czm. All Rights Reserved.

#include "Widgets/HotUpdatePackagingCallbackHandler.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdateEditor.h"
#include "Async/Async.h"


void UHotUpdatePackagingCallbackHandler::OnPatchPackageComplete(const FHotUpdatePatchPackageResult& Result)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("OnPatchPackageComplete 回调被触发"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bSuccess: %s"), Result.bSuccess ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  ErrorMessage: %s"), *Result.ErrorMessage);

	FHotUpdatePatchPackageResult ResultCopy = Result;
	AsyncTask(ENamedThreads::GameThread, [this, ResultCopy = MoveTemp(ResultCopy)]()
	{
		FHotUpdatePackageResult CommonResult;
		CommonResult.bSuccess = ResultCopy.bSuccess;
		CommonResult.ErrorMessage = ResultCopy.ErrorMessage;
		CommonResult.VersionString = ResultCopy.PatchVersion;
		CommonResult.OutputFilePath = ResultCopy.PatchUtocPath;
		CommonResult.FileSize = ResultCopy.PatchSize;
		CommonResult.AssetCount = ResultCopy.ChangedAssetCount;
		CommonResult.ManifestFilePath = ResultCopy.ManifestFilePath;

		if (OnCompleteDelegate.IsBound())
		{
			OnCompleteDelegate.Execute(CommonResult);
		}
	});
}

void UHotUpdatePackagingCallbackHandler::OnCustomPackageComplete(const FHotUpdateCustomPackageResult& Result)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("OnCustomPackageComplete 回调被触发"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bSuccess: %s"), Result.bSuccess ? TEXT("true") : TEXT("false"));

	FHotUpdateCustomPackageResult ResultCopy = Result;
	AsyncTask(ENamedThreads::GameThread, [this, ResultCopy = MoveTemp(ResultCopy)]()
	{
		FHotUpdatePackageResult CommonResult;
		CommonResult.bSuccess = ResultCopy.bSuccess;
		CommonResult.ErrorMessage = ResultCopy.ErrorMessage;
		CommonResult.VersionString = ResultCopy.PatchVersion;
		CommonResult.OutputFilePath = ResultCopy.PatchUtocPath;
		CommonResult.FileSize = ResultCopy.PatchSize;
		CommonResult.AssetCount = ResultCopy.AssetCount;
		CommonResult.ManifestFilePath = ResultCopy.ManifestFilePath;

		if (OnCustomCompleteDelegate.IsBound())
		{
			OnCustomCompleteDelegate.Execute(CommonResult);
		}
	});
}

void UHotUpdatePackagingCallbackHandler::OnPackagingProgress(const FHotUpdatePackageProgress& Progress)
{
	FHotUpdatePackageProgress ProgressCopy = Progress;
	AsyncTask(ENamedThreads::GameThread, [this, ProgressCopy = MoveTemp(ProgressCopy)]()
	{
		if (OnProgressDelegate.IsBound())
		{
			OnProgressDelegate.Execute(ProgressCopy);
		}
	});
}