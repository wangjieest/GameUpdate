// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateEditorTypes.h"

/**
 * 热更新工具函数集合
 */
namespace HotUpdateUtils
{
	/**
	 * 将平台枚举转换为显示名称字符串
	 * @param Platform 平台枚举值
	 * @return 平台显示名称 ("Windows", "Android", "IOS")
	 */
	HOTUPDATEEDITOR_API FString GetPlatformString(EHotUpdatePlatform Platform);

	/**
	 * 将平台枚举转换为目录名称字符串
	 * @param Platform 平台枚举值
	 * @return 平台目录名称 ("Win64", "Android", "IOS")
	 */
	HOTUPDATEEDITOR_API FString GetPlatformDirectoryName(EHotUpdatePlatform Platform);

	/**
	 * 获取平台目录名称（考虑 Android 纹理格式后缀）
	 * @param Platform 平台枚举值
	 * @param AndroidTextureFormat Android 纹理格式（仅 Android 平台有效）
	 * @return 平台目录名称 ("Windows", "Android_ASTC", "Android_ETC2" 等)
	 */
	HOTUPDATEEDITOR_API FString GetPlatformDirName(EHotUpdatePlatform Platform, EHotUpdateAndroidTextureFormat AndroidTextureFormat = EHotUpdateAndroidTextureFormat::Multi);

	/**
	 * 获取 Cooked 平台目录路径（Saved/Cooked/{PlatformName}）
	 * @param Platform 平台枚举值
	 * @return Cooked 平台目录的完整路径
	 */
	HOTUPDATEEDITOR_API FString GetCookedPlatformDir(EHotUpdatePlatform Platform);

	/**
	 * 获取 Cooked 平台目录路径（考虑 Android 纹理格式后缀，如 Android_ASTC）
	 * @param Platform 平台枚举值
	 * @param AndroidTextureFormat Android 纹理格式（仅 Android 平台有效）
	 * @return Cooked 平台目录的完整路径
	 */
	HOTUPDATEEDITOR_API FString GetCookedPlatformDir(EHotUpdatePlatform Platform, EHotUpdateAndroidTextureFormat AndroidTextureFormat);
}