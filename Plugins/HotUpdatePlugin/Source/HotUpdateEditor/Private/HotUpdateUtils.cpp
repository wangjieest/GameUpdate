// Copyright czm. All Rights Reserved.

#include "HotUpdateUtils.h"
#include "Misc/Paths.h"

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

FString HotUpdateUtils::GetPlatformDirName(EHotUpdatePlatform Platform, EHotUpdateAndroidTextureFormat AndroidTextureFormat)
{
	FString PlatformDir = GetPlatformString(Platform);

	// Android 平台带纹理格式时，目录名为 Android_ASTC / Android_ETC2 / Android_DXT
	if (Platform == EHotUpdatePlatform::Android && AndroidTextureFormat != EHotUpdateAndroidTextureFormat::Multi)
	{
		FString TextureFormat;
		switch (AndroidTextureFormat)
		{
		case EHotUpdateAndroidTextureFormat::ETC2: TextureFormat = TEXT("ETC2"); break;
		case EHotUpdateAndroidTextureFormat::ASTC: TextureFormat = TEXT("ASTC"); break;
		case EHotUpdateAndroidTextureFormat::DXT:  TextureFormat = TEXT("DXT");  break;
		default: break;
		}
		if (!TextureFormat.IsEmpty())
		{
			PlatformDir = FString::Printf(TEXT("Android_%s"), *TextureFormat);
		}
	}

	return PlatformDir;
}

FString HotUpdateUtils::GetCookedPlatformDir(EHotUpdatePlatform Platform)
{
	return FPaths::ProjectSavedDir() / TEXT("Cooked") / GetPlatformString(Platform);
}

FString HotUpdateUtils::GetCookedPlatformDir(EHotUpdatePlatform Platform, EHotUpdateAndroidTextureFormat AndroidTextureFormat)
{
	return FPaths::ProjectSavedDir() / TEXT("Cooked") / GetPlatformDirName(Platform, AndroidTextureFormat);
}