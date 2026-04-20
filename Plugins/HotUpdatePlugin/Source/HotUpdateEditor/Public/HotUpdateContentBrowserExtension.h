// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"

class FHotUpdateContentBrowserExtension
{
public:
	/** 注册Content Browser扩展 */
	static void Register();

	/** 注销Content Browser扩展 */
	static void Unregister();

private:
	/** 执行热更新打包（接收 UE 包路径，内部转换为磁盘路径） */
	static void ExecuteHotUpdatePackage(TArray<FString> PackagePaths);

	/** 获取选中的资源路径 */
	static TArray<FString> GetSelectedAssetPaths(const TArray<struct FAssetData>& SelectedAssets);
};