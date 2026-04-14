// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/HotUpdateTypes.h"
#include "Core/HotUpdateVersion.h"
#include "Core/HotUpdateFileInfo.h"
#include "Core/HotUpdateContainerTypes.h"
#include "Manifest/HotUpdateManifest.h"
#include "HotUpdateIncrementalCalculator.generated.h"

/**
 * 增量下载计算器
 * 负责对比服务器和本地 Manifest，计算需要下载的文件和容器
 */
UCLASS(BlueprintType)
class HOTUPDATE_API UHotUpdateIncrementalCalculator : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateIncrementalCalculator();

	/**
	 * 计算增量下载列表
	 * @param ServerManifest 服务器 Manifest
	 * @param LocalManifest 本地 Manifest
	 * @param StoragePath 本地存储路径
	 * @param CurrentVersion 当前版本
	 * @param LatestVersion 最新版本
	 * @param OutResult 输出结果
	 */
	void CalculateIncrementalDownload(
		const FHotUpdateManifest& ServerManifest,
		const FHotUpdateManifest& LocalManifest,
		const FString& StoragePath,
		const FHotUpdateVersionInfo& CurrentVersion,
		const FHotUpdateVersionInfo& LatestVersion,
		FHotUpdateVersionCheckResult& OutResult);

	/**
	 * 检查本地文件是否存在且 Hash 匹配
	 * @param StoragePath 存储路径
	 * @param RelativePath 文件相对路径
	 * @param ExpectedHash 期望的 Hash
	 * @param ExpectedSize 期望的大小
	 * @param CurrentVersion 当前版本
	 * @param LatestVersion 最新版本
	 * @return 文件是否有效
	 */
	bool IsLocalFileValid(
		const FString& StoragePath,
		const FString& RelativePath,
		const FString& ExpectedHash,
		int64 ExpectedSize,
		const FHotUpdateVersionInfo& CurrentVersion,
		const FHotUpdateVersionInfo& LatestVersion) const;

private:
	/**
	 * 计算单个文件的 Hash
	 * @param FilePath 文件路径
	 * @return SHA1 Hash 字符串
	 */
	static FString CalculateFileHash(const FString& FilePath);
};