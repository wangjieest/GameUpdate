// Copyright czm. All Rights Reserved.

#include "HotUpdatePatchPackageBuilder.h"
#include "HotUpdatePackageHelper.h"
#include "Core/HotUpdateFileUtils.h"
#include "HotUpdateEditor.h"
#include "HotUpdateIoStoreBuilder.h"
#include "HotUpdateUtils.h"
#include "HotUpdatePackagingSettingsHelper.h"
#include "HotUpdateVersionManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"

FHotUpdatePatchPackageBuilder::FHotUpdatePatchPackageBuilder()
	: bIsBuilding(false)
	, bIsCancelled(false)
{
}

FHotUpdatePatchPackageResult FHotUpdatePatchPackageBuilder::BuildPatchPackage(const FHotUpdatePatchPackageConfig& Config)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("BuildPatchPackage (同步) 开始调用"));
	FHotUpdatePatchPackageResult Result;

	// 验证配置
	FString ErrorMessage;
	CurrentConfig = Config;
	if (!ValidateConfig(Config, ErrorMessage))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("配置验证失败: %s"), *ErrorMessage);
		Result.bSuccess = false;
		Result.ErrorMessage = ErrorMessage;
		return Result;
	}
	
	bIsCancelled = false;

	// 编译项目：确保 Cook 使用最新的游戏代码
	if (!CurrentConfig.bSkipBuild)
	{
		UpdateProgress(TEXT("编译项目"), TEXT(""), 0, 0);
		if (!FHotUpdatePackageHelper::CompileProject(CurrentConfig.Platform))
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("项目编译失败");
			bIsBuilding = false;
			return Result;
		}
	}
	else
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("跳过编译步骤 (bSkipBuild = true)"));
	}

	// Cook 资源
	if (!CurrentConfig.bSkipCook)
	{
		if (CurrentConfig.bIncrementalCook)
		{
			// 增量 Cook 模式：Cook 移到 Diff 之后，只 Cook 变化的资源
			UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook 模式: Cook 将在 Diff 之后执行"));
		}
		else
		{
			// 全量 Cook 模式：先 Cook 再 Diff
			UpdateProgress(TEXT("Cook 资源"), TEXT(""), 0, 0);
			if (!FHotUpdatePackageHelper::CookAssets(CurrentConfig.Platform))
			{
				Result.bSuccess = false;
				Result.ErrorMessage = TEXT("Cook 资源失败");
				bIsBuilding = false;
				return Result;
			}
		}
	}
	else
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("跳过 Cook 步骤 (bSkipCook = true)"));
	}

	// === 热更新差异打包流程 ===

	// 1. 加载基础版本 FileManifest
	UpdateProgress(TEXT("加载基础版本"), TEXT(""), 0, 0);

	TMap<FString, FString> BaseAssetHashes;
	TMap<FString, int64> BaseAssetSizes;

	if (!LoadBaseFileManifest(CurrentConfig.BaseFileManifestPath.FilePath, BaseAssetHashes, BaseAssetSizes))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("无法加载基础版本 FileManifest");
		bIsBuilding = false;
		return Result;
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("加载了基础版本 %d 个资源"), BaseAssetHashes.Num());

	// 从 manifest 中读取实际版本号作为 BaseVersion
	FString ActualBaseVersion = CurrentConfig.BaseVersion;
	FString ManifestContent;
	if (FFileHelper::LoadFileToString(ManifestContent, *CurrentConfig.BaseFileManifestPath.FilePath))
	{
		TSharedPtr<FJsonObject> ManifestObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestContent);
		if (FJsonSerializer::Deserialize(Reader, ManifestObj) && ManifestObj.IsValid())
		{
			const TSharedPtr<FJsonObject>* VersionObj;
			if (ManifestObj->TryGetObjectField(TEXT("version"), VersionObj))
			{
				FString ManifestVersion;
				if (VersionObj->Get()->TryGetStringField(TEXT("version"), ManifestVersion))
				{
					ActualBaseVersion = ManifestVersion;
					UE_LOG(LogHotUpdateEditor, Log, TEXT("从 Manifest 更新 BaseVersion 为: %s"), *ActualBaseVersion);
				}
			}
		}
	}

	// 2. 收集当前资源
	UpdateProgress(TEXT("收集资源"), TEXT(""), 0, 0);

	TArray<FString> CurrentAssetPaths;
	TMap<FString, FString> CurrentAssetDiskPaths;
	TMap<FString, FString> CurrentAssetSourcePaths;

	if (!CollectAssets( CurrentAssetPaths, CurrentAssetDiskPaths, CurrentAssetSourcePaths, ErrorMessage))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = ErrorMessage;
		bIsBuilding = false;
		return Result;
	}

	// 3. 计算当前资源 Hash
	UpdateProgress(TEXT("计算资源 Hash"), TEXT(""), 0, CurrentAssetPaths.Num());

	TMap<FString, FString> CurrentAssetHashes;
	TMap<FString, int64> CurrentAssetSizes;

	for (int32 i = 0; i < CurrentAssetPaths.Num(); i++)
	{
		if (bIsCancelled)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("构建已取消");
			bIsBuilding = false;
			return Result;
		}

		const FString& AssetPath = CurrentAssetPaths[i];
		const FString* DiskPath = CurrentAssetDiskPaths.Find(AssetPath);
		const FString* SourcePath = CurrentAssetSourcePaths.Find(AssetPath);

		// 优先使用源文件路径计算 Hash（Cooked 文件每次 Cook 可能不同）
		FString HashPath = (SourcePath && FPaths::FileExists(**SourcePath)) ? *SourcePath
			: (DiskPath && FPaths::FileExists(**DiskPath)) ? *DiskPath : TEXT("");

		if (!HashPath.IsEmpty())
		{
			CurrentAssetHashes.Add(AssetPath, UHotUpdateFileUtils::CalculateFileHash(HashPath));
			CurrentAssetSizes.Add(AssetPath, IFileManager::Get().FileSize(*HashPath));
		}
		UpdateProgress(TEXT("计算资源 Hash"), AssetPath, i + 1, CurrentAssetPaths.Num());
	}

	// 4. 计算差异
	UpdateProgress(TEXT("计算差异"), TEXT(""), 0, 0);

	TArray<FString> ChangedAssets;
	FHotUpdateDiffReport DiffReport;

	if (!ComputeDiff(CurrentAssetPaths, CurrentAssetHashes, BaseAssetHashes, ChangedAssets, DiffReport))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("计算差异失败");
		bIsBuilding = false;
		return Result;
	}

	DiffReport.BaseVersion = CurrentConfig.BaseVersion;
	DiffReport.TargetVersion = CurrentConfig.PatchVersion;

	UE_LOG(LogHotUpdateEditor, Display, TEXT("差异: 新增 %d, 修改 %d, 删除 %d, 未变更 %d"),
		DiffReport.AddedAssets.Num(), DiffReport.ModifiedAssets.Num(), DiffReport.DeletedAssets.Num(), DiffReport.UnchangedAssets.Num());

	// 增量模式：只打包变更的资源（新增 + 修改）
	// 用于打包的资源列表（只包含变更资源）

	// === 增量 Cook：在 Diff 之后执行 ===
	if (CurrentConfig.bIncrementalCook && !CurrentConfig.bSkipCook)
	{
		// 提取修改资源（有 Cooked 输出，Hash 变了）
		TArray<FString> AssetsToCook;
		for (const FHotUpdateAssetDiff& Diff : DiffReport.ModifiedAssets)
		{
			AssetsToCook.Add(Diff.AssetPath);
		}

		// 查找新增资源：根据项目打包设置获取需要 Cook 的完整资源列表，与基础 Manifest 对比
		// 不在基础 Manifest 中的资源 = 新增资源
			{
				// 优先使用预收集数据，避免后台线程访问 AssetRegistry
				TArray<FString> AllPackagingAssetPaths;
				if (CurrentConfig.PreCollectedAssetPaths.Num() > 0)
				{
					AllPackagingAssetPaths = CurrentConfig.PreCollectedAssetPaths;
				}
				else
				{
					AllPackagingAssetPaths = FHotUpdatePackagingSettingsHelper::ParsePackagingSettings(true).AssetPaths;
				}

				// 构建基础 Manifest 资源集合
				TSet<FString> BaseAssetSet;
				for (const auto& Pair : BaseAssetHashes)
				{
					BaseAssetSet.Add(Pair.Key);
				}

				for (const FString& AssetPath : AllPackagingAssetPaths)
				{
					// 跳过 OFPA 数据
					if (AssetPath.Contains(TEXT("/__ExternalActors__/")) || AssetPath.Contains(TEXT("/__ExternalObjects__/")))
					{
						continue;
					}

					// 不在基础 Manifest 中 = 新增资源
					if (!BaseAssetSet.Contains(AssetPath))
					{
						AssetsToCook.Add(AssetPath);
						UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook: 发现新增资源: %s"), *AssetPath);
					}
				}
			}

		UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook: 需要 Cook %d 个资源 (修改 %d + 新增)"),
			AssetsToCook.Num(), DiffReport.ModifiedAssets.Num());

		if (AssetsToCook.Num() > 0)
		{
			UpdateProgress(TEXT("增量 Cook 资源"), TEXT(""), 0, AssetsToCook.Num());
			if (!FHotUpdatePackageHelper::CookAssets(CurrentConfig.Platform, AssetsToCook))
			{
				// 增量 Cook 失败，回退到全量 Cook
				UE_LOG(LogHotUpdateEditor, Warning, TEXT("增量 Cook 失败，回退到全量 Cook"));
				if (!FHotUpdatePackageHelper::CookAssets(CurrentConfig.Platform))
				{
					Result.bSuccess = false;
					Result.ErrorMessage = TEXT("Cook 资源失败");
					bIsBuilding = false;
					return Result;
				}
			}

			// 重新收集资源和计算 Hash（Cook 后可能有新的 Cooked 输出）
			UpdateProgress(TEXT("更新资源 Hash"), TEXT(""), 0, AssetsToCook.Num());
			FString CookedPlatformDir = HotUpdateUtils::GetCookedPlatformDir(CurrentConfig.Platform);
			for (int32 i = 0; i < AssetsToCook.Num(); i++)
			{
				const FString& AssetPath = AssetsToCook[i];
				FString DiskPath = FHotUpdatePackageHelper::GetCookedAssetPath(AssetPath, CookedPlatformDir);
				if (!DiskPath.IsEmpty() && FPaths::FileExists(*DiskPath))
				{
					// 优先使用源文件路径计算 Hash
					FString SourcePath = FHotUpdatePackageHelper::GetAssetSourcePath(AssetPath);
					FString HashPath = (!SourcePath.IsEmpty() && FPaths::FileExists(*SourcePath)) ? SourcePath : DiskPath;
					CurrentAssetHashes.Add(AssetPath, UHotUpdateFileUtils::CalculateFileHash(HashPath));
					CurrentAssetSizes.Add(AssetPath, IFileManager::Get().FileSize(*HashPath));
					CurrentAssetDiskPaths.Add(AssetPath, DiskPath);
					CurrentAssetPaths.AddUnique(AssetPath);
				}
				UpdateProgress(TEXT("更新资源 Hash"), AssetPath, i + 1, AssetsToCook.Num());
			}

			// 重新计算 Diff（Hash 可能因 Cook 而变化）
			ChangedAssets.Reset();
			DiffReport = FHotUpdateDiffReport();
			if (!ComputeDiff(CurrentAssetPaths, CurrentAssetHashes, BaseAssetHashes, ChangedAssets, DiffReport))
			{
				Result.bSuccess = false;
				Result.ErrorMessage = TEXT("重新计算差异失败");
				bIsBuilding = false;
				return Result;
			}
			DiffReport.BaseVersion = CurrentConfig.BaseVersion;
			DiffReport.TargetVersion = CurrentConfig.PatchVersion;

			UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook 后重新计算差异: 新增 %d, 修改 %d, 删除 %d"),
				DiffReport.AddedAssets.Num(), DiffReport.ModifiedAssets.Num(), DiffReport.DeletedAssets.Num());
		}
		else
		{
			UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook: 没有需要 Cook 的资源变更"));
		}
	}

	TMap<FString, FString> ChangedAssetDiskPaths;

	// 从已收集的 CurrentAssetDiskPaths 中查找磁盘路径，避免重复解析和 Staged/非Staged 判断不一致
	// 添加新增资源的磁盘路径
	for (const FHotUpdateAssetDiff& AddedDiff : DiffReport.AddedAssets)
	{
		const FString* DiskPath = CurrentAssetDiskPaths.Find(AddedDiff.AssetPath);
		if (DiskPath && FPaths::FileExists(**DiskPath))
		{
			ChangedAssetDiskPaths.Add(AddedDiff.AssetPath, *DiskPath);
		}
	}

	// 添加修改资源的磁盘路径
	for (const FHotUpdateAssetDiff& ModifiedDiff : DiffReport.ModifiedAssets)
	{
		const FString* DiskPath = CurrentAssetDiskPaths.Find(ModifiedDiff.AssetPath);
		if (DiskPath && FPaths::FileExists(**DiskPath))
		{
			ChangedAssetDiskPaths.Add(ModifiedDiff.AssetPath, *DiskPath);
		}
	}

	// 用于打包的资源列表
	TArray<FString> AssetsToPackage;
	for (const auto& Pair : ChangedAssetDiskPaths)
	{
		AssetsToPackage.Add(Pair.Key);
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("增量模式: 变更资源数 %d (新增 %d + 修改 %d)"),
		AssetsToPackage.Num(), DiffReport.AddedAssets.Num(), DiffReport.ModifiedAssets.Num());

	// 检查是否有变更
	bool bHasChanges = ChangedAssets.Num() > 0;
	bool bIsFullHotUpdate = CurrentConfig.bIncludeBaseContainers && !CurrentConfig.BaseContainerDirectory.Path.IsEmpty();

	if (!bHasChanges && !bIsFullHotUpdate)
	{
		Result.bSuccess = true;
		Result.ErrorMessage = TEXT("没有发现资源变更");
		Result.DiffReport = DiffReport;
		bIsBuilding = false;
		return Result;
	}

	// 5. 确定输出目录
	FString OutputDir = CurrentConfig.OutputDirectory.Path;
	if (OutputDir.IsEmpty())
	{
		OutputDir = FPaths::ProjectSavedDir() / TEXT("HotUpdateVersions");
	}

	FString PlatformStr = HotUpdateUtils::GetPlatformString(CurrentConfig.Platform);
	OutputDir = FPaths::Combine(OutputDir, CurrentConfig.PatchVersion, PlatformStr);
	FPaths::NormalizeDirectoryName(OutputDir);

	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*OutputDir);

	Result.OutputDirectory = OutputDir;
	Result.PatchVersion = CurrentConfig.PatchVersion;
	Result.BaseVersion = ActualBaseVersion;
	Result.DiffReport = DiffReport;
	Result.ChangedAssetCount = ChangedAssets.Num();
	Result.bRequiresBasePackage = true; // 默认需要基础包，全量热更新模式会在后面设置为 false

	// 6. 创建 Patch IoStore 容器
	// 全量热更新模式：即使没有变更，也需要复制基础容器
	// 增量模式：没有变更资源则跳过 Patch 容器创建
	UpdateProgress(TEXT("创建 Patch 容器"), TEXT(""), 0, AssetsToPackage.Num());

	FString PatchUtocPath;
	FString PatchUcasPath;
	int64 PatchSize = 0;

	if (ChangedAssetDiskPaths.Num() > 0)
	{
		FHotUpdateIoStoreBuilder IoStoreBuilder;

		FHotUpdateIoStoreConfig IoStoreConfig = CurrentConfig.IoStoreConfig;
		IoStoreConfig.bUseIoStore = false;  // Patch 强制使用 .pak 格式
		// UE5 标准补丁命名格式：{项目名}-{平台}_{PatchIndex}_P
			// 补丁命名格式：{项目名}_P_{版本号}，如 MyProject_P_1.5.2
			// _P 后缀让引擎运行时自动提升 PakOrder，版本号保证多补丁不重名
			IoStoreConfig.ContainerName = FString::Printf(TEXT("Patch_%s_P"), *CurrentConfig.PatchVersion);

		FString PaksDir = FPaths::Combine(OutputDir, TEXT("Paks"));
		IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*PaksDir);

		FString PatchOutputPath = FPaths::Combine(PaksDir, IoStoreConfig.ContainerName);

		FHotUpdateIoStoreResult IoStoreResult = IoStoreBuilder.BuildIoStoreContainer(
			ChangedAssetDiskPaths, PatchOutputPath, IoStoreConfig);

		if (!IoStoreResult.bSuccess)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = FString::Printf(TEXT("Patch 容器创建失败: %s"), *IoStoreResult.ErrorMessage);
			bIsBuilding = false;
			return Result;
		}

		PatchUtocPath = IoStoreResult.UtocPath;
		PatchUcasPath = IoStoreResult.UcasPath;
		PatchSize = IoStoreResult.ContainerSize;

		UE_LOG(LogHotUpdateEditor, Log, TEXT("Patch 容器创建成功: %s, 大小 %lld 字节"), *PatchUtocPath, PatchSize);
	}
	else if (bIsFullHotUpdate)
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("全量热更新模式: 无变更资源，跳过 Patch 容器创建，仅复制基础容器"));
	}
	else
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("没有有效的资源文件");
		bIsBuilding = false;
		return Result;
	}

	Result.PatchUtocPath = PatchUtocPath;
	Result.PatchUcasPath = PatchUcasPath;
	Result.PatchSize = PatchSize;

	// 6.6 从基础版本目录加载文件 hash（用于 filemanifest，不填充 BaseContainers）
	TArray<FHotUpdateContainerInfo> BaseContainers;
	TMap<FString, FString> BaseContainerFilesHash;
	TMap<FString, int64> BaseContainerFilesSize;

	if (CurrentConfig.bIncludeBaseContainers && !CurrentConfig.BaseContainerDirectory.Path.IsEmpty())
	{
		FString BaseContainerDir = CurrentConfig.BaseContainerDirectory.Path;
		FPaths::NormalizeDirectoryName(BaseContainerDir);

		UE_LOG(LogHotUpdateEditor, Log, TEXT("全量热更新: 从目录加载基础版本容器文件: %s"), *BaseContainerDir);

		TArray<FHotUpdateContainerInfo> ScannedContainers;
		if (LoadBaseContainers(BaseContainerDir, ScannedContainers, BaseContainerFilesHash, BaseContainerFilesSize))
		{
			UE_LOG(LogHotUpdateEditor, Log, TEXT("全量热更新: 扫描了 %d 个基础版本容器文件, %d 个文件 hash"),
				ScannedContainers.Num(), BaseContainerFilesHash.Num());
		}
		else
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("全量热更新: 无法加载基础版本容器目录: %s"), *BaseContainerDir);
		}
	}

	// 6.7 从基础版本 Manifest 解析容器信息（写入 manifest 供客户端下载）
	{
		FString BaseManifestJson;
		if (FFileHelper::LoadFileToString(BaseManifestJson, *CurrentConfig.BaseFileManifestPath.FilePath))
		{
			TSharedPtr<FJsonObject> BaseManifestObj;
			TSharedRef<TJsonReader<>> BaseReader = TJsonReaderFactory<>::Create(BaseManifestJson);
			if (FJsonSerializer::Deserialize(BaseReader, BaseManifestObj) && BaseManifestObj.IsValid())
			{
				// 兼容 chunks 和 containers 两种字段名
				const TArray<TSharedPtr<FJsonValue>>* ContainersArray = nullptr;
				bool bFound = BaseManifestObj->TryGetArrayField(TEXT("chunks"), ContainersArray);
				if (!bFound)
				{
					bFound = BaseManifestObj->TryGetArrayField(TEXT("containers"), ContainersArray);
				}

				if (bFound && ContainersArray)
				{
					for (const TSharedPtr<FJsonValue>& ContainerValue : *ContainersArray)
					{
						TSharedPtr<FJsonObject> ContainerObj = ContainerValue->AsObject();
						if (!ContainerObj.IsValid()) continue;

						// 只保留 patch 类型的容器（base 类型属于基础包，不在 patch manifest 中引用）
						FString ContainerType;
						if (ContainerObj->TryGetStringField(TEXT("containerType"), ContainerType) && !ContainerType.StartsWith(TEXT("patch")))
						{
							continue;
						}

						FHotUpdateContainerInfo Info;
						// 兼容 ChunkName 和 containerName
						if (ContainerObj->HasField(TEXT("ChunkName")))
						{
							Info.ContainerName = ContainerObj->GetStringField(TEXT("ChunkName"));
						}
						else
						{
							Info.ContainerName = ContainerObj->GetStringField(TEXT("containerName"));
						}
						Info.UtocPath = ContainerObj->GetStringField(TEXT("utocPath"));
						Info.UtocSize = (int64)ContainerObj->GetNumberField(TEXT("utocSize"));
						Info.UtocHash = ContainerObj->GetStringField(TEXT("utocHash"));

						if (ContainerObj->HasField(TEXT("ucasPath")))
						{
							Info.UcasPath = ContainerObj->GetStringField(TEXT("ucasPath"));
							Info.UcasSize = (int64)ContainerObj->GetNumberField(TEXT("ucasSize"));
							Info.UcasHash = ContainerObj->GetStringField(TEXT("ucasHash"));
						}

						Info.ContainerType = EHotUpdateContainerType::Patch;
						// 优先使用 manifest 中的 version 字段，否则使用 CurrentConfig.BaseVersion
						if (ContainerObj->HasField(TEXT("version")))
						{
							Info.Version = ContainerObj->GetStringField(TEXT("version"));
						}
						else
						{
							Info.Version = ActualBaseVersion;
						}

						BaseContainers.Add(Info);
					}

					UE_LOG(LogHotUpdateEditor, Log, TEXT("从 Manifest 加载了 %d 个 patch 容器引用"), BaseContainers.Num());
				}
			}
		}

		// 如果有基础容器，标记包含基础容器并计算总下载大小
		if (BaseContainers.Num() > 0)
		{
			Result.bIncludesBaseContainers = true;
			Result.BaseContainers = BaseContainers;

			// 总下载大小 = Patch 包大小 + 基础容器大小
			Result.TotalDownloadSize = Result.PatchSize;
			for (const FHotUpdateContainerInfo& Container : BaseContainers)
			{
				Result.TotalDownloadSize += Container.UtocSize + Container.UcasSize;
			}
		}
	}

	// 7. 生成 Manifest
	UpdateProgress(TEXT("生成 Manifest"), TEXT(""), 0, 0);

	// 更新 CurrentConfig 中的 BaseVersion
	CurrentConfig.BaseVersion = ActualBaseVersion;

	FString ManifestPath = FPaths::Combine(OutputDir, TEXT("manifest.json"));

	if (!GenerateManifest(ManifestPath, Result.PatchUtocPath, Result.PatchUcasPath, AssetsToPackage, ChangedAssetDiskPaths, BaseAssetHashes, BaseAssetSizes, DiffReport, BaseContainers, BaseContainerFilesHash, BaseContainerFilesSize))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("生成 Manifest 失败");
		bIsBuilding = false;
		return Result;
	}

	Result.ManifestFilePath = ManifestPath;

	// 8. 注册版本
	UpdateProgress(TEXT("注册版本"), TEXT(""), 0, 0);

	FHotUpdateVersionManager VersionManager;

	FHotUpdateEditorVersionInfo VersionInfo;
	VersionInfo.VersionString = CurrentConfig.PatchVersion;
	VersionInfo.PackageKind = EHotUpdatePackageKind::Patch;
	VersionInfo.BaseVersion = CurrentConfig.BaseVersion;
	VersionInfo.Platform = CurrentConfig.Platform;
	VersionInfo.CreatedTime = FDateTime::Now();
	VersionInfo.FileManifestPath = FPaths::Combine(OutputDir, TEXT("filemanifest.json"));
	VersionInfo.UtocPath = Result.PatchUtocPath;
	VersionInfo.AssetCount = AssetsToPackage.Num();
	VersionInfo.PackageSize = Result.bIncludesBaseContainers ? Result.TotalDownloadSize : Result.PatchSize;

	VersionManager.RegisterVersion(VersionInfo);

	// 9. 完成
	Result.bSuccess = true;

	bIsBuilding = false;

	UpdateProgress(TEXT("完成"), TEXT(""), 1, 1);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("更新包构建成功: %s, 变更 %d 个资源, 大小 %lld 字节"), *OutputDir, ChangedAssets.Num(), Result.PatchSize);

	return Result;
}

void FHotUpdatePatchPackageBuilder::BuildPatchPackageAsync(const FHotUpdatePatchPackageConfig& Config)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("BuildPatchPackageAsync 开始调用"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bIsBuilding: %s"), bIsBuilding ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  BuildTask.IsValid(): %s"), BuildTask.IsValid() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  BuildTask.IsReady(): %s"), BuildTask.IsReady() ? TEXT("true") : TEXT("false"));

	// 检查是否有正在运行的构建任务
	if (bIsBuilding)
	{
		if (BuildTask.IsValid() && !BuildTask.IsReady())
		{
			// 任务仍在运行
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("已有更新包构建任务正在运行，拒绝新的构建请求"));
			FHotUpdatePatchPackageResult Result;
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("已有构建任务正在进行中");
			OnComplete.Broadcast(Result);
			return;
		}
		else
		{
			// 之前的构建异常终止，重置状态
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("检测到之前的构建异常终止，正在重置构建状态"));
			bIsBuilding = false;
			bIsCancelled = false;
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("开始新的更新包构建任务，版本: %s，基础版本: %s"), *CurrentConfig.PatchVersion, *CurrentConfig.BaseVersion);

	bIsBuilding = true;
	bIsCancelled = false;

	CurrentConfig = Config;

	// 始终从打包配置读取资源路径
	UE_LOG(LogHotUpdateEditor, Log, TEXT("在游戏线程预收集打包设置中的资源..."));

	FHotUpdatePackagingSettingsResult SettingsResult = FHotUpdatePackagingSettingsHelper::ParsePackagingSettings(true);
	if (SettingsResult.Errors.Num() > 0)
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("解析打包设置失败: %s"), *FString::Join(SettingsResult.Errors, TEXT("\n")));

		FHotUpdatePatchPackageResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Join(SettingsResult.Errors, TEXT("\n"));
		bIsBuilding = false;
		OnComplete.Broadcast(Result);
		return;
		}

	CurrentConfig.PreCollectedAssetPaths = SettingsResult.AssetPaths;

	// 收集 Staged 文件（非 UE 资产）
	for (const FHotUpdateStagedFileInfo& StagedFile : SettingsResult.StagedFiles)
	{
		CurrentConfig.PreCollectedNonAssetPaths.Add(StagedFile.PakPath);
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("预收集完成（含依赖），共 %d 个资源, %d 个非资源文件"), CurrentConfig.PreCollectedAssetPaths.Num(), CurrentConfig.PreCollectedNonAssetPaths.Num());

	TWeakPtr<FHotUpdatePatchPackageBuilder> WeakBuilder(AsShared());
	BuildTask = Async(EAsyncExecution::Thread, [WeakBuilder](){
		TSharedPtr<FHotUpdatePatchPackageBuilder> Builder = WeakBuilder.Pin();
		if (!Builder.IsValid())
		{
			return;
		}

		FHotUpdatePatchPackageResult Result = Builder->BuildPatchPackage(Builder->CurrentConfig);

		AsyncTask(ENamedThreads::GameThread, [WeakBuilder, Result]()
		{
			TSharedPtr<FHotUpdatePatchPackageBuilder> PinnedBuilder = WeakBuilder.Pin();
			if (PinnedBuilder.IsValid())
			{
				PinnedBuilder->OnComplete.Broadcast(Result);
			}
		});
	});
}

void FHotUpdatePatchPackageBuilder::CancelBuild()
{
	bIsCancelled = true;
}

bool FHotUpdatePatchPackageBuilder::ValidateConfig(const FHotUpdatePatchPackageConfig& Config, FString& OutErrorMessage)
{
	if (Config.PatchVersion.IsEmpty())
	{
		OutErrorMessage = TEXT("更新包版本号不能为空");
		return false;
	}

	// 热更新打包需要基础版本相关配置
	if (Config.BaseVersion.IsEmpty())
	{
		OutErrorMessage = TEXT("基础版本号不能为空");
		return false;
	}

	if (Config.BaseFileManifestPath.FilePath.IsEmpty())
	{
		OutErrorMessage = TEXT("基础版本 Manifest 路径不能为空");
		return false;
	}

	if (!FPaths::FileExists(Config.BaseFileManifestPath.FilePath))
	{
		OutErrorMessage = FString::Printf(TEXT("基础版本 Manifest 文件不存在: %s"), *Config.BaseFileManifestPath.FilePath);
		return false;
	}

	return true;
}

bool FHotUpdatePatchPackageBuilder::CollectAssets(TArray<FString>& OutAssetPaths, TMap<FString, FString>& OutAssetDiskPaths, TMap<FString, FString>& OutAssetSourcePaths, FString& OutErrorMessage) const
{
	TArray<FString> AllAssetPaths;
	TArray<FString> AllNonAssetPaths;

	// 优先使用预收集的资源列表（游戏线程已收集，含依赖），避免后台线程访问 AssetRegistry
	if (CurrentConfig.PreCollectedAssetPaths.Num() > 0)
	{
		AllAssetPaths = CurrentConfig.PreCollectedAssetPaths;
		AllNonAssetPaths = CurrentConfig.PreCollectedNonAssetPaths;
	}
	else
	{
		// 同步调用路径（如 Commandlet），此时在游戏线程，可以直接访问 AssetRegistry
		FHotUpdatePackagingSettingsResult SettingsResult = FHotUpdatePackagingSettingsHelper::ParsePackagingSettings(true);
		if (SettingsResult.Errors.Num() > 0)
		{
			OutErrorMessage = FString::Join(SettingsResult.Errors, TEXT("\n"));
			return false;
		}
		AllAssetPaths = SettingsResult.AssetPaths;
		// 收集 Staged 文件的 PakPath
		for (const FHotUpdateStagedFileInfo& StagedFile : SettingsResult.StagedFiles)
		{
			AllNonAssetPaths.Add(StagedFile.PakPath);
		}
	}

	AllAssetPaths.Append(AllNonAssetPaths);
	
	// 获取磁盘路径
	// 使用 Cooked 目录解析磁盘路径
	// 非增量模式：文件不存在则自动过滤（包括 OFPA 等已合入 .umap 的资源）
	FString CookedPlatformDir = HotUpdateUtils::GetCookedPlatformDir(CurrentConfig.Platform);
	for (const FString& AssetPath : AllAssetPaths)
	{
		// 检查是否为 Staged 文件（pak 内路径格式，如 /GameUpdate/Content/Setting/ui.txt）
		FString StagedPrefix = TEXT("/") + FString(FApp::GetProjectName()) + TEXT("/Content/");
		bool bIsStagedFile = AssetPath.StartsWith(StagedPrefix) &&
			!AssetPath.EndsWith(TEXT(".uasset")) && !AssetPath.EndsWith(TEXT(".umap"));

		if (bIsStagedFile)
		{
			// Staged 文件：从 pak 内路径映射回 Content 目录的源文件路径
			// /GameUpdate/Content/Setting/ui.txt -> Content/Setting/ui.txt
			FString RelativePath = AssetPath.RightChop(StagedPrefix.Len());
			FString SourcePath = FPaths::ProjectContentDir() / RelativePath;

			if (FPaths::FileExists(*SourcePath))
			{
				// Staged 文件：使用源文件绝对路径作为 AssetPath，与基准 filemanifest 格式一致
				FString AbsoluteSourcePath = FPaths::ConvertRelativePathToFull(SourcePath);
				OutAssetPaths.Add(AbsoluteSourcePath);
				OutAssetDiskPaths.Add(AbsoluteSourcePath, SourcePath);
				OutAssetSourcePaths.Add(AbsoluteSourcePath, SourcePath);
			}
			else
			{
				UE_LOG(LogHotUpdateEditor, Verbose, TEXT("跳过源文件不存在的 Staged 文件: %s"), *AssetPath);
			}
		}
		else
		{
			const FString DiskPath = FHotUpdatePackageHelper::GetCookedAssetPath(AssetPath, CookedPlatformDir);
			if (!DiskPath.IsEmpty() && FPaths::FileExists(*DiskPath))
			{
				// UE 资产：使用源文件绝对路径作为 key，与基准 filemanifest 格式一致
				FString SourcePath = FHotUpdatePackageHelper::GetAssetSourcePath(AssetPath);
				FString AbsoluteAssetPath;
				if (!SourcePath.IsEmpty() && FPaths::FileExists(*SourcePath))
				{
					AbsoluteAssetPath = FPaths::ConvertRelativePathToFull(SourcePath);
				}
				else
				{
					// 无法获取源文件路径时，回退到虚拟路径
					AbsoluteAssetPath = AssetPath;
				}

				OutAssetPaths.Add(AbsoluteAssetPath);
				OutAssetDiskPaths.Add(AbsoluteAssetPath, DiskPath);
				if (!SourcePath.IsEmpty())
				{
					OutAssetSourcePaths.Add(AbsoluteAssetPath, SourcePath);
				}
			}
			else
			{
				UE_LOG(LogHotUpdateEditor, Verbose, TEXT("跳过无 cooked 文件的资源: %s"), *AssetPath);
			}
		}
	}

	return true;
}

bool FHotUpdatePatchPackageBuilder::LoadBaseFileManifest(
	const FString& ManifestPath,
	TMap<FString, FString>& OutAssetHashes,
	TMap<FString, int64>& OutAssetSizes)
{
	// 优先读取 filemanifest.json（包含 files 信息）
	const FString FileManifestPath = ManifestPath;
	
	FString JsonString;
	bool bLoaded = false;

	// 读取 filemanifest.json
	if (FPaths::FileExists(FileManifestPath))
	{
		bLoaded = FFileHelper::LoadFileToString(JsonString, *FileManifestPath);
		if (bLoaded)
		{
			UE_LOG(LogHotUpdateEditor, Log, TEXT("加载 fileManifest: %s"), *FileManifestPath);
		}
	}

	if (!bLoaded)
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法读取 Manifest: %s 或 %s"), *ManifestPath, *FileManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法解析 File Manifest JSON: %s"), *ManifestPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* FilesArray;
	if (JsonObject->TryGetArrayField(TEXT("files"), FilesArray))
	{
		for (const TSharedPtr<FJsonValue>& FileValue : *FilesArray)
		{
			TSharedPtr<FJsonObject> FileObj = FileValue->AsObject();
			if (!FileObj.IsValid()) continue;

			FString FilePath = FileObj->GetStringField(TEXT("filePath"));
			FString Hash = FileObj->GetStringField(TEXT("fileHash"));
			int64 Size = (int64)FileObj->GetNumberField(TEXT("fileSize"));

			// 直接使用 filePath 作为 key（与 CollectAssets 生成的绝对路径格式一致）
			FPaths::NormalizeFilename(FilePath);

			OutAssetHashes.Add(FilePath, Hash);
			OutAssetSizes.Add(FilePath, Size);
		}
	}

	UE_LOG(LogHotUpdateEditor, Display, TEXT("LoadBaseManifest: 加载了 %d 个资产"), OutAssetHashes.Num());

	return OutAssetHashes.Num() > 0;
}

bool FHotUpdatePatchPackageBuilder::ComputeDiff(
	const TArray<FString>& CurrentAssets,
	const TMap<FString, FString>& CurrentHashes,
	const TMap<FString, FString>& BaseHashes,
	TArray<FString>& OutChangedAssets,
	FHotUpdateDiffReport& OutReport)
{
	UE_LOG(LogHotUpdateEditor, Display, TEXT("ComputeDiff: CurrentAssets.Num=%d, CurrentHashes.Num=%d, BaseHashes.Num=%d"),
		CurrentAssets.Num(), CurrentHashes.Num(), BaseHashes.Num());

	// 收集所有路径
	TSet<FString> AllPaths;
	for (const auto& Pair : BaseHashes) AllPaths.Add(Pair.Key);
	for (const FString& Path : CurrentAssets) AllPaths.Add(Path);

	UE_LOG(LogHotUpdateEditor, Display, TEXT("ComputeDiff: AllPaths.Num=%d (去重后)"), AllPaths.Num());

	for (const FString& Path : AllPaths)
	{
		bool bInBase = BaseHashes.Contains(Path);
		bool bInCurrent = CurrentHashes.Contains(Path);

		FHotUpdateAssetDiff Diff;
		Diff.AssetPath = Path;

		if (!bInBase && bInCurrent)
		{
			// 新增资源
			Diff.ChangeType = EHotUpdateFileChangeType::Added;
			Diff.NewHash = CurrentHashes[Path];
			Diff.ChangeDescription = TEXT("新增资源");
			OutReport.AddedAssets.Add(Diff);
			OutChangedAssets.Add(Path);
		}
		else if (bInBase && !bInCurrent)
		{
			// 删除资源
			Diff.ChangeType = EHotUpdateFileChangeType::Deleted;
			Diff.OldHash = BaseHashes[Path];
			Diff.ChangeDescription = TEXT("删除资源");
			OutReport.DeletedAssets.Add(Diff);
		}
		else if (bInBase && bInCurrent)
		{
			if (BaseHashes[Path] != CurrentHashes[Path])
			{
				// 修改资源
				Diff.ChangeType = EHotUpdateFileChangeType::Modified;
				Diff.OldHash = BaseHashes[Path];
				Diff.NewHash = CurrentHashes[Path];
				Diff.ChangeDescription = TEXT("修改资源");
				OutReport.ModifiedAssets.Add(Diff);
				OutChangedAssets.Add(Path);
			}
			else
			{
				// 未变更
				Diff.ChangeType = EHotUpdateFileChangeType::Unchanged;
				Diff.OldHash = BaseHashes[Path];
				Diff.NewHash = CurrentHashes[Path];
				OutReport.UnchangedAssets.Add(Diff);
			}
		}
	}

	return true;
}

bool FHotUpdatePatchPackageBuilder::GenerateManifest(
	const FString& ManifestPath,
	const FString& PatchUtocPath,
	const FString& PatchUcasPath,
	const TArray<FString>& ChangedAssetPaths,
	const TMap<FString, FString>& ChangedAssetDiskPaths,
	const TMap<FString, FString>& BaseAssetHashes,
	const TMap<FString, int64>& BaseAssetSizes,
	const FHotUpdateDiffReport& DiffReport,
	const TArray<FHotUpdateContainerInfo>& BaseContainers,
	const TMap<FString, FString>& BaseContainerFilesHash,
	const TMap<FString, int64>& BaseContainerFilesSize) const
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	// Manifest 版本
	RootObject->SetNumberField(TEXT("manifestVersion"), 4); // 升级版本号，支持链式 Patch

	// 包类型
	RootObject->SetNumberField(TEXT("packageKind"), static_cast<int32>(EHotUpdatePackageKind::Patch));

	// 版本信息
	TSharedPtr<FJsonObject> VersionInfo = MakeShareable(new FJsonObject);

	TArray<FString> VersionParts;
	CurrentConfig.PatchVersion.ParseIntoArray(VersionParts, TEXT("."));
	
	VersionInfo->SetStringField(TEXT("version"), CurrentConfig.PatchVersion);
	VersionInfo->SetStringField(TEXT("platform"), HotUpdateUtils::GetPlatformString(CurrentConfig.Platform));
	VersionInfo->SetNumberField(TEXT("timestamp"), FDateTime::Now().ToUnixTimestamp());

	RootObject->SetObjectField(TEXT("version"), VersionInfo);

	// 基础版本
	RootObject->SetStringField(TEXT("baseVersion"), CurrentConfig.BaseVersion);

	// 差异摘要
	TSharedPtr<FJsonObject> DiffSummary = MakeShareable(new FJsonObject);
	DiffSummary->SetNumberField(TEXT("addedCount"), DiffReport.AddedAssets.Num());
	DiffSummary->SetNumberField(TEXT("modifiedCount"), DiffReport.ModifiedAssets.Num());
	DiffSummary->SetNumberField(TEXT("deletedCount"), DiffReport.DeletedAssets.Num());
	DiffSummary->SetNumberField(TEXT("unchangedCount"), DiffReport.UnchangedAssets.Num());
	RootObject->SetObjectField(TEXT("diffSummary"), DiffSummary);

	// 全量热更新标志
	if (BaseContainers.Num() > 0)
	{
		RootObject->SetBoolField(TEXT("bIncludesBaseContainers"), true);
		RootObject->SetBoolField(TEXT("bRequiresBasePackage"), false);

		// 计算总下载大小
		int64 TotalDownloadSize = 0;
		for (const FHotUpdateContainerInfo& Container : BaseContainers)
		{
			TotalDownloadSize += Container.UtocSize + Container.UcasSize;
		}
		// 加上当前 patch 容器大小
		if (!PatchUtocPath.IsEmpty() && FPaths::FileExists(*PatchUtocPath))
		{
			TotalDownloadSize += IFileManager::Get().FileSize(*PatchUtocPath);
		}
		RootObject->SetNumberField(TEXT("totalDownloadSize"), TotalDownloadSize);
	}
	else
	{
		RootObject->SetBoolField(TEXT("bIncludesBaseContainers"), false);
		RootObject->SetBoolField(TEXT("bRequiresBasePackage"), true);
	}

	// 容器文件列表
	TArray<TSharedPtr<FJsonValue>> ContainersArray;

	// 1. 先添加基础版本容器（全量热更新模式）
	for (const FHotUpdateContainerInfo& BaseContainer : BaseContainers)
	{
		TSharedPtr<FJsonObject> BaseContainerObj = MakeShareable(new FJsonObject);
		BaseContainerObj->SetStringField(TEXT("containerName"), BaseContainer.ContainerName);
		BaseContainerObj->SetStringField(TEXT("utocPath"), BaseContainer.UtocPath);
		BaseContainerObj->SetNumberField(TEXT("utocSize"), BaseContainer.UtocSize);
		BaseContainerObj->SetStringField(TEXT("utocHash"), BaseContainer.UtocHash);

		if (!BaseContainer.UcasPath.IsEmpty())
		{
			BaseContainerObj->SetStringField(TEXT("ucasPath"), BaseContainer.UcasPath);
			BaseContainerObj->SetNumberField(TEXT("ucasSize"), BaseContainer.UcasSize);
			BaseContainerObj->SetStringField(TEXT("ucasHash"), BaseContainer.UcasHash);
			BaseContainerObj->SetStringField(TEXT("containerType"), TEXT("patch"));
		}
		else
		{
			BaseContainerObj->SetStringField(TEXT("ucasPath"), TEXT(""));
			BaseContainerObj->SetNumberField(TEXT("ucasSize"), 0);
			BaseContainerObj->SetStringField(TEXT("ucasHash"), TEXT(""));
			BaseContainerObj->SetStringField(TEXT("containerType"), TEXT("patch_pak"));
		}

		BaseContainerObj->SetStringField(TEXT("version"), BaseContainer.Version);
		ContainersArray.Add(MakeShareable(new FJsonValueObject(BaseContainerObj)));
	}

	// 添加当前 Patch 容器

	if (!PatchUtocPath.IsEmpty() && FPaths::FileExists(*PatchUtocPath))
	{
		TSharedPtr<FJsonObject> PatchContainerObj = MakeShareable(new FJsonObject);

			FString ContainerName = FString::Printf(TEXT("Patch_%s_P"), *CurrentConfig.PatchVersion);
		PatchContainerObj->SetStringField(TEXT("containerName"), ContainerName);

		// .utoc 文件信息
		FString UtocFileName = FPaths::GetCleanFilename(PatchUtocPath);
		PatchContainerObj->SetStringField(TEXT("utocPath"), TEXT("Paks/") + UtocFileName);
		PatchContainerObj->SetNumberField(TEXT("utocSize"), IFileManager::Get().FileSize(*PatchUtocPath));
		PatchContainerObj->SetStringField(TEXT("utocHash"), UHotUpdateFileUtils::CalculateFileHash(PatchUtocPath));

		// .ucas 文件信息（可选）
		if (!PatchUcasPath.IsEmpty() && FPaths::FileExists(*PatchUcasPath))
		{
			FString UcasFileName = FPaths::GetCleanFilename(PatchUcasPath);
			PatchContainerObj->SetStringField(TEXT("ucasPath"), TEXT("Paks/") + UcasFileName);
			PatchContainerObj->SetNumberField(TEXT("ucasSize"), IFileManager::Get().FileSize(*PatchUcasPath));
			PatchContainerObj->SetStringField(TEXT("ucasHash"), UHotUpdateFileUtils::CalculateFileHash(PatchUcasPath));
			PatchContainerObj->SetStringField(TEXT("containerType"), TEXT("patch"));
		}
		else if (PatchUtocPath.EndsWith(TEXT(".pak")))
		{
			// 传统 .pak 格式
			PatchContainerObj->SetStringField(TEXT("ucasPath"), TEXT(""));
			PatchContainerObj->SetNumberField(TEXT("ucasSize"), 0);
			PatchContainerObj->SetStringField(TEXT("ucasHash"), TEXT(""));
			PatchContainerObj->SetStringField(TEXT("containerType"), TEXT("patch_pak"));
		}
		else
		{
			// 单文件格式，数据嵌入在 utoc 中
			PatchContainerObj->SetStringField(TEXT("ucasPath"), TEXT(""));
			PatchContainerObj->SetNumberField(TEXT("ucasSize"), 0);
			PatchContainerObj->SetStringField(TEXT("ucasHash"), TEXT(""));
			PatchContainerObj->SetStringField(TEXT("containerType"), TEXT("patch_embedded"));
		}

			PatchContainerObj->SetStringField(TEXT("version"), CurrentConfig.PatchVersion);

		ContainersArray.Add(MakeShareable(new FJsonValueObject(PatchContainerObj)));
	}

	RootObject->SetArrayField(TEXT("containers"), ContainersArray);

	// 注意: 不生成 files 字段，客户端只关心需要下载的容器文件
	// files 信息仅供编辑器端差异计算使用，存储在单独的 fileManifest 文件中

	// 序列化客户端 manifest
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(OutputString, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}

	// 生成编辑器端 fileManifest（包含 files 信息，用于差异计算）
	FString FileManifestPath = FPaths::Combine(FPaths::GetPath(ManifestPath), TEXT("filemanifest.json"));

	TSharedPtr<FJsonObject> FileManifestObj = MakeShareable(new FJsonObject);
	FileManifestObj->SetNumberField(TEXT("manifestVersion"), 4);
	FileManifestObj->SetNumberField(TEXT("packageKind"), static_cast<int32>(EHotUpdatePackageKind::Patch));
	FileManifestObj->SetObjectField(TEXT("version"), VersionInfo);
	FileManifestObj->SetStringField(TEXT("baseVersion"), CurrentConfig.BaseVersion);

	FileManifestObj->SetObjectField(TEXT("diffSummary"), DiffSummary);
	FileManifestObj->SetArrayField(TEXT("containers"), ContainersArray);

	// 收集所有资源路径（变更 + 未变更）
	TSet<FString> AllAssetPaths;
	for (const FString& Path : ChangedAssetPaths)
	{
		AllAssetPaths.Add(Path);
	}
	for (const FHotUpdateAssetDiff& UnchangedDiff : DiffReport.UnchangedAssets)
	{
		AllAssetPaths.Add(UnchangedDiff.AssetPath);
	}

	// 文件列表（仅用于编辑器端差异计算）
	TArray<TSharedPtr<FJsonValue>> FilesArray;

	for (const FString& AssetPath : AllAssetPaths)
	{
		TSharedPtr<FJsonObject> FileObj = MakeShareable(new FJsonObject);

		// filePath 使用源文件绝对路径（与基准 filemanifest 格式一致）
		// 对于 UE 资产，从 AssetPath 获取源文件路径
		// 对于 Staged 文件，AssetPath 已经是源文件绝对路径
		FString FilePath;
		FString AssetSourcePath = FHotUpdatePackageHelper::GetAssetSourcePath(AssetPath);
		if (!AssetSourcePath.IsEmpty() && FPaths::FileExists(*AssetSourcePath))
		{
			// UE 资产：使用源文件绝对路径
			FilePath = FPaths::ConvertRelativePathToFull(AssetSourcePath);
		}
		else
		{
			// Staged 文件或其他：AssetPath 已经是绝对路径
			FilePath = AssetPath;
		}

		FileObj->SetStringField(TEXT("filePath"), FilePath);

		// 检查文件来源（优先级：当前 patch > 基础版本容器 > base manifest）
		bool bIsCurrentPatch = ChangedAssetDiskPaths.Contains(AssetPath);
		bool bIsBaseContainer = BaseContainerFilesHash.Contains(AssetPath);

		if (bIsCurrentPatch)
		{
			// 当前变更资源：从当前 patch 加载
			if (const FString* DiskPath = ChangedAssetDiskPaths.Find(AssetPath))
			{
				// 优先使用源文件计算 Hash
				FString SourcePath = FHotUpdatePackageHelper::GetAssetSourcePath(AssetPath);
				FString HashPath = (!SourcePath.IsEmpty() && FPaths::FileExists(*SourcePath)) ? SourcePath : *DiskPath;
				int64 FileSize = IFileManager::Get().FileSize(*HashPath);
				FileObj->SetNumberField(TEXT("fileSize"), FileSize);
				FileObj->SetStringField(TEXT("fileHash"), UHotUpdateFileUtils::CalculateFileHash(HashPath));
				FileObj->SetStringField(TEXT("source"), TEXT("patch"));
			}
		}
		else if (bIsBaseContainer)
		{
			// 基础版本容器中的文件：从基础容器加载
			const FString* BaseContainerHash = BaseContainerFilesHash.Find(AssetPath);
			const int64* BaseContainerSize = BaseContainerFilesSize.Find(AssetPath);

			if (BaseContainerHash && BaseContainerSize)
			{
				FileObj->SetNumberField(TEXT("fileSize"), *BaseContainerSize);
				FileObj->SetStringField(TEXT("fileHash"), *BaseContainerHash);
				FileObj->SetStringField(TEXT("source"), TEXT("base_container"));
			}
		}
		else
		{
			// 未变更资源：从基础包 manifest 加载
			const FString* BaseHash = BaseAssetHashes.Find(AssetPath);
			const int64* BaseSize = BaseAssetSizes.Find(AssetPath);
			if (BaseHash && BaseSize)
			{
				FileObj->SetNumberField(TEXT("fileSize"), *BaseSize);
				FileObj->SetStringField(TEXT("fileHash"), *BaseHash);
				FileObj->SetStringField(TEXT("source"), TEXT("base"));
			}
		}
		
		FileObj->SetBoolField(TEXT("isCompressed"), CurrentConfig.IoStoreConfig.CompressionFormat != TEXT("None"));

		FilesArray.Add(MakeShareable(new FJsonValueObject(FileObj)));
	}

	FileManifestObj->SetArrayField(TEXT("files"), FilesArray);

	// 序列化 fileManifest
	FString FileManifestString;
	TSharedRef<TJsonWriter<>> FileWriter = TJsonWriterFactory<>::Create(&FileManifestString);
	FJsonSerializer::Serialize(FileManifestObj.ToSharedRef(), FileWriter);

	return FFileHelper::SaveStringToFile(FileManifestString, *FileManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FHotUpdatePatchPackageBuilder::UpdateProgress(
	const FString& Stage,
	const FString& CurrentFile,
	int32 ProcessedFiles,
	int32 TotalFiles)
{
	FHotUpdatePackageProgress ProgressCopy;
	{
		FScopeLock Lock(&ProgressCriticalSection);
		CurrentProgress.CurrentStage = Stage;
		CurrentProgress.CurrentFile = CurrentFile;
		CurrentProgress.ProcessedFiles = ProcessedFiles;
		CurrentProgress.TotalFiles = TotalFiles;
		CurrentProgress.bIsComplete = (ProcessedFiles >= TotalFiles && TotalFiles > 0);

		// 计算进度百分比
		CurrentProgress.ProgressPercent = TotalFiles > 0 ? (float)ProcessedFiles / TotalFiles * 100.0f : 0.0f;
		// 设置阶段描述
		CurrentProgress.StageDescription = FText::FromString(Stage);

		ProgressCopy = CurrentProgress;
	}

	// 同步模式下直接广播
	if (CurrentConfig.bSynchronousMode)
	{
		OnProgress.Broadcast(ProgressCopy);
	}
	else
	{
		// 异步模式下通过 AsyncTask 在游戏线程广播
		TWeakPtr<FHotUpdatePatchPackageBuilder> WeakBuilder(AsShared());
		AsyncTask(ENamedThreads::GameThread, [WeakBuilder, ProgressCopy]()
		{
			TSharedPtr<FHotUpdatePatchPackageBuilder> PinnedBuilder = WeakBuilder.Pin();
			if (PinnedBuilder.IsValid())
			{
				PinnedBuilder->OnProgress.Broadcast(ProgressCopy);
			}
		});
	}
}

bool FHotUpdatePatchPackageBuilder::LoadBaseContainers(
	const FString& ContainerDirectory,
	TArray<FHotUpdateContainerInfo>& OutContainers,
	TMap<FString, FString>& OutFilesHash,
	TMap<FString, int64>& OutFilesSize)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	if (!PlatformFile.DirectoryExists(*ContainerDirectory))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("基础版本容器目录不存在: %s"), *ContainerDirectory);
		return false;
	}

	// 查找目录中的 manifest 文件
	TArray<FString> FoundManifests;
	PlatformFile.FindFilesRecursively(FoundManifests, *ContainerDirectory, TEXT(".manifest.json"));

	if (FoundManifests.Num() == 0)
	{
		// 尝试查找 filemanifest.json
		PlatformFile.FindFilesRecursively(FoundManifests, *ContainerDirectory, TEXT(".filemanifest.json"));
	}

	if (FoundManifests.Num() == 0)
	{
		UE_LOG(LogHotUpdateEditor, Warning, TEXT("未在目录中找到 manifest 文件: %s，尝试扫描容器文件"), *ContainerDirectory);

		// 直接扫描目录中的 .utoc 文件
		TArray<FString> FoundUtocFiles;
		PlatformFile.FindFilesRecursively(FoundUtocFiles, *ContainerDirectory, TEXT(".utoc"));

		for (const FString& UtocPath : FoundUtocFiles)
		{
			FHotUpdateContainerInfo ContainerInfo;
			ContainerInfo.ContainerName = FPaths::GetBaseFilename(UtocPath);
			ContainerInfo.UtocPath = FPaths::GetCleanFilename(UtocPath);
			ContainerInfo.UtocSize = IFileManager::Get().FileSize(*UtocPath);
			ContainerInfo.UtocHash = UHotUpdateFileUtils::CalculateFileHash(UtocPath);
			ContainerInfo.ContainerType = EHotUpdateContainerType::Base;

			// 查找对应的 .ucas 文件
			FString UcasPath = FPaths::ChangeExtension(UtocPath, TEXT("ucas"));
			if (FPaths::FileExists(*UcasPath))
			{
				ContainerInfo.UcasPath = FPaths::GetCleanFilename(UcasPath);
				ContainerInfo.UcasSize = IFileManager::Get().FileSize(*UcasPath);
				ContainerInfo.UcasHash = UHotUpdateFileUtils::CalculateFileHash(UcasPath);
			}

			OutContainers.Add(ContainerInfo);
			UE_LOG(LogHotUpdateEditor, Log, TEXT("扫描到容器: %s (utoc: %lld bytes)"),
				*ContainerInfo.ContainerName, ContainerInfo.UtocSize);
		}

		return OutContainers.Num() > 0;
	}

	// 加载找到的第一个 manifest 文件
	FString ManifestPath = FoundManifests[0];
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法读取 Manifest 文件: %s"), *ManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法解析 Manifest JSON: %s"), *ManifestPath);
		return false;
	}

	// 解析 chunks 或 containers 字段
	const TArray<TSharedPtr<FJsonValue>>* ChunksArray = nullptr;
	bool bHasChunks = JsonObject->TryGetArrayField(TEXT("chunks"), ChunksArray);

	const TArray<TSharedPtr<FJsonValue>>* ContainersArray = nullptr;
	bool bHasContainers = JsonObject->TryGetArrayField(TEXT("containers"), ContainersArray);

	const TArray<TSharedPtr<FJsonValue>>* TargetArray = bHasChunks ? ChunksArray : (bHasContainers ? ContainersArray : nullptr);

	if (TargetArray)
	{
		for (const TSharedPtr<FJsonValue>& ChunkValue : *TargetArray)
		{
			TSharedPtr<FJsonObject> ChunkObj = ChunkValue->AsObject();
			if (!ChunkObj.IsValid()) continue;

			FHotUpdateContainerInfo ContainerInfo;

			// 兼容两种格式
			if (ChunkObj->HasField(TEXT("ChunkName")))
			{
				ContainerInfo.ContainerName = ChunkObj->GetStringField(TEXT("ChunkName"));
			}
			else if (ChunkObj->HasField(TEXT("containerName")))
			{
				ContainerInfo.ContainerName = ChunkObj->GetStringField(TEXT("containerName"));
			}

			ContainerInfo.UtocPath = ChunkObj->GetStringField(TEXT("utocPath"));
			ContainerInfo.UtocSize = (int64)ChunkObj->GetNumberField(TEXT("utocSize"));
			ContainerInfo.UtocHash = ChunkObj->GetStringField(TEXT("utocHash"));

			if (ChunkObj->HasField(TEXT("ucasPath")))
			{
				ContainerInfo.UcasPath = ChunkObj->GetStringField(TEXT("ucasPath"));
				ContainerInfo.UcasSize = (int64)ChunkObj->GetNumberField(TEXT("ucasSize"));
				ContainerInfo.UcasHash = ChunkObj->GetStringField(TEXT("ucasHash"));
			}

			ContainerInfo.ContainerType = EHotUpdateContainerType::Base;

			OutContainers.Add(ContainerInfo);
		}
	}

	// 解析 files 字段（如果存在）
	const TArray<TSharedPtr<FJsonValue>>* FilesArray;
	if (JsonObject->TryGetArrayField(TEXT("files"), FilesArray))
	{
		for (const TSharedPtr<FJsonValue>& FileValue : *FilesArray)
		{
			TSharedPtr<FJsonObject> FileObj = FileValue->AsObject();
			if (!FileObj.IsValid()) continue;

			FString FilePath = FileObj->GetStringField(TEXT("filePath"));
			FString Hash = FileObj->GetStringField(TEXT("fileHash"));
			int64 Size = (int64)FileObj->GetNumberField(TEXT("fileSize"));

			FString NormalizedPath = FHotUpdatePackageHelper::FileNameToAssetPath(FilePath);

			// 检查 source 字段，只添加 base 资源
			FString Source = TEXT("base");
			if (FileObj->HasField(TEXT("source")))
			{
				Source = FileObj->GetStringField(TEXT("source"));
			}

			if (Source == TEXT("base") || Source.IsEmpty())
			{
				OutFilesHash.Add(NormalizedPath, Hash);
				OutFilesSize.Add(NormalizedPath, Size);
			}
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("加载基础版本容器成功: %s, 容器数: %d, 文件数: %d"),
		*ManifestPath, OutContainers.Num(), OutFilesHash.Num());

	return OutContainers.Num() > 0;
}


