// Copyright czm. All Rights Reserved.

#include "HotUpdateAssetManager.h"
#include "HotUpdate.h"
#include "Core/HotUpdateSettings.h"
#include "Core/HotUpdateFileUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"

/** 最小包配置缓存结构 */
struct FMinimalPackageConfigCache
{
	bool bEnabled = false;
	TMap<FString, int32> ChunkMapping;
	FDateTime FileTimestamp = FDateTime(0);
};

/** 最小包配置文件路径 */
static FString GetMinimalPackageConfigFilePath()
{
	return FPaths::ProjectIntermediateDir() / TEXT("MinimalPackageConfig.json");
}

/**
 * 加载最小包配置（带缓存）
 * @param OutConfig 输出配置结构
 * @return true 如果配置文件存在且已加载
 */
static bool LoadMinimalPackageConfig(FMinimalPackageConfigCache& OutConfig)
{
	static FMinimalPackageConfigCache CachedConfig;

	FString ConfigFile = GetMinimalPackageConfigFilePath();

	// 配置文件不存在，使用引擎默认
	if (!FPaths::FileExists(ConfigFile))
	{
		CachedConfig.bEnabled = false;
		CachedConfig.ChunkMapping.Empty();
		CachedConfig.FileTimestamp = FDateTime(0);
		OutConfig = CachedConfig;
		return false;
	}

	// 检查是否需要重新加载
	FFileStatData StatData = IFileManager::Get().GetStatData(*ConfigFile);
	if (StatData.bIsValid && StatData.ModificationTime == CachedConfig.FileTimestamp)
	{
		// 缓存有效，直接返回
		OutConfig = CachedConfig;
		return true;
	}

	// 需要重新加载
	CachedConfig.FileTimestamp = StatData.ModificationTime;
	CachedConfig.ChunkMapping.Empty();

	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *ConfigFile))
	{
		CachedConfig.bEnabled = false;
		OutConfig = CachedConfig;
		return false;
	}

	UE_LOG(LogHotUpdate, Log, TEXT("LoadMinimalPackageConfig: Loading from %s"), *ConfigFile);

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		CachedConfig.bEnabled = false;
		OutConfig = CachedConfig;
		return false;
	}

	CachedConfig.bEnabled = JsonObj->GetBoolField(TEXT("bEnableMinimalPackage"));

	// 读取 ChunkMapping（包含所有资产的 Chunk 分配，Chunk 0 为首包）
	const TSharedPtr<FJsonObject>* MappingObj;
	if (JsonObj->TryGetObjectField(TEXT("ChunkMapping"), MappingObj))
	{
		for (const auto& Pair : (*MappingObj)->Values)
		{
			CachedConfig.ChunkMapping.Add(Pair.Key, (int32)Pair.Value->AsNumber());
		}
		UE_LOG(LogHotUpdate, Log, TEXT("LoadMinimalPackageConfig: Loaded %d ChunkMapping entries"), CachedConfig.ChunkMapping.Num());
	}

	UE_LOG(LogHotUpdate, Log, TEXT("LoadMinimalPackageConfig: Enabled=%s, ChunkMapping=%d"),
		CachedConfig.bEnabled ? TEXT("True") : TEXT("False"),
		CachedConfig.ChunkMapping.Num());

	OutConfig = CachedConfig;
	return true;
}

UHotUpdateAssetManager::UHotUpdateAssetManager()
{
	UE_LOG(LogHotUpdate, Log, TEXT("UHotUpdateAssetManager constructed"));
}

#if WITH_EDITOR
bool UHotUpdateAssetManager::GetPackageChunkIds(
	FName PackageName, const ITargetPlatform* TargetPlatform,
	TArrayView<const int32> ExistingChunkList, TArray<int32>& OutChunkList,
	TArray<int32>* OutOverrideChunkList) const
{
	// 获取引擎默认 Chunk 分配
	bool bHasChunkIds = Super::GetPackageChunkIds(PackageName, TargetPlatform, ExistingChunkList, OutChunkList, OutOverrideChunkList);

	// 加载最小包配置
	FMinimalPackageConfigCache Config;
	LoadMinimalPackageConfig(Config);

	// 最小包模式未启用，返回引擎默认
	if (!Config.bEnabled)
	{
		return bHasChunkIds;
	}

	FString PackageStr = PackageName.ToString();

	// 引擎资源始终分配到 Chunk 0（首包）
	if (UHotUpdateFileUtils::IsEngineAsset(PackageStr))
	{
		OutChunkList.Empty();
		OutChunkList.Add(0);
		return true;
	}

	// 查找 ChunkMapping
	const int32* MappedChunkId = Config.ChunkMapping.Find(PackageStr);
	if (MappedChunkId)
	{
		OutChunkList.Empty();
		OutChunkList.Add(*MappedChunkId);
		return true;
	}

	// 未找到映射，分配到 Chunk 11（热更包）
	OutChunkList.Empty();
	OutChunkList.Add(11);
	return true;
}

TMap<int32, FAssetManagerChunkInfo> UHotUpdateAssetManager::BuildChunkMap(const TSet<FName>& PackagesToUpdateChunksFor,
	const TSet<FName>& StartupPackages, const TSet<FName>& PackageThatWereCooked) const
{
	UE_LOG(LogHotUpdate, Log, TEXT("BuildChunkMap called:"));
	UE_LOG(LogHotUpdate, Log, TEXT("  PackagesToUpdateChunksFor: %d items"), PackagesToUpdateChunksFor.Num());
	UE_LOG(LogHotUpdate, Log, TEXT("  StartupPackages: %d items"), StartupPackages.Num());
	UE_LOG(LogHotUpdate, Log, TEXT("  PackageThatWereCooked: %d items"), PackageThatWereCooked.Num());

	// 输出 PackagesToUpdateChunksFor 内容
	if (PackagesToUpdateChunksFor.Num() > 0)
	{
		UE_LOG(LogHotUpdate, Log, TEXT("  PackagesToUpdateChunksFor items:"));
		for (const FName& Package : PackagesToUpdateChunksFor)
		{
			UE_LOG(LogHotUpdate, Log, TEXT("    - %s"), *Package.ToString());
		}
	}

	// 输出 StartupPackages 内容
	if (StartupPackages.Num() > 0)
	{
		UE_LOG(LogHotUpdate, Log, TEXT("  StartupPackages items:"));
		for (const FName& Package : StartupPackages)
		{
			UE_LOG(LogHotUpdate, Log, TEXT("    - %s"), *Package.ToString());
		}
	}

	// 输出 PackageThatWereCooked 前 20 个作为样本
	if (PackageThatWereCooked.Num() > 0)
	{
		UE_LOG(LogHotUpdate, Log, TEXT("  PackageThatWereCooked sample (first 20):"));
		int32 Count = 0;
		for (const FName& Package : PackageThatWereCooked)
		{
			if (Count >= 20) break;
			UE_LOG(LogHotUpdate, Log, TEXT("    - %s"), *Package.ToString());
			Count++;
		}
		if (PackageThatWereCooked.Num() > 20)
		{
			UE_LOG(LogHotUpdate, Log, TEXT("    ... (%d more items)"), PackageThatWereCooked.Num() - 20);
		}
	}

	return Super::BuildChunkMap(PackagesToUpdateChunksFor, StartupPackages, PackageThatWereCooked);
}
#endif
