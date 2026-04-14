// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateChunkTypes.generated.h"

/**
 * Int32 数组包装结构体（用于解决 TMap 不支持 TArray 作为值的问题）
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateInt32Array
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Array")
	TArray<int32> Values;

	FHotUpdateInt32Array() {}
};

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

	/// 父 Chunk ID 列表
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	TArray<int32> ParentChunks;

	/// 子 Chunk ID 列表
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	TArray<int32> ChildChunks;

	/// 未压缩大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int64 UncompressedSize;

	/// 压缩后大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk")
	int64 CompressedSize;

	FHotUpdateChunkInfo()
		: ChunkId(-1)
		, Priority(0)
		, UncompressedSize(0)
		, CompressedSize(0)
	{
	}
};

/**
 * 简化版差异信息（用于 Manifest）
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateDiffSummary
{
	GENERATED_BODY()

	/// 新增资源数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int32 AddedCount;

	/// 修改资源数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int32 ModifiedCount;

	/// 删除资源数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int32 DeletedCount;

	/// 新增资源总大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int64 AddedSize;

	/// 修改资源大小变化
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int64 ModifiedSizeDiff;

	/// 删除资源总大小
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diff")
	int64 DeletedSize;

	FHotUpdateDiffSummary()
		: AddedCount(0)
		, ModifiedCount(0)
		, DeletedCount(0)
		, AddedSize(0)
		, ModifiedSizeDiff(0)
		, DeletedSize(0)
	{
	}

	};