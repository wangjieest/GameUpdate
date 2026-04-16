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
 * 负责对比服务器和本地 Manifest，计算需要下载的 Container
 *
 * 注意：运行时热更新是基于 Container（.utoc/.ucas）级别的，
 * 服务端 manifest.json 只包含 version 和 chunks 信息，
 * 不包含 files 列表（files 在 filemanifest.json 中，仅供编辑器端差异计算使用）
 */
UCLASS(BlueprintType)
class HOTUPDATE_API UHotUpdateIncrementalCalculator : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateIncrementalCalculator();

	/**
	 * 计算增量下载列表（Container 级对比）
	 * @param ServerManifest 服务器 Manifest
	 * @param LocalManifest 本地 Manifest
	 * @param OutResult 输出结果
	 */
	void CalculateIncrementalDownload(
		const FHotUpdateManifest& ServerManifest,
		const FHotUpdateManifest& LocalManifest,
		FHotUpdateVersionCheckResult& OutResult);
};