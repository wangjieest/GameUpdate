// Copyright czm. All Rights Reserved.

#include "HotUpdateDiffTool.h"
#include "HotUpdateEditor.h"
#include "Core/HotUpdateFileUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "JsonObjectConverter.h"
#include "IPlatformFilePak.h"

UHotUpdateDiffTool::UHotUpdateDiffTool()
	: AssetRegistry(nullptr)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistry = &AssetRegistryModule.Get();
}

FHotUpdateDiffReport UHotUpdateDiffTool::CompareDirectories(
	const FString& BaseDirectory,
	const FString& TargetDirectory,
	bool bRecursive)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("CompareDirectories: Base='%s', Target='%s'"),
		*BaseDirectory, *TargetDirectory);

	FHotUpdateDiffReport Report;
	Report.BaseVersion = BaseDirectory;
	Report.TargetVersion = TargetDirectory;

	// 扫描基础目录
	TMap<FString, FHotUpdateAssetDiff> BaseAssets;
	ScanDirectory(BaseDirectory, bRecursive, BaseAssets);

	// 扫描目标目录
	TMap<FString, FHotUpdateAssetDiff> TargetAssets;
	ScanDirectory(TargetDirectory, bRecursive, TargetAssets);

	// 比较差异
	TSet<FString> AllPaths;
	for (const auto& Pair : BaseAssets)
	{
		AllPaths.Add(Pair.Key);
	}
	for (const auto& Pair : TargetAssets)
	{
		AllPaths.Add(Pair.Key);
	}

	for (const FString& Path : AllPaths)
	{
		bool bInBase = BaseAssets.Contains(Path);
		bool bInTarget = TargetAssets.Contains(Path);

		FHotUpdateAssetDiff Diff;
		Diff.AssetPath = Path;

		if (!bInBase && bInTarget)
		{
			// 新增
			Diff = TargetAssets[Path];
			Diff.ChangeType = EHotUpdateFileChangeType::Added;
			Diff.ChangeDescription = FString::Printf(TEXT("新增资源 (%s)"), *FormatFileSize(Diff.NewSize));
			Report.AddedAssets.Add(Diff);
		}
		else if (bInBase && !bInTarget)
		{
			// 删除
			Diff = BaseAssets[Path];
			Diff.ChangeType = EHotUpdateFileChangeType::Deleted;
			Diff.ChangeDescription = FString::Printf(TEXT("删除资源 (%s)"), *FormatFileSize(Diff.OldSize));
			Report.DeletedAssets.Add(Diff);
		}
		else if (bInBase && bInTarget)
		{
			// 比较Hash
			const FHotUpdateAssetDiff& BaseDiff = BaseAssets[Path];
			const FHotUpdateAssetDiff& TargetDiff = TargetAssets[Path];

			// 注意：ScanDirectory 扫描结果存储在 NewHash 和 NewSize 字段
			// 所以比较时应该用 BaseDiff.NewHash 和 TargetDiff.NewHash
			if (BaseDiff.NewHash != TargetDiff.NewHash)
			{
				// 修改
				Diff = TargetDiff;
				Diff.ChangeType = EHotUpdateFileChangeType::Modified;
				// OldSize 和 OldHash 应该从 BaseDiff 的 NewSize 和 NewHash 获取
				Diff.OldSize = BaseDiff.NewSize;
				Diff.OldHash = BaseDiff.NewHash;

				int64 SizeDiff = Diff.NewSize - Diff.OldSize;
				FString SizeDiffStr = SizeDiff >= 0
					? FString::Printf(TEXT("+%s"), *FormatFileSize(SizeDiff))
					: FString::Printf(TEXT("-%s"), *FormatFileSize(-SizeDiff));

				Diff.ChangeDescription = FString::Printf(TEXT("已修改 (大小变化: %s)"), *SizeDiffStr);
				Report.ModifiedAssets.Add(Diff);
			}
			else
			{
				// 未变更
				Diff.ChangeType = EHotUpdateFileChangeType::Unchanged;
				Diff.ChangeDescription = TEXT("未变更");
				Report.UnchangedAssets.Add(Diff);
			}
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("CompareDirectories result: Added=%d, Modified=%d, Deleted=%d, Unchanged=%d"),
		Report.AddedAssets.Num(), Report.ModifiedAssets.Num(), Report.DeletedAssets.Num(), Report.UnchangedAssets.Num());

	OnDiffComplete.Broadcast(Report);
	return Report;
}

FHotUpdateDiffReport UHotUpdateDiffTool::CompareManifests(
	const FString& BaseManifestPath,
	const FString& TargetManifestPath)
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("CompareManifests: Base='%s', Target='%s'"),
		*BaseManifestPath, *TargetManifestPath);

	FHotUpdateDiffReport Report;
	Report.BaseVersion = BaseManifestPath;
	Report.TargetVersion = TargetManifestPath;

	// 解析基础Manifest
	TMap<FString, FHotUpdateManifestEntry> BaseEntries;
	if (!ParseManifestFile(BaseManifestPath, BaseEntries))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法解析基础Manifest: %s"), *BaseManifestPath);
		return Report;
	}

	// 解析目标Manifest
	TMap<FString, FHotUpdateManifestEntry> TargetEntries;
	if (!ParseManifestFile(TargetManifestPath, TargetEntries))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("无法解析目标Manifest: %s"), *TargetManifestPath);
		return Report;
	}

	// 收集所有路径
	TSet<FString> AllPaths;
	for (const auto& Pair : BaseEntries)
	{
		AllPaths.Add(Pair.Key);
	}
	for (const auto& Pair : TargetEntries)
	{
		AllPaths.Add(Pair.Key);
	}

	// 比较差异
	for (const FString& Path : AllPaths)
	{
		bool bInBase = BaseEntries.Contains(Path);
		bool bInTarget = TargetEntries.Contains(Path);

		FHotUpdateAssetDiff Diff;
		Diff.AssetPath = Path;

		if (!bInBase && bInTarget)
		{
			const FHotUpdateManifestEntry& Entry = TargetEntries[Path];
			Diff.ChangeType = EHotUpdateFileChangeType::Added;
			Diff.NewSize = Entry.FileSize;
			Diff.NewHash = Entry.FileHash;
			Diff.AssetType = GetAssetTypeFromExtension(FPaths::GetExtension(Path));
			Diff.ChangeDescription = FString::Printf(TEXT("新增资源 (%s)"), *FormatFileSize(Diff.NewSize));
			Report.AddedAssets.Add(Diff);
		}
		else if (bInBase && !bInTarget)
		{
			const FHotUpdateManifestEntry& Entry = BaseEntries[Path];
			Diff.ChangeType = EHotUpdateFileChangeType::Deleted;
			Diff.OldSize = Entry.FileSize;
			Diff.OldHash = Entry.FileHash;
			Diff.AssetType = GetAssetTypeFromExtension(FPaths::GetExtension(Path));
			Diff.ChangeDescription = FString::Printf(TEXT("删除资源 (%s)"), *FormatFileSize(Diff.OldSize));
			Report.DeletedAssets.Add(Diff);
		}
		else if (bInBase && bInTarget)
		{
			const FHotUpdateManifestEntry& BaseEntry = BaseEntries[Path];
			const FHotUpdateManifestEntry& TargetEntry = TargetEntries[Path];

			if (BaseEntry.FileHash != TargetEntry.FileHash)
			{
				Diff.ChangeType = EHotUpdateFileChangeType::Modified;
				Diff.OldSize = BaseEntry.FileSize;
				Diff.OldHash = BaseEntry.FileHash;
				Diff.NewSize = TargetEntry.FileSize;
				Diff.NewHash = TargetEntry.FileHash;
				Diff.AssetType = GetAssetTypeFromExtension(FPaths::GetExtension(Path));

				int64 SizeDiff = Diff.NewSize - Diff.OldSize;
				FString SizeDiffStr = SizeDiff >= 0
					? FString::Printf(TEXT("+%s"), *FormatFileSize(SizeDiff))
					: FString::Printf(TEXT("-%s"), *FormatFileSize(-SizeDiff));

				Diff.ChangeDescription = FString::Printf(TEXT("已修改 (大小变化: %s)"), *SizeDiffStr);
				Report.ModifiedAssets.Add(Diff);
			}
			else
			{
				Diff.ChangeType = EHotUpdateFileChangeType::Unchanged;
				Diff.ChangeDescription = TEXT("未变更");
				Report.UnchangedAssets.Add(Diff);
			}
		}
	}

	// 从 manifest JSON 中提取版本信息
	auto ExtractVersionFromManifest = [](const FString& ManifestPath) -> FString {
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath)) return TEXT("");

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return TEXT("");

		// 新格式：version.version
		const TSharedPtr<FJsonObject>* VersionObj;
		if (JsonObject->TryGetObjectField(TEXT("version"), VersionObj))
		{
			if (VersionObj->Get()->HasField(TEXT("version")))
			{
				return VersionObj->Get()->GetStringField(TEXT("version"));
			}
		}
		// 旧格式：versionInfo.versionString
		const TSharedPtr<FJsonObject>* VersionInfoObj;
		if (JsonObject->TryGetObjectField(TEXT("versionInfo"), VersionInfoObj))
		{
			if (VersionInfoObj->Get()->HasField(TEXT("versionString")))
			{
				return VersionInfoObj->Get()->GetStringField(TEXT("versionString"));
			}
		}
		return TEXT("");
	};

	FString BaseVersionStr = ExtractVersionFromManifest(BaseManifestPath);
	FString TargetVersionStr = ExtractVersionFromManifest(TargetManifestPath);

	if (!BaseVersionStr.IsEmpty()) Report.BaseVersion = BaseVersionStr;
	if (!TargetVersionStr.IsEmpty()) Report.TargetVersion = TargetVersionStr;

	UE_LOG(LogHotUpdateEditor, Log, TEXT("CompareManifests result: Added=%d, Modified=%d, Deleted=%d, Unchanged=%d (Base=%s, Target=%s)"),
		Report.AddedAssets.Num(), Report.ModifiedAssets.Num(), Report.DeletedAssets.Num(), Report.UnchangedAssets.Num(),
		*Report.BaseVersion, *Report.TargetVersion);

	OnDiffComplete.Broadcast(Report);
	return Report;
}

FHotUpdateDiffReport UHotUpdateDiffTool::ComparePakFiles(
	const FString& BasePakPath,
	const FString& TargetPakPath)
{
	FHotUpdateDiffReport Report;
	Report.BaseVersion = BasePakPath;
	Report.TargetVersion = TargetPakPath;

	// 从 Pak 文件中提取文件名和 Hash
	TMap<FString, FString> BaseFileHashes = GetPakFileHashes(BasePakPath);
	TMap<FString, FString> TargetFileHashes = GetPakFileHashes(TargetPakPath);

	// 合并所有路径
	TSet<FString> AllPaths;
	for (const auto& Pair : BaseFileHashes)
	{
		AllPaths.Add(Pair.Key);
	}
	for (const auto& Pair : TargetFileHashes)
	{
		AllPaths.Add(Pair.Key);
	}

	// 比较差异
	for (const FString& Path : AllPaths)
	{
		bool bInBase = BaseFileHashes.Contains(Path);
		bool bInTarget = TargetFileHashes.Contains(Path);

		FHotUpdateAssetDiff Diff;
		Diff.AssetPath = Path;
		Diff.AssetType = GetAssetTypeFromExtension(FPaths::GetExtension(Path));

		if (!bInBase && bInTarget)
		{
			Diff.ChangeType = EHotUpdateFileChangeType::Added;
			Diff.NewHash = TargetFileHashes[Path];
			Diff.ChangeDescription = TEXT("新增资源");
			Report.AddedAssets.Add(Diff);
		}
		else if (bInBase && !bInTarget)
		{
			Diff.ChangeType = EHotUpdateFileChangeType::Deleted;
			Diff.OldHash = BaseFileHashes[Path];
			Diff.ChangeDescription = TEXT("删除资源");
			Report.DeletedAssets.Add(Diff);
		}
		else if (bInBase && bInTarget)
		{
			if (BaseFileHashes[Path] != TargetFileHashes[Path])
			{
				Diff.ChangeType = EHotUpdateFileChangeType::Modified;
				Diff.OldHash = BaseFileHashes[Path];
				Diff.NewHash = TargetFileHashes[Path];
				Diff.ChangeDescription = TEXT("已修改");
				Report.ModifiedAssets.Add(Diff);
			}
			else
			{
				Diff.ChangeType = EHotUpdateFileChangeType::Unchanged;
				Diff.ChangeDescription = TEXT("未变更");
				Report.UnchangedAssets.Add(Diff);
			}
		}
	}

	OnDiffComplete.Broadcast(Report);
	return Report;
}

TMap<FString, FString> UHotUpdateDiffTool::ComputeDirectoryHashes(const FString& Directory, bool bRecursive)
{
	TMap<FString, FString> Hashes;

	// 规范化目录路径：统一分隔符，移除尾部斜杠
	FString NormalizedDir = Directory;
	FPaths::NormalizeDirectoryName(NormalizedDir);
	NormalizedDir.ReplaceInline(TEXT("\\"), TEXT("/"));

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFilesRecursive(FoundFiles, *NormalizedDir, TEXT("*.*"), true, false, bRecursive);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("ComputeDirectoryHashes: Scanning '%s', found %d files"), *NormalizedDir, FoundFiles.Num());

	for (const FString& File : FoundFiles)
	{
		// 使用字符串截取获取相对路径，而非 MakePathRelativeTo
		// 因为 MakePathRelativeTo 内部会取 InRelativeTo 的父目录，导致相对路径包含目录名本身
		FString RelativePath = File;
		RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (RelativePath.StartsWith(NormalizedDir))
		{
			RelativePath = RelativePath.RightChop(NormalizedDir.Len());
			// 移除开头的路径分隔符
			while (RelativePath.Len() > 0 && (RelativePath[0] == TEXT('/') || RelativePath[0] == TEXT('\\')))
			{
				RelativePath.RightChopInline(1);
			}
		}
		FString Hash = UHotUpdateFileUtils::CalculateFileHash(File);
		Hashes.Add(RelativePath, Hash);
	}

	return Hashes;
}

FName UHotUpdateDiffTool::GetAssetIconName(const FString& AssetPath)
{
	FString Extension = FPaths::GetExtension(AssetPath).ToLower();

	static TMap<FString, FName> ExtensionToIcon;
	if (ExtensionToIcon.Num() == 0)
	{
		ExtensionToIcon.Add(TEXT("uasset"), FName("ClassIcon.BlueprintCore"));
		ExtensionToIcon.Add(TEXT("umap"), FName("ClassIcon.World"));
		ExtensionToIcon.Add(TEXT("png"), FName("ClassIcon.Texture2D"));
		ExtensionToIcon.Add(TEXT("tga"), FName("ClassIcon.Texture2D"));
		ExtensionToIcon.Add(TEXT("jpg"), FName("ClassIcon.Texture2D"));
		ExtensionToIcon.Add(TEXT("wav"), FName("ClassIcon.SoundWave"));
		ExtensionToIcon.Add(TEXT("fbx"), FName("ClassIcon.StaticMesh"));
		ExtensionToIcon.Add(TEXT("obj"), FName("ClassIcon.StaticMesh"));
	}

	if (ExtensionToIcon.Contains(Extension))
	{
		return ExtensionToIcon[Extension];
	}

	return FName("ClassIcon.Object");
}

FText UHotUpdateDiffTool::GetAssetTypeDisplayName(const FString& AssetPath)
{
	FString Extension = FPaths::GetExtension(AssetPath).ToLower();

	static TMap<FString, FText> ExtensionToDisplayName;
	if (ExtensionToDisplayName.Num() == 0)
	{
		ExtensionToDisplayName.Add(TEXT("uasset"), FText::FromString(TEXT("Asset")));
		ExtensionToDisplayName.Add(TEXT("umap"), FText::FromString(TEXT("Map")));
		ExtensionToDisplayName.Add(TEXT("png"), FText::FromString(TEXT("Texture")));
		ExtensionToDisplayName.Add(TEXT("tga"), FText::FromString(TEXT("Texture")));
		ExtensionToDisplayName.Add(TEXT("jpg"), FText::FromString(TEXT("Texture")));
		ExtensionToDisplayName.Add(TEXT("wav"), FText::FromString(TEXT("Sound")));
		ExtensionToDisplayName.Add(TEXT("fbx"), FText::FromString(TEXT("Mesh")));
		ExtensionToDisplayName.Add(TEXT("obj"), FText::FromString(TEXT("Mesh")));
		ExtensionToDisplayName.Add(TEXT("pak"), FText::FromString(TEXT("Pak File")));
		ExtensionToDisplayName.Add(TEXT("utoc"), FText::FromString(TEXT("IoStore TOC")));
		ExtensionToDisplayName.Add(TEXT("ucas"), FText::FromString(TEXT("IoStore CAS")));
	}

	if (ExtensionToDisplayName.Contains(Extension))
	{
		return ExtensionToDisplayName[Extension];
	}

	return FText::FromString(FPaths::GetExtension(AssetPath));
}

void UHotUpdateDiffTool::ScanDirectory(
	const FString& Directory,
	bool bIncludeHiddenFiles,
	TMap<FString, FHotUpdateAssetDiff>& OutAssets)
{
	// 规范化目录路径：统一分隔符，移除尾部斜杠
	FString NormalizedDir = Directory;
	FPaths::NormalizeDirectoryName(NormalizedDir);
	// 统一使用正斜杠，确保与 FindFilesRecursive 返回的路径格式匹配
	NormalizedDir.ReplaceInline(TEXT("\\"), TEXT("/"));

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	TArray<FString> FoundFiles;
	// 注意：FindFilesRecursive 始终递归搜索，第 6 个参数是 bFindHidden（是否包含隐藏文件）
	IFileManager::Get().FindFilesRecursive(FoundFiles, *NormalizedDir, TEXT("*.*"), true, false, bIncludeHiddenFiles);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("ScanDirectory: Scanning '%s' (normalized from '%s'), found %d files"),
		*NormalizedDir, *Directory, FoundFiles.Num());

	for (const FString& File : FoundFiles)
	{
		// 使用字符串截取获取相对路径，而非 MakePathRelativeTo
		// 因为 MakePathRelativeTo 内部会取 InRelativeTo 的父目录，导致相对路径包含目录名本身
		FString RelativePath = File;
		// 统一使用正斜杠，确保与 NormalizedDir 的 StartsWith 匹配
		RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (RelativePath.StartsWith(NormalizedDir))
		{
			RelativePath = RelativePath.RightChop(NormalizedDir.Len());
			// 移除开头的路径分隔符
			while (RelativePath.Len() > 0 && (RelativePath[0] == TEXT('/') || RelativePath[0] == TEXT('\\')))
			{
				RelativePath.RightChopInline(1);
			}
		}
		else
		{
			UE_LOG(LogHotUpdateEditor, Warning, TEXT("ScanDirectory: File '%s' does not start with dir '%s', using full path"), *File, *NormalizedDir);
		}

		UE_LOG(LogHotUpdateEditor, Verbose, TEXT("ScanDirectory: RelativePath='%s' (from '%s')"), *RelativePath, *File);

		FHotUpdateAssetDiff Diff;
		Diff.AssetPath = RelativePath;
		Diff.NewSize = PlatformFile.FileSize(*File);
		Diff.NewHash = UHotUpdateFileUtils::CalculateFileHash(File);
		Diff.AssetType = GetAssetTypeFromExtension(FPaths::GetExtension(File));

		OutAssets.Add(RelativePath, Diff);
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("ScanDirectory: Added %d assets from '%s'"), OutAssets.Num(), *NormalizedDir);
}

bool UHotUpdateDiffTool::ParseManifestFile(
	const FString& ManifestPath,
	TMap<FString, FHotUpdateManifestEntry>& OutEntries)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("ParseManifestFile: Cannot load file '%s'"), *ManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("ParseManifestFile: Cannot parse JSON '%s'"), *ManifestPath);
		return false;
	}

	// 优先解析 files 数组（filemanifest.json 格式）
	const TArray<TSharedPtr<FJsonValue>>* FilesArray;
	if (JsonObject->TryGetArrayField(TEXT("files"), FilesArray))
	{
		for (const TSharedPtr<FJsonValue>& FileValue : *FilesArray)
		{
			TSharedPtr<FJsonObject> FileObj = FileValue->AsObject();
			if (!FileObj.IsValid()) continue;

			FHotUpdateManifestEntry Entry;
			Entry.FilePath = FileObj->GetStringField(TEXT("filePath"));
			Entry.FileSize = (int64)FileObj->GetNumberField(TEXT("fileSize"));
			Entry.FileHash = FileObj->GetStringField(TEXT("fileHash"));

			// 可选字段
			if (FileObj->HasField(TEXT("chunkId")))
			{
				Entry.ChunkId = FileObj->GetIntegerField(TEXT("chunkId"));
			}
			if (FileObj->HasField(TEXT("source")))
			{
				Entry.Source = FileObj->GetStringField(TEXT("source"));
			}

			OutEntries.Add(Entry.FilePath, Entry);
		}

		UE_LOG(LogHotUpdateEditor, Log, TEXT("ParseManifestFile: Parsed %d entries from 'files' array in '%s'"),
			OutEntries.Num(), *ManifestPath);
		return true;
	}

	// 兼容旧格式 assets 数组
	const TArray<TSharedPtr<FJsonValue>>* AssetsArray;
	if (JsonObject->TryGetArrayField(TEXT("assets"), AssetsArray))
	{
		for (const TSharedPtr<FJsonValue>& AssetValue : *AssetsArray)
		{
			TSharedPtr<FJsonObject> AssetObj = AssetValue->AsObject();
			if (!AssetObj.IsValid()) continue;

			FHotUpdateManifestEntry Entry;
			Entry.FilePath = AssetObj->GetStringField(TEXT("path"));
			Entry.FileSize = (int64)AssetObj->GetNumberField(TEXT("size"));
			Entry.FileHash = AssetObj->GetStringField(TEXT("hash"));

			OutEntries.Add(Entry.FilePath, Entry);
		}

		UE_LOG(LogHotUpdateEditor, Log, TEXT("ParseManifestFile: Parsed %d entries from 'assets' array (legacy format) in '%s'"),
			OutEntries.Num(), *ManifestPath);
		return true;
	}

	UE_LOG(LogHotUpdateEditor, Warning, TEXT("ParseManifestFile: No 'files' or 'assets' array found in '%s'"), *ManifestPath);
	return false;
}

TArray<FString> UHotUpdateDiffTool::GetPakContentList(const FString& PakPath)
{
	TArray<FString> ContentList;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*PakPath))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("Pak file not found: %s"), *PakPath);
		return ContentList;
	}

	TRefCountPtr<FPakFile> PakFile = new FPakFile(&PlatformFile, *PakPath, false);
	if (!PakFile.IsValid() || !PakFile->IsValid())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("Failed to open Pak file: %s"), *PakPath);
		return ContentList;
	}

	for (FPakFile::FFilenameIterator It(*PakFile); It; ++It)
	{
		ContentList.Add(It.Filename());
	}

	// FFilenameIterator 无结果时，使用 FPakEntryIterator 作为降级方案
	if (ContentList.Num() == 0 && PakFile->GetNumFiles() > 0)
	{
		for (FPakFile::FPakEntryIterator It(*PakFile); It; ++It)
		{
			const FString* Filename = It.TryGetFilename();
			if (Filename && !Filename->IsEmpty())
			{
				ContentList.Add(*Filename);
			}
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("Found %d files in Pak: %s"), ContentList.Num(), *PakPath);

	return ContentList;
}

TMap<FString, FString> UHotUpdateDiffTool::GetPakFileHashes(const FString& PakPath)
{
	TMap<FString, FString> FileHashes;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*PakPath))
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("Pak file not found: %s"), *PakPath);
		return FileHashes;
	}

	TRefCountPtr<FPakFile> PakFile = new FPakFile(&PlatformFile, *PakPath, false);
	if (!PakFile.IsValid() || !PakFile->IsValid())
	{
		UE_LOG(LogHotUpdateEditor, Error, TEXT("Failed to open Pak file: %s"), *PakPath);
		return FileHashes;
	}

	// 使用 FFilenameIterator 遍历 Pak 条目
	for (FPakFile::FFilenameIterator It(*PakFile); It; ++It)
	{
		const FPakEntry& Entry = It.Info();
		FString Hash = UHotUpdateFileUtils::BytesToHex(Entry.Hash, sizeof(Entry.Hash));
		FileHashes.Add(It.Filename(), Hash);
	}

	// 降级方案：使用 FPakEntryIterator
	if (FileHashes.Num() == 0 && PakFile->GetNumFiles() > 0)
	{
		for (FPakFile::FPakEntryIterator It(*PakFile); It; ++It)
		{
			const FString* Filename = It.TryGetFilename();
			if (Filename && !Filename->IsEmpty())
			{
				const FPakEntry& Entry = It.Info();
				FString Hash = UHotUpdateFileUtils::BytesToHex(Entry.Hash, sizeof(Entry.Hash));
				FileHashes.Add(*Filename, Hash);
			}
		}
	}

	UE_LOG(LogHotUpdateEditor, Log, TEXT("Extracted %d file hashes from Pak: %s"), FileHashes.Num(), *PakPath);

	return FileHashes;
}

FString UHotUpdateDiffTool::GetAssetTypeFromExtension(const FString& Extension)
{
	FString Ext = Extension.ToLower();

	static TMap<FString, FString> ExtensionToType;
	if (ExtensionToType.Num() == 0)
	{
		ExtensionToType.Add(TEXT("uasset"), TEXT("Asset"));
		ExtensionToType.Add(TEXT("umap"), TEXT("Map"));
		ExtensionToType.Add(TEXT("png"), TEXT("Texture"));
		ExtensionToType.Add(TEXT("tga"), TEXT("Texture"));
		ExtensionToType.Add(TEXT("jpg"), TEXT("Texture"));
		ExtensionToType.Add(TEXT("wav"), TEXT("Sound"));
		ExtensionToType.Add(TEXT("fbx"), TEXT("Mesh"));
		ExtensionToType.Add(TEXT("obj"), TEXT("Mesh"));
		ExtensionToType.Add(TEXT("pak"), TEXT("Pak"));
		ExtensionToType.Add(TEXT("utoc"), TEXT("IoStore"));
		ExtensionToType.Add(TEXT("ucas"), TEXT("IoStore"));
	}

	return ExtensionToType.Contains(Ext) ? ExtensionToType[Ext] : TEXT("Unknown");
}

FString UHotUpdateDiffTool::FormatFileSize(int64 Size)
{
	if (Size < 1024)
	{
		return FString::Printf(TEXT("%lld B"), Size);
	}
	else if (Size < 1024 * 1024)
	{
		return FString::Printf(TEXT("%.2f KB"), Size / 1024.0);
	}
	else if (Size < 1024 * 1024 * 1024)
	{
		return FString::Printf(TEXT("%.2f MB"), Size / (1024.0 * 1024.0));
	}
	else
	{
		return FString::Printf(TEXT("%.2f GB"), Size / (1024.0 * 1024.0 * 1024.0));
	}
}

FString UHotUpdateDiffTool::FindFileManifestPath(const FString& VersionDirectory)
{
	// 规范化目录路径
	FString NormalizedDir = VersionDirectory;
	FPaths::NormalizeDirectoryName(NormalizedDir);
	NormalizedDir.ReplaceInline(TEXT("\\"), TEXT("/"));

	// 查找顺序
	TArray<FString> SearchPaths = {
		NormalizedDir / TEXT("Windows") / TEXT("filemanifest.json"),
		NormalizedDir / TEXT("filemanifest.json"),
		NormalizedDir / TEXT("Windows") / TEXT("manifest.json"),
		NormalizedDir / TEXT("manifest.json")
	};

	for (const FString& SearchPath : SearchPaths)
	{
		if (FPaths::FileExists(SearchPath))
		{
			UE_LOG(LogHotUpdateEditor, Log, TEXT("FindFileManifestPath: Found manifest at '%s'"), *SearchPath);
			return SearchPath;
		}
	}

	UE_LOG(LogHotUpdateEditor, Warning, TEXT("FindFileManifestPath: No manifest found in '%s'"), *NormalizedDir);
	return TEXT("");
}