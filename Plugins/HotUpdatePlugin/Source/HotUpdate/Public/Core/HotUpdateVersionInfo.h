// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HotUpdateVersionInfo.generated.h"

/**
 * 版本信息
 */
USTRUCT(BlueprintType)
struct HOTUPDATE_API FHotUpdateVersionInfo
{
	GENERATED_BODY()

	/// 主版本号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 MajorVersion;

	/// 次版本号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 MinorVersion;

	/// 补丁版本号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 PatchVersion;

	/// 构建号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int32 BuildNumber;

	/// 版本字符串 (例如 "1.2.3.456")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString VersionString;

	/// 平台标识
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString Platform;

	/// 发布时间戳
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	int64 Timestamp;

	/// 更新说明
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HotUpdate")
	FString ReleaseNotes;

	/// 比较版本号
	bool operator>(const FHotUpdateVersionInfo& Other) const
	{
		if (MajorVersion != Other.MajorVersion) return MajorVersion > Other.MajorVersion;
		if (MinorVersion != Other.MinorVersion) return MinorVersion > Other.MinorVersion;
		if (PatchVersion != Other.PatchVersion) return PatchVersion > Other.PatchVersion;
		return BuildNumber > Other.BuildNumber;
	}

	bool operator<(const FHotUpdateVersionInfo& Other) const
	{
		if (MajorVersion != Other.MajorVersion) return MajorVersion < Other.MajorVersion;
		if (MinorVersion != Other.MinorVersion) return MinorVersion < Other.MinorVersion;
		if (PatchVersion != Other.PatchVersion) return PatchVersion < Other.PatchVersion;
		return BuildNumber < Other.BuildNumber;
	}

	bool operator==(const FHotUpdateVersionInfo& Other) const
	{
		return MajorVersion == Other.MajorVersion
			&& MinorVersion == Other.MinorVersion
			&& PatchVersion == Other.PatchVersion
			&& BuildNumber == Other.BuildNumber;
	}

	/// 获取 Hash 值（用于 TSet/TMap 键）
	friend uint32 GetTypeHash(const FHotUpdateVersionInfo& Version)
	{
		return HashCombine(GetTypeHash(Version.MajorVersion),
			HashCombine(GetTypeHash(Version.MinorVersion),
			HashCombine(GetTypeHash(Version.PatchVersion),
			GetTypeHash(Version.BuildNumber))));
	}

	/// 从字符串解析版本
	static FHotUpdateVersionInfo FromString(const FString& InVersionString)
	{
		FHotUpdateVersionInfo Result;
		Result.VersionString = InVersionString;

		TArray<FString> Parts;
		InVersionString.ParseIntoArray(Parts, TEXT("."));

		// 验证各部分为有效数字，非数字输入返回 0
		auto ParseVersionPart = [](const FString& Str) -> int32
		{
			if (Str.IsEmpty()) return 0;
			for (int32 i = 0; i < Str.Len(); i++)
			{
				if (!FChar::IsDigit(Str[i])) return 0;
			}
			return FCString::Atoi(*Str);
		};

		if (Parts.Num() >= 1) Result.MajorVersion = ParseVersionPart(Parts[0]);
		if (Parts.Num() >= 2) Result.MinorVersion = ParseVersionPart(Parts[1]);
		if (Parts.Num() >= 3) Result.PatchVersion = ParseVersionPart(Parts[2]);
		if (Parts.Num() >= 4) Result.BuildNumber = ParseVersionPart(Parts[3]);

		return Result;
	}

	/// 转换为字符串
	FString ToString() const
	{
		if (!VersionString.IsEmpty())
		{
			return VersionString;
		}
		return FString::Printf(TEXT("%d.%d.%d.%d"), MajorVersion, MinorVersion, PatchVersion, BuildNumber);
	}

	FHotUpdateVersionInfo()
		: MajorVersion(0)
		, MinorVersion(0)
		, PatchVersion(0)
		, BuildNumber(0)
		, Timestamp(0)
	{
	}
};