// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateFileUtils.generated.h"

/**
 * 热更新文件工具类
 * 提供文件操作的公共静态方法
 */
UCLASS(BlueprintType)
class HOTUPDATE_API UHotUpdateFileUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 计算文件 SHA1 Hash
	 * @param FilePath 文件路径
	 * @return SHA1 Hash 十六进制字符串，失败返回空字符串
	 */
	UFUNCTION(BlueprintCallable, Category = "HotUpdate|File")
	static FString CalculateFileHash(const FString& FilePath);

	/**
	 * 确保目录存在
	 * @param DirectoryPath 目录路径
	 * @return 是否成功创建或目录已存在
	 */
	UFUNCTION(BlueprintCallable, Category = "HotUpdate|File")
	static bool EnsureDirectoryExists(const FString& DirectoryPath);

	/**
	 * 字节数组转十六进制字符串
	 * @param Bytes 字节数组指针
	 * @param Count 字节数量
	 * @return 十六进制字符串
	 */
	static FString BytesToHex(const uint8* Bytes, int32 Count);

	/**
	 * 十六进制字符串转字节数组
	 * @param HexString 十六进制字符串
	 * @param OutBytes 输出字节数组
	 * @return 是否转换成功
	 */
	static bool HexToBytes(const FString& HexString, TArray<uint8>& OutBytes);

	/**
	 * 获取文件大小
	 * @param FilePath 文件路径
	 * @return 文件大小（字节），失败返回 -1
	 */
	static int64 GetFileSize(const FString& FilePath);

	/**
	 * 检查文件是否存在
	 * @param FilePath 文件路径
	 * @return 文件是否存在
	 */
	static bool FileExists(const FString& FilePath);

	/**
	 * 检查目录是否存在
	 * @param DirectoryPath 目录路径
	 * @return 目录是否存在
	 */
	static bool DirectoryExists(const FString& DirectoryPath);

	/**
	 * 创建目录（包括父目录）
	 * @param DirectoryPath 目录路径
	 * @return 是否成功
	 */
	static bool CreateDirectoryTree(const FString& DirectoryPath);

	/**
	 * 删除目录及其内容
	 * @param DirectoryPath 目录路径
	 * @return 是否成功
	 */
	static bool DeleteDirectoryRecursively(const FString& DirectoryPath);
};