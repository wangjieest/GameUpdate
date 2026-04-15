// Copyright czm. All Rights Reserved.

#include "HotUpdateUtils.h"

FString HotUpdateUtils::GetPlatformString(EHotUpdatePlatform Platform)
{
	switch (Platform)
	{
	case EHotUpdatePlatform::Windows:
		return TEXT("Windows");
	case EHotUpdatePlatform::Android:
		return TEXT("Android");
	case EHotUpdatePlatform::IOS:
		return TEXT("IOS");
	default:
		return TEXT("Windows");
	}
}

bool HotUpdateUtils::IsExternalActorOrObjectPath(const FString& Path)
{
	// UE5 OFPA (One-File-Per-Actor) 系统将 Level 中的 Actor 存储为独立包
	// 路径如 /Game/__ExternalActors__/... 和 /Game/__ExternalObjects__/...
	// 这些是 Level 的子对象，烘焙时已合入 .umap，不应作为独立资源打包
	return Path.Contains(TEXT("/__ExternalActors__/")) ||
		Path.Contains(TEXT("/__ExternalObjects__/"));
}

FString HotUpdateUtils::GetPlatformDirectoryName(EHotUpdatePlatform Platform)
{
	switch (Platform)
	{
	case EHotUpdatePlatform::Windows:
		return TEXT("Win64");
	case EHotUpdatePlatform::Android:
		return TEXT("Android");
	case EHotUpdatePlatform::IOS:
		return TEXT("IOS");
	default:
		return TEXT("Win64");
	}
}