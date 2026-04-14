// Copyright czm. All Rights Reserved.

#include "HotUpdateIoStoreBuilder.h"
#include "HotUpdateEditor.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/SecureHash.h"
#include "Misc/PackageName.h"
#include "GenericPlatform/GenericPlatformProcess.h"

UHotUpdateIoStoreBuilder::UHotUpdateIoStoreBuilder()
	: bIsBuilding(false)
	, bIsCancelled(false)
{
}

FHotUpdateIoStoreResult UHotUpdateIoStoreBuilder::BuildIoStoreContainer(
	const TMap<FString, FString>& AssetPathToDiskPath,
	const FString& OutputPath,
	const FHotUpdateIoStoreConfig& Config)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("BuildIoStoreContainer (同步) 开始调用"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  输出路径: %s"), *OutputPath);
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  资源文件数: %d"), AssetPathToDiskPath.Num());

	FHotUpdateIoStoreResult Result;

	// 验证配置
	FString ErrorMessage;
	if (!ValidateConfig(Config, ErrorMessage))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("配置验证失败: %s"), *ErrorMessage);
		Result.bSuccess = false;
		Result.ErrorMessage = ErrorMessage;
		return Result;
	}

	bIsCancelled = false;

	// 执行构建
	bool bSuccess = CreateIoStoreWithUnrealPak(AssetPathToDiskPath, OutputPath, Config, Result);

	Result.bSuccess = bSuccess;
	Result.FileCount = AssetPathToDiskPath.Num();

	bIsBuilding = false;

	return Result;
}

void UHotUpdateIoStoreBuilder::BuildIoStoreContainerAsync(
	const TMap<FString, FString>& AssetPathToDiskPath,
	const FString& OutputPath,
	const FHotUpdateIoStoreConfig& Config)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("BuildIoStoreContainerAsync 开始调用"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  bIsBuilding: %s"), bIsBuilding ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  BuildTask.IsValid(): %s"), BuildTask.IsValid() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  BuildTask.IsReady(): %s"), BuildTask.IsReady() ? TEXT("true") : TEXT("false"));

	if (bIsBuilding)
	{
		if (BuildTask.IsValid() && !BuildTask.IsReady())
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("已有构建任务正在运行，拒绝新的构建请求"));
			FHotUpdateIoStoreResult Result;
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("已有构建任务正在进行中");
			OnComplete.Broadcast(Result);
			return;
		}
		else
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("检测到之前的构建异常终止，正在重置构建状态"));
			bIsBuilding = false;
			bIsCancelled = false;
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("开始新的构建任务，输出路径: %s"), *OutputPath);
	UE_LOG(LogHotUpdateEditor, Log, TEXT("资源文件数: %d"), AssetPathToDiskPath.Num());

	bIsBuilding = true;
	bIsCancelled = false;

	BuildTask = Async(EAsyncExecution::Thread, [this, AssetPathToDiskPath, OutputPath, Config]()
	{
		FHotUpdateIoStoreResult Result;
		bool bSuccess = CreateIoStoreWithUnrealPak(AssetPathToDiskPath, OutputPath, Config, Result);

		Result.bSuccess = bSuccess;
		Result.FileCount = AssetPathToDiskPath.Num();

		bIsBuilding = false;

		AsyncTask(ENamedThreads::GameThread, [this, Result]()
		{
			OnComplete.Broadcast(Result);
		});
	});
}

void UHotUpdateIoStoreBuilder::CancelBuild()
{
	bIsCancelled = true;
}

FHotUpdateIoStoreProgress UHotUpdateIoStoreBuilder::GetCurrentProgress() const
{
	FScopeLock Lock(&ProgressCriticalSection);
	return CurrentProgress;
}

bool UHotUpdateIoStoreBuilder::ValidateConfig(const FHotUpdateIoStoreConfig& Config, FString& OutErrorMessage)
{
	if (Config.ContainerName.IsEmpty())
	{
		OutErrorMessage = TEXT("容器名称不能为空");
		return false;
	}

	if (Config.CompressionFormat != TEXT("Oodle") &&
		Config.CompressionFormat != TEXT("Zlib") &&
		Config.CompressionFormat != TEXT("GZip") &&
		Config.CompressionFormat != TEXT("None"))
	{
		OutErrorMessage = FString::Printf(TEXT("不支持的压缩格式: %s"), *Config.CompressionFormat);
		return false;
	}

	return true;
}

bool UHotUpdateIoStoreBuilder::CreateIoStoreWithUnrealPak(
	const TMap<FString, FString>& AssetPathToDiskPath,
	const FString& OutputPath,
	const FHotUpdateIoStoreConfig& Config,
	FHotUpdateIoStoreResult& OutResult)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("开始创建 IoStore/Pak 容器: %s"), *OutputPath);
	UE_LOG(LogHotUpdateEditor, Log, TEXT("  使用 IoStore 格式: %s"), Config.bUseIoStore ? TEXT("true") : TEXT("false"));

	// 1. 查找 UnrealPak 工具
	FString UnrealPakPath = FindUnrealPakPath(OutResult.ErrorMessage);
	if (UnrealPakPath.IsEmpty())
	{
		return false;
	}

	// 2. 准备临时目录
	FString TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("IoStoreTemp"), Config.ContainerName);
	if (!PrepareTempDirectory(TempDir, OutResult.ErrorMessage))
	{
		return false;
	}

	// 3. 生成响应文件
	FString ResponseFilePath = FPaths::Combine(TempDir, TEXT("IoStoreCommands.txt"));
	int32 ValidFileCount = 0;
	int64 TotalSize = 0;

	if (!GenerateResponseFile(AssetPathToDiskPath, ResponseFilePath,
		ValidFileCount, TotalSize, OutResult.ErrorMessage))
	{
		CleanupTempDirectory(TempDir);
		return false;
	}

	// 4. 生成加密密钥文件
	FString CryptoKeyPath;
	if (!Config.EncryptionKey.IsEmpty())
	{
		CryptoKeyPath = GenerateCryptoKeyFile(TempDir, Config);
	}

	// 5. 准备输出目录
	if (!PrepareOutputDirectory(OutputPath, OutResult.ErrorMessage))
	{
		CleanupTempDirectory(TempDir);
		return false;
	}

	// 6. 构建命令行
	FString CmdLine = BuildUnrealPakCommandLine(OutputPath, ResponseFilePath, CryptoKeyPath, Config);

	// 7. 执行 UnrealPak
	UpdateProgress(TEXT("创建容器"), TEXT(""), 0, 1, 0, TotalSize);
	UE_LOG(LogHotUpdateEditor, Log, TEXT("执行 UnrealPak: %s %s"), *UnrealPakPath, *CmdLine);

	if (!ExecuteUnrealPak(UnrealPakPath, CmdLine, OutputPath, OutResult))
	{
		CleanupTempDirectory(TempDir);
		return false;
	}

	// 8. 清理临时目录
	CleanupTempDirectory(TempDir);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("容器创建完成，总大小: %lld 字节"), OutResult.ContainerSize);
	UpdateProgress(TEXT("完成"), TEXT(""), 1, 1, TotalSize, TotalSize);

	return true;
}

FString UHotUpdateIoStoreBuilder::FindUnrealPakPath(FString& OutErrorMessage)
{
	FString EngineDir = FPaths::EngineDir();
	FString UnrealPakPath = FPaths::Combine(EngineDir, TEXT("Binaries/Win64/UnrealPak.exe"));

	if (!FPaths::FileExists(UnrealPakPath))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("找不到 UnrealPak 工具: %s"), *UnrealPakPath);
		OutErrorMessage = TEXT("找不到 UnrealPak 工具");
		return TEXT("");
	}

	return UnrealPakPath;
}

bool UHotUpdateIoStoreBuilder::PrepareTempDirectory(
	const FString& TempDir,
	FString& OutErrorMessage)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	if (PlatformFile.DirectoryExists(*TempDir))
	{
		PlatformFile.DeleteDirectoryRecursively(*TempDir);
	}

	FString StagingDir = FPaths::Combine(TempDir, TEXT("Staging"));
	if (!PlatformFile.CreateDirectoryTree(*StagingDir))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法创建临时目录: %s"), *TempDir);
		OutErrorMessage = TEXT("无法创建临时目录");
		return false;
	}

	return true;
}

FString UHotUpdateIoStoreBuilder::GetPakInternalPath(const FString& AssetPath)
{
	// 从 AssetPath（如 "/Game/Maps/Start"）转换为 Pak 内部路径
	// Pak 内部路径直接使用 UE 长包名格式加上扩展名
	// 如 "/Game/Maps/Start" -> "/Game/Maps/Start.umap"

	FString PakPath = AssetPath;

	// 确保路径以 / 开头（标准 UE 长包名格式）
	if (!PakPath.StartsWith(TEXT("/")))
	{
		PakPath = TEXT("/") + PakPath;
	}

	// 根据资源类型添加正确的扩展名
	// uasset 是普通资源，umap 是地图
	if (PakPath.EndsWith(TEXT(".uasset")) || PakPath.EndsWith(TEXT(".umap")))
	{
		// 已经有扩展名，直接使用
	}
	else
	{
		// 尝试判断是地图还是普通资源
		// 地图通常包含在 Maps 目录下
		if (PakPath.Contains(TEXT("/Maps/")) || PakPath.Contains(TEXT("Map")))
		{
			PakPath += TEXT(".umap");
		}
		else
		{
			PakPath += TEXT(".uasset");
		}
	}

	// 确保使用正斜杠
	PakPath.ReplaceCharInline('\\', '/');

	UE_LOG(LogHotUpdateEditor, Log, TEXT("AssetPath -> PakPath: %s -> %s"), *AssetPath, *PakPath);

	return PakPath;
}

bool UHotUpdateIoStoreBuilder::GenerateResponseFile(
	const TMap<FString, FString>& AssetPathToDiskPath,
	const FString& ResponseFilePath,
	int32& OutValidFileCount,
	int64& OutTotalSize,
	FString& OutErrorMessage)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	FString ResponseContent;

	OutValidFileCount = 0;
	OutTotalSize = 0;

	int32 TotalAssets = AssetPathToDiskPath.Num();
	UpdateProgress(TEXT("准备资源"), TEXT(""), 0, TotalAssets, 0, 0);

	int32 Index = 0;
	for (const TPair<FString, FString>& Pair : AssetPathToDiskPath)
	{
		if (bIsCancelled)
		{
			OutErrorMessage = TEXT("构建已取消");
			return false;
		}

		const FString& AssetPath = Pair.Key;
		const FString& DiskPath = Pair.Value;

		if (!PlatformFile.FileExists(*DiskPath))
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("源文件不存在: %s (Asset: %s)"), *DiskPath, *AssetPath);
			continue;
		}

		// 使用 AssetPath 计算 Pak 内部路径
		FString PakInternalPath = GetPakInternalPath(AssetPath);

		// 源路径使用正斜杠
		FString UnixDiskPath = DiskPath;
		UnixDiskPath.ReplaceCharInline('\\', '/');

		ResponseContent += FString::Printf(TEXT("\"%s\" \"%s\"\n"), *UnixDiskPath, *PakInternalPath);

		int64 FileSize = IFileManager::Get().FileSize(*DiskPath);
		OutTotalSize += FileSize;
		OutValidFileCount++;

		UpdateProgress(TEXT("准备资源"), DiskPath, ++Index, TotalAssets, 0, OutTotalSize);
	}

	if (OutValidFileCount == 0)
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("没有有效的资源文件可打包"));
		OutErrorMessage = TEXT("没有有效的资源文件可打包");
		return false;
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("已准备 %d 个资源文件，总大小: %lld 字节"), OutValidFileCount, OutTotalSize);

	if (!FFileHelper::SaveStringToFile(ResponseContent, *ResponseFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法创建响应文件: %s"), *ResponseFilePath);
		OutErrorMessage = TEXT("无法创建响应文件");
		return false;
	}

	return true;
}

bool UHotUpdateIoStoreBuilder::PrepareOutputDirectory(
	const FString& OutputPath,
	FString& OutErrorMessage)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	FString OutputDir = FPaths::GetPath(OutputPath);
	if (!PlatformFile.CreateDirectoryTree(*OutputDir))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法创建输出目录: %s"), *OutputDir);
		OutErrorMessage = TEXT("无法创建输出目录");
		return false;
	}

	// UE5 IoStore 标准格式：清理 .utoc, .ucas, .pak 文件
	TArray<FString> Extensions = { TEXT(".utoc"), TEXT(".ucas"), TEXT(".pak") };
	for (const FString& Ext : Extensions)
	{
		FString ExistingPath = OutputPath + Ext;
		if (PlatformFile.FileExists(*ExistingPath))
		{
			PlatformFile.DeleteFile(*ExistingPath);
			UE_LOG(LogHotUpdateEditor, Log, TEXT("删除已存在的文件: %s"), *ExistingPath);
		}
	}

	return true;
}

FString UHotUpdateIoStoreBuilder::BuildUnrealPakCommandLine(
	const FString& OutputPath,
	const FString& ResponseFilePath,
	const FString& CryptoKeyPath,
	const FHotUpdateIoStoreConfig& Config)
{
	FString ProjectDir = FPaths::ProjectDir();
	ProjectDir.ReplaceCharInline('\\', '/');

	FString CmdLine;

	// UE5 IoStore 打包方式：
	// 使用 -Create 参数配合 -iostore 标志来创建 IoStore 容器 (.utoc + .ucas + .pak)
	// 输出路径不带扩展名，UnrealPak 会自动添加 .utoc 和 .ucas
	if (Config.bUseIoStore)
	{
		// IoStore 方式：使用 -Create 配合 -iostore 标志
		// 响应文件格式与传统 Pak 相同，但添加 -iostore 标志
		CmdLine = FString::Printf(
			TEXT("\"%s.utoc\" -Create=\"%s\" -iostore -projectdir=\"%s\""),
			*OutputPath,
			*ResponseFilePath,
			*ProjectDir);
	}
	else
	{
		// 传统 Pak 方式：只生成 .pak
		CmdLine = FString::Printf(
			TEXT("\"%s.pak\" -Create=\"%s\" -projectdir=\"%s\""),
			*OutputPath,
			*ResponseFilePath,
			*ProjectDir);
	}

	// 添加 MountPoint 参数，指定 Pak 文件的挂载点
	// 热更新 Pak 文件使用 /Game/ 作为挂载点，这样运行时可以正确加载
	CmdLine += TEXT(" -mountpoint=\"/Game/\"");

	if (Config.CompressionFormat != TEXT("None"))
	{
		CmdLine += FString::Printf(TEXT(" -compressionformats=%s"), *Config.CompressionFormat);

		if (Config.CompressionLevel > 0)
		{
			CmdLine += FString::Printf(TEXT(" -compressionlevel=%d"), Config.CompressionLevel);
		}
	}

	if (!Config.EncryptionKey.IsEmpty() && !CryptoKeyPath.IsEmpty())
	{
		CmdLine += FString::Printf(TEXT(" -encrypt -cryptokeys=\"%s\""), *CryptoKeyPath);

		if (Config.bEncryptIndex)
		{
			CmdLine += TEXT(" -encryptindex");
		}
	}

	return CmdLine;
}

bool UHotUpdateIoStoreBuilder::ExecuteUnrealPak(
	const FString& UnrealPakPath,
	const FString& CmdLine,
	const FString& OutputPath,
	FHotUpdateIoStoreResult& OutResult)
{
	int32 ReturnCode = 0;
	FString StdOut;
	FString StdErr;

	UE_LOG(LogHotUpdateEditor, Log, TEXT("执行 UnrealPak: %s %s"), *UnrealPakPath, *CmdLine);
	FPlatformProcess::ExecProcess(*UnrealPakPath, *CmdLine, &ReturnCode, &StdOut, &StdErr);

	if (!StdOut.IsEmpty())
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("UnrealPak 输出: %s"), *StdOut);
	}

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	// 检查 IoStore 格式输出（.utoc + .ucas）
	FString UtocPath = OutputPath + TEXT(".utoc");
	FString UcasPath = OutputPath + TEXT(".ucas");
	FString PakPath = OutputPath + TEXT(".pak");

	bool bUtocExists = PlatformFile.FileExists(*UtocPath);
	bool bUcasExists = PlatformFile.FileExists(*UcasPath);
	bool bPakExists = PlatformFile.FileExists(*PakPath);

	int64 UtocSize = bUtocExists ? IFileManager::Get().FileSize(*UtocPath) : 0;
	int64 UcasSize = bUcasExists ? IFileManager::Get().FileSize(*UcasPath) : 0;
	int64 PakSize = bPakExists ? IFileManager::Get().FileSize(*PakPath) : 0;

	// UE5 IoStore 标准格式：utoc + ucas
	if (bUtocExists && bUcasExists && UtocSize > 0 && UcasSize > 0)
	{
		OutResult.UtocPath = UtocPath;
		OutResult.UcasPath = UcasPath;
		OutResult.ContainerSize = UtocSize + UcasSize + PakSize;
		UE_LOG(LogHotUpdateEditor, Log, TEXT("IoStore 容器创建成功 (utoc + ucas):"));
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  .utoc: %s (%lld 字节)"), *UtocPath, UtocSize);
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  .ucas: %s (%lld 字节)"), *UcasPath, UcasSize);
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  .pak: %s (%lld 字节)"), *PakPath, PakSize);
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  总大小: %lld 字节"), OutResult.ContainerSize);
		return true;
	}

	// UE5 IoStore 单文件格式（仅 .utoc，数据嵌入在 utoc 中）
	// 某些情况下 UnrealPak 只生成 .utoc 文件，包含所有数据
	if (bUtocExists && UtocSize > 0)
	{
		OutResult.UtocPath = UtocPath;
		OutResult.UcasPath = TEXT("");
		OutResult.ContainerSize = UtocSize;
		UE_LOG(LogHotUpdateEditor, Log, TEXT("IoStore 容器创建成功 (仅 utoc):"));
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  .utoc: %s (%lld 字节)"), *UtocPath, UtocSize);
		UE_LOG(LogHotUpdateEditor, Log, TEXT("  总大小: %lld 字节"), OutResult.ContainerSize);
		return true;
	}

	// 传统 Pak 格式（仅 .pak 文件）
	if (bPakExists && PakSize > 0)
	{
		OutResult.UtocPath = PakPath;  // 用 UtocPath 存储 pak 路径（兼容旧逻辑）
		OutResult.UcasPath = TEXT("");
		OutResult.ContainerSize = PakSize;
		UE_LOG(LogHotUpdateEditor, Log, TEXT("Pak 文件已创建: %s, 大小: %lld 字节"), *PakPath, PakSize);
		return true;
	}

	UE_LOG(LogHotUpdateEditor, Error, TEXT("UnrealPak 执行失败，返回码: %d"), ReturnCode);
	UE_LOG(LogHotUpdateEditor, Error, TEXT("  .utoc 存在: %s, 大小: %lld"), bUtocExists ? TEXT("true") : TEXT("false"), UtocSize);
	UE_LOG(LogHotUpdateEditor, Error, TEXT("  .ucas 存在: %s, 大小: %lld"), bUcasExists ? TEXT("true") : TEXT("false"), UcasSize);
	UE_LOG(LogHotUpdateEditor, Error, TEXT("  .pak 存在: %s, 大小: %lld"), bPakExists ? TEXT("true") : TEXT("false"), PakSize);

	if (!StdErr.IsEmpty())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("错误信息: %s"), *StdErr);
	}
	OutResult.ErrorMessage = FString::Printf(TEXT("UnrealPak 执行失败: %s"), *StdErr);
	return false;
}

void UHotUpdateIoStoreBuilder::CleanupTempDirectory(const FString& TempDir)
{
	IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*TempDir);
}

FString UHotUpdateIoStoreBuilder::GenerateCryptoKeyFile(
	const FString& TempDir,
	const FHotUpdateIoStoreConfig& Config)
{
	if (Config.EncryptionKey.IsEmpty())
	{
		return TEXT("");
	}

	FString CryptoPath = FPaths::Combine(TempDir, TEXT("Crypto.json"));

	FString CryptoContent = FString::Printf(TEXT(
		"{\n"
		"  \"EncryptionKey\": {\n"
		"    \"Name\": \"%s\",\n"
		"    \"Guid\": \"%s\",\n"
		"    \"Key\": \"%s\"\n"
		"  },\n"
		"  \"bEnablePakSigning\": false\n"
		"}"),
		*Config.ContainerName,
		*FGuid::NewGuid().ToString(),
		*Config.EncryptionKey
	);

	if (FFileHelper::SaveStringToFile(CryptoContent, *CryptoPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return CryptoPath;
	}

	return TEXT("");
}

void UHotUpdateIoStoreBuilder::UpdateProgress(
	const FString& Stage,
	const FString& CurrentFile,
	int32 ProcessedFiles,
	int32 TotalFiles,
	int64 ProcessedBytes,
	int64 TotalBytes)
{
	{
		FScopeLock Lock(&ProgressCriticalSection);
		CurrentProgress.CurrentStage = Stage;
		CurrentProgress.CurrentFile = CurrentFile;
		CurrentProgress.ProcessedFiles = ProcessedFiles;
		CurrentProgress.TotalFiles = TotalFiles;
		CurrentProgress.ProcessedBytes = ProcessedBytes;
		CurrentProgress.TotalBytes = TotalBytes;
		CurrentProgress.bIsComplete = (ProcessedFiles >= TotalFiles && TotalFiles > 0);
	}

	OnProgress.Broadcast(CurrentProgress);
}