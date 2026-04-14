// Copyright czm. All Rights Reserved.

#include "Core/HotUpdateVersionStorage.h"
#include "Core/HotUpdateFileUtils.h"
#include "HotUpdate.h"
#include "Manifest/HotUpdateManifestParser.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UHotUpdateVersionStorage::UHotUpdateVersionStorage()
{
}

void UHotUpdateVersionStorage::Initialize(const FString& InStoragePath)
{
	StoragePath = InStoragePath;

	// 确保存储目录存在
	UHotUpdateFileUtils::EnsureDirectoryExists(StoragePath);
}

bool UHotUpdateVersionStorage::LoadLocalVersion(FHotUpdateVersionInfo& OutVersion)
{
	FString VersionFilePath = GetVersionFilePath();

	if (!UHotUpdateFileUtils::FileExists(VersionFilePath))
	{
		UE_LOG(LogHotUpdate, Log, TEXT("Version file not found: %s"), *VersionFilePath);
		OutVersion = FHotUpdateVersionInfo();
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *VersionFilePath))
	{
		UE_LOG(LogHotUpdate, Warning, TEXT("Failed to read version file: %s"), *VersionFilePath);
		return false;
	}

	// 解析 JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogHotUpdate, Warning, TEXT("Failed to parse version file JSON"));
		return false;
	}

	// 解析版本信息
	JsonObject->TryGetNumberField(TEXT("majorVersion"), OutVersion.MajorVersion);
	JsonObject->TryGetNumberField(TEXT("minorVersion"), OutVersion.MinorVersion);
	JsonObject->TryGetNumberField(TEXT("patchVersion"), OutVersion.PatchVersion);
	JsonObject->TryGetNumberField(TEXT("buildNumber"), OutVersion.BuildNumber);
	JsonObject->TryGetStringField(TEXT("versionString"), OutVersion.VersionString);
	JsonObject->TryGetStringField(TEXT("platform"), OutVersion.Platform);
	JsonObject->TryGetNumberField(TEXT("timestamp"), OutVersion.Timestamp);

	UE_LOG(LogHotUpdate, Log, TEXT("Loaded local version: %s"), *OutVersion.ToString());
	return true;
}

bool UHotUpdateVersionStorage::SaveLocalVersion(const FHotUpdateVersionInfo& Version)
{
	FString VersionFilePath = GetVersionFilePath();

	// 确保目录存在
	UHotUpdateFileUtils::EnsureDirectoryExists(FPaths::GetPath(VersionFilePath));

	// 构建 JSON
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetNumberField(TEXT("majorVersion"), Version.MajorVersion);
	JsonObject->SetNumberField(TEXT("minorVersion"), Version.MinorVersion);
	JsonObject->SetNumberField(TEXT("patchVersion"), Version.PatchVersion);
	JsonObject->SetNumberField(TEXT("buildNumber"), Version.BuildNumber);
	JsonObject->SetStringField(TEXT("versionString"), Version.VersionString);
	JsonObject->SetStringField(TEXT("platform"), Version.Platform);
	JsonObject->SetNumberField(TEXT("timestamp"), Version.Timestamp);

	// 序列化
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	// 保存文件
	if (FFileHelper::SaveStringToFile(JsonString, *VersionFilePath))
	{
		UE_LOG(LogHotUpdate, Log, TEXT("Saved local version: %s"), *Version.ToString());
		return true;
	}
	else
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to save version file: %s"), *VersionFilePath);
		return false;
	}
}

TArray<FHotUpdateVersionInfo> UHotUpdateVersionStorage::GetLocalVersionHistory() const
{
	TArray<FHotUpdateVersionInfo> History;
	TSet<FHotUpdateVersionInfo> AddedVersions;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*StoragePath))
	{
		return History;
	}

	// 遍历版本目录
	PlatformFile.IterateDirectory(*StoragePath, [&History, &AddedVersions, this, &PlatformFile](const TCHAR* Path, bool bIsDirectory)
	{
		if (bIsDirectory)
		{
			FString VersionStr = FPaths::GetCleanFilename(Path);

			// 尝试从目录名解析版本
			FHotUpdateVersionInfo Version = FHotUpdateVersionInfo::FromString(VersionStr);

			// 检查版本文件是否存在
			FString VersionFile = FString(Path) / TEXT("version.json");
			if (PlatformFile.FileExists(*VersionFile))
			{
				FString JsonString;
				if (FFileHelper::LoadFileToString(JsonString, *VersionFile))
				{
					TSharedPtr<FJsonObject> JsonObject;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
					if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
					{
						JsonObject->TryGetNumberField(TEXT("majorVersion"), Version.MajorVersion);
						JsonObject->TryGetNumberField(TEXT("minorVersion"), Version.MinorVersion);
						JsonObject->TryGetNumberField(TEXT("patchVersion"), Version.PatchVersion);
						JsonObject->TryGetNumberField(TEXT("buildNumber"), Version.BuildNumber);
						JsonObject->TryGetStringField(TEXT("versionString"), Version.VersionString);
					}
				}
			}

			if (!AddedVersions.Contains(Version) && Version.MajorVersion > 0)
			{
				History.Add(Version);
				AddedVersions.Add(Version);
			}
		}
		return true;
	});

	// 按版本号排序（最新的在前）
	History.Sort([](const FHotUpdateVersionInfo& A, const FHotUpdateVersionInfo& B)
	{
		return A > B;
	});

	return History;
}

bool UHotUpdateVersionStorage::LoadLocalManifest(FHotUpdateManifest& OutManifest)
{
	FString ManifestPath = GetManifestFilePath();

	if (!UHotUpdateFileUtils::FileExists(ManifestPath))
	{
		UE_LOG(LogHotUpdate, Verbose, TEXT("Local manifest not found: %s"), *ManifestPath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		UE_LOG(LogHotUpdate, Warning, TEXT("Failed to read local manifest: %s"), *ManifestPath);
		return false;
	}

	if (!UHotUpdateManifestParser::ParseFromJson(JsonString, OutManifest))
	{
		UE_LOG(LogHotUpdate, Warning, TEXT("Failed to parse local manifest"));
		return false;
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Loaded local manifest: version %s, %d files"),
		*OutManifest.VersionInfo.ToString(), OutManifest.Files.Num());
	return true;
}

bool UHotUpdateVersionStorage::SaveLocalManifest(const FHotUpdateManifest& Manifest)
{
	FString ManifestPath = GetManifestFilePath();

	if (UHotUpdateManifestParser::SaveToFile(ManifestPath, Manifest))
	{
		UE_LOG(LogHotUpdate, Log, TEXT("Saved local manifest: %s"), *ManifestPath);
		return true;
	}
	else
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to save manifest: %s"), *ManifestPath);
		return false;
	}
}