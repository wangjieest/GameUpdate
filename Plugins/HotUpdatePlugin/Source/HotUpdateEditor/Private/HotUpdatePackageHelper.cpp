// Copyright czm. All Rights Reserved.

#include "HotUpdatePackageHelper.h"
#include "HotUpdateEditor.h"
#include "HotUpdateUtils.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "JsonObjectConverter.h"
#include "Interfaces/IPluginManager.h"

bool FHotUpdatePackageHelper::CompileProject(EHotUpdatePlatform Platform)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("开始编译项目..."));

	FString EngineDir = FPaths::EngineDir();
	const FString UBTPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(EngineDir, TEXT("Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll")));

	const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString PlatformName = HotUpdateUtils::GetPlatformDirectoryName(Platform);
	const FString BuildConfig = TEXT("Development");

	const FString Params = FString::Printf(
		TEXT("\"%s\" GameUpdate %s %s -project=\"%s\""),
		*UBTPath, *PlatformName, *BuildConfig, *ProjectPath);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("执行编译: dotnet %s"), *Params);

	const FString CommandLine = FString::Printf(TEXT("/c dotnet %s"), *Params);

	FMonitoredProcess Process(TEXT("cmd.exe"), CommandLine, true);

	Process.OnOutput().BindLambda([](const FString& Output)
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("%s"), *Output);
	});

	if (!Process.Launch())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法启动编译进程"));
		return false;
	}

	while (Process.Update())
	{
		FPlatformProcess::Sleep(0.1f);
	}

	int32 ReturnCode = Process.GetReturnCode();

	if (ReturnCode != 0)
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("编译失败，返回码: %d"), ReturnCode);
		return false;
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("编译完成"));
	return true;
}

bool FHotUpdatePackageHelper::CookAssets(EHotUpdatePlatform Platform)
{
	return CookAssets(Platform, TArray<FString>());
}

bool FHotUpdatePackageHelper::CookAssets(EHotUpdatePlatform Platform, const TArray<FString>& AssetsToCook)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("开始 Cook 资源..."));

	FString EngineDir = FPaths::EngineDir();
#if PLATFORM_WINDOWS
	FString ExePath = FPaths::ConvertRelativePathToFull(EngineDir / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe"));
#elif PLATFORM_MAC
	FString ExePath = FPaths::ConvertRelativePathToFull(EngineDir / TEXT("Binaries/Mac/UnrealEditor-Cmd"));
#else
	FString ExePath = FPaths::ConvertRelativePathToFull(EngineDir / TEXT("Binaries/Win64/UnrealEditor-Cmd.exe"));
#endif
	const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString CookPlatform = HotUpdateUtils::GetPlatformString(Platform);

	FString Params;
	if (AssetsToCook.Num() > 0)
	{
		FString PackageList;
		for (int32 i = 0; i < AssetsToCook.Num(); i++)
		{
			if (i > 0) PackageList += TEXT("+");
			PackageList += AssetsToCook[i];
		}

		Params = FString::Printf(TEXT("\"%s\" -run=cook -targetplatform=%s -PACKAGE=%s -cooksinglepackage -NullRHI -unattended -NoSound"),
			*ProjectPath, *CookPlatform, *PackageList);

		UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook: 只 Cook %d 个资源"), AssetsToCook.Num());
	}
	else
	{
		Params = FString::Printf(TEXT("\"%s\" -run=cook -targetplatform=%s -NullRHI -unattended -NoSound"),
			*ProjectPath, *CookPlatform);

		UE_LOG(LogHotUpdateEditor, Log, TEXT("全量 Cook"));
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("执行 Cook: %s %s"), *ExePath, *Params);

	FMonitoredProcess Process(ExePath, Params, true);

	Process.OnOutput().BindLambda([](const FString& Output)
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("%s"), *Output);
	});

	if (!Process.Launch())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法启动 Cook 进程"));
		return false;
	}

	while (Process.Update())
	{
		FPlatformProcess::Sleep(0.1f);
	}

	int32 ReturnCode = Process.GetReturnCode();

	if (ReturnCode != 0)
	{
		bool bIsIncremental = AssetsToCook.Num() > 0;
		if (bIsIncremental && ReturnCode == 1)
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("增量 Cook 返回警告码 1，检查 Cook 输出..."));
			FString CookedPlatformDir = HotUpdateUtils::GetCookedPlatformDir(Platform);
			int32 FoundCount = 0;
			for (const FString& AssetPath : AssetsToCook)
			{
				FString DiskPath = GetAssetDiskPath(AssetPath, CookedPlatformDir);
				if (!DiskPath.IsEmpty() && FPaths::FileExists(*DiskPath))
				{
					FoundCount++;
				}
			}
			if (FoundCount > 0)
			{
				UE_LOG(LogHotUpdateEditor, Log, TEXT("增量 Cook: %d/%d 个目标文件已生成，视为成功"), FoundCount, AssetsToCook.Num());
				return true;
			}
		}
		UE_LOG(LogHotUpdateEditor, Error, TEXT("Cook 失败，返回码: %d"), ReturnCode);
		return false;
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("Cook 完成"));
	return true;
}

FString FHotUpdatePackageHelper::GetAssetDiskPath(const FString& AssetPath, const FString& CookedPlatformDir)
{
	if (CookedPlatformDir.IsEmpty())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("GetAssetDiskPath: CookedPlatformDir 不能为空"));
		return TEXT("");
	}

	// Script 包是原生类，无法解析为文件路径
	if (FPackageName::IsScriptPackage(AssetPath) || FPackageName::IsMemoryPackage(AssetPath))
	{
		return TEXT("");
	}

	FString ResolvedPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(AssetPath, ResolvedPath, FString()))
	{
		return TEXT("");
	}

	// 转换为绝对路径
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(ResolvedPath);
	FPaths::NormalizeFilename(AbsolutePath);
	
	FString RelativePath;

	// 简化判断：包含 Plugins/ → 插件资源，包含 Engine → 引擎资源，否则 → 项目资源
	if (AbsolutePath.Contains(TEXT("Plugins/")))
	{
		// 插件资源：查找 Engine 或 Project 目录作为基准
		const int32 PluginsIdx = AbsolutePath.Find(TEXT("Plugins/"));
		const int32 EngineIdx = AbsolutePath.Find(TEXT("Engine/"));
		const FString ProjectName = FApp::GetProjectName();
		const int32 ProjectIdx = AbsolutePath.Find(ProjectName + TEXT("/"));

		if (EngineIdx != INDEX_NONE && EngineIdx < PluginsIdx)
		{
			// 引擎插件：Engine/Plugins/...
			RelativePath = AbsolutePath.Mid(EngineIdx);
		}
		else if (ProjectIdx != INDEX_NONE && ProjectIdx < PluginsIdx)
		{
			// 项目插件：{ProjectName}/Plugins/...
			RelativePath = ProjectName / AbsolutePath.Mid(ProjectIdx + ProjectName.Len() + 1);
		}
		else
		{
			// 无法确定归属，保留 Plugins 前路径
			int32 PrefixEnd = PluginsIdx;
			while (PrefixEnd > 0 && AbsolutePath[PrefixEnd - 1] != '/') PrefixEnd--;
			RelativePath = AbsolutePath.Mid(PrefixEnd);
		}
	}
	else if (AbsolutePath.Contains(TEXT("Engine/")))
	{
		// 引擎资源（非插件）
		const int32 EngineIdx = AbsolutePath.Find(TEXT("Engine/"));
		RelativePath = AbsolutePath.Mid(EngineIdx);
	}
	else
	{
		// 项目资源
		const FString ProjectName = FApp::GetProjectName();
		const int32 ProjectIdx = AbsolutePath.Find(ProjectName + TEXT("/"));
		if (ProjectIdx != INDEX_NONE)
			RelativePath = AbsolutePath.Mid(ProjectIdx);
		else
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("GetAssetDiskPath: 无法识别项目路径 %s"), *AssetPath);
			return TEXT("");
		}
	}

	FString CookedMapPath = FPaths::Combine(CookedPlatformDir, RelativePath + TEXT(".umap"));
	if (FPaths::FileExists(*CookedMapPath))
		return CookedMapPath;

	FString CookedAssetPath = FPaths::Combine(CookedPlatformDir, RelativePath + TEXT(".uasset"));
	if (FPaths::FileExists(*CookedAssetPath))
		return CookedAssetPath;

	return TEXT("");
}

FString FHotUpdatePackageHelper::GetAssetSourcePath(const FString& AssetPath)
{
	FString ResolvedPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(AssetPath, ResolvedPath, TEXT("")))
	{
		return TEXT("");
	}

	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(ResolvedPath);

	if (FPaths::FileExists(AbsolutePath + TEXT(".umap")))
		return AbsolutePath + TEXT(".umap");
	if (FPaths::FileExists(AbsolutePath + TEXT(".uasset")))
		return AbsolutePath + TEXT(".uasset");

	return TEXT("");
}

FString FHotUpdatePackageHelper::ConvertAssetPathToFileName(const FString& AssetPath, const FString& CookedPlatformDir)
{
	// Script 包是原生类，没有对应的文件
	if (FPackageName::IsScriptPackage(AssetPath))
	{
		return TEXT("");
	}

	// 使用 GetAssetDiskPath 获取实际 Cooked 文件路径
	const FString DiskPath = GetAssetDiskPath(AssetPath, CookedPlatformDir);
	if (DiskPath.IsEmpty())
	{
		return TEXT("");
	}

	// 从 Cooked 文件路径提取 RelativePath（去掉 CookedPlatformDir）
	FString RelativePath;
	if (DiskPath.StartsWith(CookedPlatformDir))
	{
		RelativePath = DiskPath.Mid(CookedPlatformDir.Len());
	}
	else
	{
		RelativePath = DiskPath;
	}

	return RelativePath;
}

FString FHotUpdatePackageHelper::FileNameToAssetPath(const FString& FileName)
{
	FString AssetPath = FileName;

	if (!AssetPath.StartsWith(TEXT("/")))
	{
		AssetPath = TEXT("/") + AssetPath;
	}

	if (AssetPath.EndsWith(TEXT(".uasset")))
	{
		AssetPath.LeftChopInline(7);
	}
	else if (AssetPath.EndsWith(TEXT(".umap")))
	{
		AssetPath.LeftChopInline(5);
	}

	return AssetPath;
}