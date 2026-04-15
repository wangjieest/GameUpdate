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
	 * 检查路径是否为 UE5 OFPA (One-File-Per-Actor) 外部路径
	 * __ExternalActors__ 和 __ExternalObjects__ 是 Level 的子对象，
	 * 烘焙时已合入 .umap 文件，不应作为独立资源打包
	 * @param Path 资源包路径（如 "/Game/__ExternalActors__/..."）
	 * @return true 表示是外部 Actor/Object 路径，应被过滤
	 */
	HOTUPDATEEDITOR_API bool IsExternalActorOrObjectPath(const FString& Path);

	/**
	 * 将平台枚举转换为目录名称字符串
	 * @param Platform 平台枚举值
	 * @return 平台目录名称 ("Win64", "Android", "IOS")
	 */
	HOTUPDATEEDITOR_API FString GetPlatformDirectoryName(EHotUpdatePlatform Platform);
}