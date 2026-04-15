// Copyright czm. All Rights Reserved.

#include "Manifest/HotUpdateManifest.h"

void FHotUpdateManifest::BuildPathIndex()
{
	PathIndex.Empty(Files.Num());
	ChunkIndex.Empty(Chunks.Num());

	// 构建文件路径索引
	for (int32 i = 0; i < Files.Num(); i++)
	{
		PathIndex.Add(Files[i].FilePath, i);
	}

	// 构建 Chunk 索引
	for (int32 i = 0; i < Chunks.Num(); i++)
	{
		ChunkIndex.Add(Chunks[i].ChunkId, i);
	}

	bPathIndexBuilt = true;
}

const FHotUpdateManifestEntry* FHotUpdateManifest::FindFile(const FString& RelativePath) const
{
	// 如果索引未构建，构建它
	if (!bPathIndexBuilt)
	{
		const_cast<FHotUpdateManifest*>(this)->BuildPathIndex();
	}

	// 使用索引查找
	const int32* IndexPtr = PathIndex.Find(RelativePath);
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