// Copyright czm. All Rights Reserved.

#include "Manifest/HotUpdateManifestParser.h"
#include "HotUpdate.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UHotUpdateManifestParser::LoadFromFile(const FString& FilePath, FHotUpdateManifest& OutManifest)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to load manifest file: %s"), *FilePath);
		return false;
	}

	return ParseFromJson(JsonString, OutManifest);
}

bool UHotUpdateManifestParser::ParseFromJson(const FString& JsonString, FHotUpdateManifest& OutManifest)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to parse manifest JSON"));
		return false;
	}

	// 解析版本信息
	const TSharedPtr<FJsonObject>* VersionObject;
	if (JsonObject->TryGetObjectField(TEXT("version"), VersionObject))
	{
		(*VersionObject)->TryGetStringField(TEXT("version"), OutManifest.VersionInfo.VersionString);
		(*VersionObject)->TryGetStringField(TEXT("platform"), OutManifest.VersionInfo.Platform);
		(*VersionObject)->TryGetNumberField(TEXT("timestamp"), OutManifest.VersionInfo.Timestamp);
	}

	// 解析版本信息（新格式 - versionInfo）
	const TSharedPtr<FJsonObject>* VersionInfoObject;
	if (JsonObject->TryGetObjectField(TEXT("versionInfo"), VersionInfoObject))
	{
		(*VersionInfoObject)->TryGetNumberField(TEXT("majorVersion"), OutManifest.VersionInfo.MajorVersion);
		(*VersionInfoObject)->TryGetNumberField(TEXT("minorVersion"), OutManifest.VersionInfo.MinorVersion);
		(*VersionInfoObject)->TryGetNumberField(TEXT("patchVersion"), OutManifest.VersionInfo.PatchVersion);
		(*VersionInfoObject)->TryGetNumberField(TEXT("buildNumber"), OutManifest.VersionInfo.BuildNumber);
		(*VersionInfoObject)->TryGetStringField(TEXT("versionString"), OutManifest.VersionInfo.VersionString);
		(*VersionInfoObject)->TryGetStringField(TEXT("platform"), OutManifest.VersionInfo.Platform);
		(*VersionInfoObject)->TryGetNumberField(TEXT("timestamp"), OutManifest.VersionInfo.Timestamp);
	}

	// 解析全量热更新标志
	JsonObject->TryGetBoolField(TEXT("bIncludesBaseContainers"), OutManifest.bIncludesBaseContainers);
	JsonObject->TryGetBoolField(TEXT("bRequiresBasePackage"), OutManifest.bRequiresBasePackage);
	JsonObject->TryGetNumberField(TEXT("totalDownloadSize"), OutManifest.TotalDownloadSize);

	// 解析基础版本号
	JsonObject->TryGetStringField(TEXT("baseVersion"), OutManifest.BaseVersion);

	// 解析 chunks 数组（旧格式）
	const TArray<TSharedPtr<FJsonValue>>* ChunksArray;
	if (JsonObject->TryGetArrayField(TEXT("chunks"), ChunksArray))
	{
		OutManifest.Containers.Empty();
		for (const TSharedPtr<FJsonValue>& ChunkValue : *ChunksArray)
		{
			TSharedPtr<FJsonObject> ChunkObject = ChunkValue->AsObject();
			if (!ChunkObject.IsValid()) continue;

			FHotUpdateContainerInfo Container;
			ChunkObject->TryGetStringField(TEXT("ChunkName"), Container.ContainerName);
			ChunkObject->TryGetStringField(TEXT("utocPath"), Container.UtocPath);
			ChunkObject->TryGetNumberField(TEXT("utocSize"), Container.UtocSize);
			ChunkObject->TryGetStringField(TEXT("utocHash"), Container.UtocHash);
			ChunkObject->TryGetStringField(TEXT("ucasPath"), Container.UcasPath);
			ChunkObject->TryGetNumberField(TEXT("ucasSize"), Container.UcasSize);
			ChunkObject->TryGetStringField(TEXT("ucasHash"), Container.UcasHash);

			// 解析容器类型
			FString ContainerTypeStr;
			if (ChunkObject->TryGetStringField(TEXT("containerType"), ContainerTypeStr))
			{
				Container.ContainerType = ContainerTypeStr.StartsWith(TEXT("base"))
					? EHotUpdateContainerType::Base
					: EHotUpdateContainerType::Patch;
			}

			// 解析 ChunkId
			double ChunkIdValue;
			if (ChunkObject->TryGetNumberField(TEXT("chunkId"), ChunkIdValue))
			{
				Container.ChunkId = static_cast<int32>(ChunkIdValue);
			}
			ChunkObject->TryGetStringField(TEXT("version"), Container.Version);

			OutManifest.Containers.Add(Container);
		}
	}

	// 解析 containers 数组（新格式）
	const TArray<TSharedPtr<FJsonValue>>* ContainersArray;
	if (JsonObject->TryGetArrayField(TEXT("containers"), ContainersArray))
	{
		OutManifest.Containers.Empty();
		for (const TSharedPtr<FJsonValue>& ContainerValue : *ContainersArray)
		{
			TSharedPtr<FJsonObject> ContainerObject = ContainerValue->AsObject();
			if (!ContainerObject.IsValid()) continue;

			FHotUpdateContainerInfo Container;
			ContainerObject->TryGetStringField(TEXT("containerName"), Container.ContainerName);
			ContainerObject->TryGetStringField(TEXT("utocPath"), Container.UtocPath);
			ContainerObject->TryGetNumberField(TEXT("utocSize"), Container.UtocSize);
			ContainerObject->TryGetStringField(TEXT("utocHash"), Container.UtocHash);
			ContainerObject->TryGetStringField(TEXT("ucasPath"), Container.UcasPath);
			ContainerObject->TryGetNumberField(TEXT("ucasSize"), Container.UcasSize);
			ContainerObject->TryGetStringField(TEXT("ucasHash"), Container.UcasHash);

			// 解析容器类型
			FString ContainerTypeStr;
			if (ContainerObject->TryGetStringField(TEXT("containerType"), ContainerTypeStr))
			{
				if (ContainerTypeStr.StartsWith(TEXT("base")))
				{
					Container.ContainerType = EHotUpdateContainerType::Base;
				}
				else if (ContainerTypeStr.StartsWith(TEXT("patch")))
				{
					Container.ContainerType = EHotUpdateContainerType::Patch;
				}
			}

			// 解析 ChunkId
			ContainerObject->TryGetNumberField(TEXT("chunkId"), Container.ChunkId);

			ContainerObject->TryGetStringField(TEXT("version"), Container.Version);
			OutManifest.Containers.Add(Container);
		}
	}

	OutManifest.BuildPathIndex();

	UE_LOG(LogHotUpdate, Log, TEXT("Parsed manifest: version %s, %d chunks"),
		*OutManifest.VersionInfo.VersionString,
		OutManifest.Containers.Num());

	return true;
}

bool UHotUpdateManifestParser::SaveToFile(const FString& FilePath, const FHotUpdateManifest& Manifest)
{
	FString JsonString = ToJsonString(Manifest);
	if (JsonString.IsEmpty()) return false;

	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	return FFileHelper::SaveStringToFile(JsonString, *FilePath);
}

FString UHotUpdateManifestParser::ToJsonString(const FHotUpdateManifest& Manifest)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	// 版本信息（新格式 versionInfo）
	TSharedPtr<FJsonObject> VersionObject = MakeShareable(new FJsonObject());
	VersionObject->SetStringField(TEXT("version"), Manifest.VersionInfo.VersionString);
	VersionObject->SetStringField(TEXT("platform"), Manifest.VersionInfo.Platform);
	VersionObject->SetNumberField(TEXT("timestamp"), Manifest.VersionInfo.Timestamp);
	JsonObject->SetObjectField(TEXT("versionInfo"), VersionObject);

	// 兼容旧格式
	TSharedPtr<FJsonObject> LegacyVersionObject = MakeShareable(new FJsonObject());
	LegacyVersionObject->SetStringField(TEXT("version"), Manifest.VersionInfo.VersionString);
	LegacyVersionObject->SetStringField(TEXT("platform"), Manifest.VersionInfo.Platform);
	LegacyVersionObject->SetNumberField(TEXT("timestamp"), Manifest.VersionInfo.Timestamp);
	JsonObject->SetObjectField(TEXT("version"), LegacyVersionObject);

	// 全量热更新标志
	JsonObject->SetBoolField(TEXT("bIncludesBaseContainers"), Manifest.bIncludesBaseContainers);
	JsonObject->SetBoolField(TEXT("bRequiresBasePackage"), Manifest.bRequiresBasePackage);
	JsonObject->SetNumberField(TEXT("totalDownloadSize"), Manifest.TotalDownloadSize);

	// 基础版本号
	if (!Manifest.BaseVersion.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("baseVersion"), Manifest.BaseVersion);
	}

	// containers 数组（新格式，包含 containerType 和 chunkId）
	TArray<TSharedPtr<FJsonValue>> ContainersArray;
	for (const FHotUpdateContainerInfo& Container : Manifest.Containers)
	{
		TSharedPtr<FJsonObject> ContainerObject = MakeShareable(new FJsonObject());
		ContainerObject->SetStringField(TEXT("containerName"), Container.ContainerName);
		ContainerObject->SetStringField(TEXT("utocPath"), Container.UtocPath);
		ContainerObject->SetNumberField(TEXT("utocSize"), Container.UtocSize);
		ContainerObject->SetStringField(TEXT("utocHash"), Container.UtocHash);
		ContainerObject->SetStringField(TEXT("ucasPath"), Container.UcasPath);
		ContainerObject->SetNumberField(TEXT("ucasSize"), Container.UcasSize);
		ContainerObject->SetStringField(TEXT("ucasHash"), Container.UcasHash);

		// 容器类型
		ContainerObject->SetStringField(TEXT("containerType"),
			Container.ContainerType == EHotUpdateContainerType::Base ? TEXT("base") : TEXT("patch"));

		// Chunk ID
		ContainerObject->SetNumberField(TEXT("chunkId"), Container.ChunkId);
			ContainerObject->SetStringField(TEXT("version"), Container.Version);

		ContainersArray.Add(MakeShareable(new FJsonValueObject(ContainerObject)));
	}
	JsonObject->SetArrayField(TEXT("containers"), ContainersArray);

	// 兼容旧格式 chunks 数组
	TArray<TSharedPtr<FJsonValue>> ChunksArray;
	for (const FHotUpdateContainerInfo& Container : Manifest.Containers)
	{
		TSharedPtr<FJsonObject> ChunkObject = MakeShareable(new FJsonObject());
		ChunkObject->SetStringField(TEXT("containerType"),
			Container.ContainerType == EHotUpdateContainerType::Base ? TEXT("base") : TEXT("patch"));
		ChunkObject->SetNumberField(TEXT("chunkId"), Container.ChunkId);
		ChunkObject->SetStringField(TEXT("ChunkName"), Container.ContainerName);
		ChunkObject->SetStringField(TEXT("utocPath"), Container.UtocPath);
		ChunkObject->SetNumberField(TEXT("utocSize"), Container.UtocSize);
		ChunkObject->SetStringField(TEXT("utocHash"), Container.UtocHash);
		ChunkObject->SetStringField(TEXT("ucasPath"), Container.UcasPath);
		ChunkObject->SetNumberField(TEXT("ucasSize"), Container.UcasSize);
		ChunkObject->SetStringField(TEXT("ucasHash"), Container.UcasHash);
			ChunkObject->SetStringField(TEXT("version"), Container.Version);
		ChunksArray.Add(MakeShareable(new FJsonValueObject(ChunkObject)));
	}
	JsonObject->SetArrayField(TEXT("chunks"), ChunksArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}