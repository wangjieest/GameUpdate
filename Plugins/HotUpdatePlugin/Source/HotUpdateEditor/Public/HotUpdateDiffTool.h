// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdateDiffTool.generated.h"

class IAssetRegistry;

/**
 * 资源差异比较工具
 * 用于比较两个版本间的资源差异
 */
UCLASS(BlueprintType)
class HOTUPDATEEDITOR_API UHotUpdateDiffTool : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateDiffTool();

	/**
	 * 比较两个目录的资源差异
	 * @param BaseDirectory 基础版本目录
	 * @param TargetDirectory 目标版本目录
	 * @param bRecursive 是否递归搜索
	 * @return 差异报告
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|Diff")
	FHotUpdateDiffReport CompareDirectories(
		const FString& BaseDirectory,
		const FString& TargetDirectory,
		bool bRecursive = true);

	/**
	 * 比较两个Manifest文件的差异
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|Diff")
	FHotUpdateDiffReport CompareManifests(
		const FString& BaseManifestPath,
		const FString& TargetManifestPath);

	/**
	 * 比较两个Pak文件的内容差异
	 */
	UFUNCTION(BlueprintCallable, Category = "Hot Update|Diff")
	FHotUpdateDiffReport ComparePakFiles(
		const FString& BasePakPath,
		const FString& TargetPakPath);

	/**
	 * 计算目录中所有资源的Hash
	 */
	TMap<FString, FString> ComputeDirectoryHashes(const FString& Directory, bool bRecursive = true);

	/**
	 * 获取资源类型图标名称
	 */
	static FName GetAssetIconName(const FString& AssetPath);

	/**
	 * 获取资源类型显示名称
	 */
	static FText GetAssetTypeDisplayName(const FString& AssetPath);

	/**
	 * 格式化文件大小
	 */
	static FString FormatFileSize(int64 Size);

	/**
	 * 在版本目录中查找 filemanifest.json 文件路径
	 * 查找顺序：1) VersionDir/Windows/filemanifest.json 2) VersionDir/filemanifest.json
	 * 如果找不到 filemanifest.json，回退查找 manifest.json
	 * @param VersionDirectory 版本目录路径
	 * @return 找到的 manifest 文件路径，找不到返回空字符串
	 */
	static FString FindFileManifestPath(const FString& VersionDirectory);

	// 差异比较完成委托
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDiffComplete, const FHotUpdateDiffReport&);
	FOnDiffComplete OnDiffComplete;

private:
	/**
	 * 扫描目录获取所有资源
	 * @param Directory 目录路径
	 * @param bIncludeHiddenFiles 是否包含隐藏文件（传递给 FindFilesRecursive 的 bFindHidden 参数）
	 * @param OutAssets 输出资源映射
	 */
	void ScanDirectory(
		const FString& Directory,
		bool bIncludeHiddenFiles,
		TMap<FString, FHotUpdateAssetDiff>& OutAssets);

	/**
	 * 解析Manifest文件
	 */
	bool ParseManifestFile(
		const FString& ManifestPath,
		TMap<FString, FHotUpdateManifestEntry>& OutEntries);

	/**
	 * 获取Pak文件内容列表
	 */
	TArray<FString> GetPakContentList(const FString& PakPath);

	/**
	 * 从Pak文件中提取文件名到SHA1 Hash的映射
	 */
	TMap<FString, FString> GetPakFileHashes(const FString& PakPath);

	/**
	 * 获取文件扩展名对应的资源类型
	 */
	FString GetAssetTypeFromExtension(const FString& Extension);

private:
	/// 资源注册表
	IAssetRegistry* AssetRegistry;
};