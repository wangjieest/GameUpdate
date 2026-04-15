// Copyright czm. All Rights Reserved.

#include "Manifest/HotUpdateManifest.h"

void FHotUpdateManifest::BuildPathIndex()
{
	PathIndex.Empty(Files.Num());
	ChunkIndex.Empty(Chunks.Num());
	ContainerIndex.Empty(Containers.Num());

	// 构建文件路径索引（编辑器端使用，运行时 Files 通常为空）
	for (int32 i = 0; i < Files.Num(); i++)
	{
		PathIndex.Add(Files[i].FilePath, i);
	}

	// 构建 Chunk 索引
	for (int32 i = 0; i < Chunks.Num(); i++)
	{
		ChunkIndex.Add(Chunks[i].ChunkId, i);
	}

	// 构建 Container 索引（运行时使用）
	for (int32 i = 0; i < Containers.Num(); i++)
	{
		ContainerIndex.Add(Containers[i].ChunkId, i);
	}

	bPathIndexBuilt = true;
}

const FHotUpdateManifestEntry* FHotUpdateManifest::FindFile(const FString& FilePath) const
{
	// 如果索引未构建，构建它
	if (!bPathIndexBuilt)
	{
		const_cast<FHotUpdateManifest*>(this)->BuildPathIndex();
	}

	// 使用索引查找（Files 通常只在编辑器端有数据）
	const int32* IndexPtr = PathIndex.Find(FilePath);
	if (IndexPtr)
	{
		return &Files[*IndexPtr];
	}

	return nullptr;
}

const FHotUpdateChunkInfo* FHotUpdateManifest::FindChunk(int32 ChunkId) const
{
	// 如果索引未构建，构建它
	if (!bPathIndexBuilt)
	{
		const_cast<FHotUpdateManifest*>(this)->BuildPathIndex();
	}

	// 使用索引查找
	const int32* IndexPtr = ChunkIndex.Find(ChunkId);
	if (IndexPtr)
	{
		return &Chunks[*IndexPtr];
	}

	return nullptr;
}

const FHotUpdateContainerInfo* FHotUpdateManifest::FindContainer(int32 ChunkId) const
{
	// 如果索引未构建，构建它
	if (!bPathIndexBuilt)
	{
		const_cast<FHotUpdateManifest*>(this)->BuildPathIndex();
	}

	// 使用索引查找（运行时热更新使用）
	const int32* IndexPtr = ContainerIndex.Find(ChunkId);
	if (IndexPtr)
	{
		return &Containers[*IndexPtr];
	}

	return nullptr;
}