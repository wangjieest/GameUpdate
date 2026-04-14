// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/HotUpdateVersionInfo.h"
#include "Core/HotUpdateVersion.h"
#include "Manifest/HotUpdateManifest.h"
#include "HotUpdateVersionStorage.generated.h"

/**
 * 版本存储管理器
 * 负责本地版本信息和 Manifest 的持久化存储
 */
UCLASS(BlueprintType)
class HOTUPDATE_API UHotUpdateVersionStorage : public UObject
{
	GENERATED_BODY()

public:
	UHotUpdateVersionStorage();

	/**
	 * 初始化存储管理器
	 * @param InStoragePath 存储根目录路径
	 */
	UFUNCTION(BlueprintCallable, Category = "HotUpdate|Storage")
	void Initialize(const FString& InStoragePath);

	// == 版本信息管理 ==

	/**
	 * 加载本地版本信息
	 * @param OutVersion 输出版本信息
	 * @return 是否成功加载
	 */
	bool LoadLocalVersion(FHotUpdateVersionInfo& OutVersion);

	/**
	 * 保存本地版本信息
	 * @param Version 版本信息
	 * @return 是否成功保存
	 */
	bool SaveLocalVersion(const FHotUpdateVersionInfo& Version);

	/**
	 * 获取本地版本历史
	 * @return 版本历史列表（按版本号降序）
	 */
	TArray<FHotUpdateVersionInfo> GetLocalVersionHistory() const;

	// == Manifest 管理 ==

	/**
	 * 加载本地 Manifest 缓存
	 * @param OutManifest 输出 Manifest
	 * @return 是否成功加载
	 */
	bool LoadLocalManifest(FHotUpdateManifest& OutManifest);

	/**
	 * 保存 Manifest 到本地缓存
	 * @param Manifest Manifest 数据
	 * @return 是否成功保存
	 */
	bool SaveLocalManifest(const FHotUpdateManifest& Manifest);

	// == 路径获取 ==

	/** 获取版本文件路径 */
	FString GetVersionFilePath() const { return StoragePath / TEXT("version.json"); }

	/** 获取 Manifest 文件路径 */
	FString GetManifestFilePath() const { return StoragePath / TEXT("manifest.json"); }

	/** 获取指定版本的 Pak 目录路径 */
	FString GetVersionPakPath(const FHotUpdateVersionInfo& Version) const { return StoragePath / Version.ToString(); }

	/** 获取存储根目录 */
	const FString& GetStoragePath() const { return StoragePath; }

private:
	/** 存储根目录 */
	UPROPERTY(Transient)
	FString StoragePath;
};