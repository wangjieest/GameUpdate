// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateChunkTypes.generated.h"

/**
 * Chunk 定义（运行时）
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateChunkInfo
{
	GENERATED_BODY()

	/// Chunk ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 ChunkId;

	/// Chunk 名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	FString ChunkName;

	/// 加载优先级
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int32 Priority;

	FHotUpdateChunkInfo()
		: ChunkId(-1)
		, Priority(0)
	{
	}
};