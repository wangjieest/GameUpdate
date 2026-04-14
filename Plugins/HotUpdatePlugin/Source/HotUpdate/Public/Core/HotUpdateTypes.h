// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateTypes.generated.h"

/**
 * 热更新状态
 */
UENUM(BlueprintType)
enum class EHotUpdateState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	CheckingVersion UMETA(DisplayName = "Checking Version"),
	Downloading     UMETA(DisplayName = "Downloading"),
	Paused          UMETA(DisplayName = "Paused"),
	Installing      UMETA(DisplayName = "Installing"),
	Success         UMETA(DisplayName = "Success"),
	Failed          UMETA(DisplayName = "Failed"),
	Rollback        UMETA(DisplayName = "Rollback")
};

/**
 * 文件变更类型
 */
UENUM(BlueprintType)
enum class EHotUpdateFileChangeType : uint8
{
	Added           UMETA(DisplayName = "Added"),
	Modified        UMETA(DisplayName = "Modified"),
	Deleted         UMETA(DisplayName = "Deleted"),
	Unchanged       UMETA(DisplayName = "Unchanged")
};

/**
 * 包类型（基础包 vs 更新包）
 */
UENUM(BlueprintType)
enum class EHotUpdatePackageKind : uint8
{
	Base            UMETA(DisplayName = "基础包"),
	Patch           UMETA(DisplayName = "更新包")
};