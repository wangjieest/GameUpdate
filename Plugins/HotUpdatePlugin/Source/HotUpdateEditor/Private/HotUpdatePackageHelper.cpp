// Copyright czm. All Rights Reserved.

#include "HotUpdatePackageHelper.h"
#include "HotUpdateEditor.h"
#include "HotUpdateUtils.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

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
				FString DiskPath = GetCookedAssetPath(AssetPath, CookedPlatformDir);
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

FString FHotUpdatePackageHelper::GetCookedAssetPath(const FString& AssetPath, const FString& CookedPlatformDir)
{
	if (CookedPlatformDir.IsEmpty())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("GetCookedAssetPath: CookedPlatformDir 不能为空"));
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
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("GetCookedAssetPath: 无法识别项目路径 %s"), *AssetPath);
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

	// 使用 GetCookedAssetPath 获取实际 Cooked 文件路径
	const FString DiskPath = GetCookedAssetPath(AssetPath, CookedPlatformDir);
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
	FString Result = FileName;
	FPaths::NormalizeFilename(Result);

	// Staged 文件（非 .uasset/.umap）：使用 Pak 路径格式 /{ProjectName}/Content/xxx
	if (!Result.EndsWith(TEXT(".uasset")) && !Result.EndsWith(TEXT(".umap")))
	{
		const FString ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		if (Result.StartsWith(ProjectContentDir))
		{
			FString RelativePath = Result.RightChop(ProjectContentDir.Len());
			return TEXT("/") + FString(FApp::GetProjectName()) + TEXT("/Content/") + RelativePath;
		}
		return Result;
	}

	// UE 资产：使用引擎标准 API 转换为 Long Package Name
	FString LongPackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(Result, LongPackageName))
	{
		UE_LOG(LogHotUpdateEditor, Display, TEXT("FileNameToAssetPath: %s -> %s"), *FileName, *LongPackageName);
		return LongPackageName;
	}
	
	UE_LOG(LogHotUpdateEditor, Display, TEXT("TryConvertFilenameToLongPackageName FAILED: %s"), *Result);
	return Result;
}

FString FHotUpdatePackageHelper::AssetPathToPakPath(const FString& AssetPath, const FString& Extension)
{
	FString Result = AssetPath;

	// 添加扩展名
	if (!Extension.IsEmpty() && !Result.EndsWith(Extension))
	{
		Result += Extension;
	}

	// /Game/xxx -> /{ProjectName}/Content/xxx
	if (Result.StartsWith(TEXT("/Game/")))
	{
		Result = TEXT("/") + FString(FApp::GetProjectName()) + TEXT("/Content/") + Result.RightChop(6);
	}
	// /Engine/xxx -> /Engine/Content/xxx（引擎原生资源）
	else if (Result.StartsWith(TEXT("/Engine/")) && !Result.StartsWith(TEXT("/Engine/Plugins/")))
	{
		Result = TEXT("/Engine/Content/") + Result.RightChop(8);
	}
	// /{Plugin}/xxx -> /Engine/Plugins/{Plugin}/Content/xxx（插件资源，非标准路径）
	else if (!Result.StartsWith(TEXT("/Game/")) && !Result.StartsWith(TEXT("/Engine/")) && !Result.StartsWith(TEXT("/Script/")))
	{
		// 提取插件名（第一个路径段）
		int32 FirstSlash = Result.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
		if (FirstSlash != INDEX_NONE)
		{
			FString PluginName = Result.Mid(1, FirstSlash - 1);
			FString RestPath = Result.RightChop(FirstSlash);
			Result = TEXT("/Engine/Plugins/") + PluginName + TEXT("/Content") + RestPath;
		}
	}

	return Result;
}


FString FHotUpdatePackageHelper::GetAssetExtension(const FString& AssetPath, const FString& CookedPlatformDir)
{
	FString CookedPath = GetCookedAssetPath(AssetPath, CookedPlatformDir);
	if (CookedPath.IsEmpty())
	{
		return TEXT("");
	}
	return FPaths::GetExtension(CookedPath);
}
