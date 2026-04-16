// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HotUpdateEditorTypes.h"
#include "HotUpdateEditorSettings.generated.h"

/**
 * 热更新编辑器设置
 * 存储全局打包配置
 */
UCLASS(Config = Editor, DefaultConfig, meta = (
	DisplayName = "热更新编辑器设置",
	DisplayPriority = "100",
	CategoryName = "Plugins",
	SectionName = "HotUpdate"
))
class HOTUPDATEEDITOR_API UHotUpdateEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHotUpdateEditorSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("HotUpdate"); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	// == 默认打包配置 ==

	/// 默认输出格式
	UPROPERTY(Config, EditAnywhere, Category = "Packaging", meta = (DisplayName = "默认输出格式"))
	EHotUpdateOutputFormat DefaultOutputFormat;

	/// 默认输出目录
	UPROPERTY(Config, EditAnywhere, Category = "Packaging", meta = (DisplayName = "默认输出目录", ContentDir))
	FDirectoryPath DefaultOutputDirectory;

	/// 默认启用压缩
	UPROPERTY(Config, EditAnywhere, Category = "Packaging", meta = (DisplayName = "默认启用压缩"))
	bool bDefaultEnableCompression;

	/// 默认压缩级别
	UPROPERTY(Config, EditAnywhere, Category = "Packaging", meta = (
		DisplayName = "默认压缩级别",
		ClampMin = "0",
		ClampMax = "9",
		EditCondition = "bDefaultEnableCompression",
		EditConditionHides
	))
	int32 DefaultCompressionLevel;
	/// 默认加密密钥
	UPROPERTY(Config, EditAnywhere, Category = "Packaging|Advanced", meta = (DisplayName = "默认加密密钥"))
	FString DefaultEncryptionKey;

	/// 默认加密索引
	UPROPERTY(Config, EditAnywhere, Category = "Packaging|Advanced", meta = (DisplayName = "默认加密索引"))
	bool bDefaultEncryptIndex;

	/// 默认加密内容
	UPROPERTY(Config, EditAnywhere, Category = "Packaging|Advanced", meta = (DisplayName = "默认加密内容"))
	bool bDefaultEncryptContent;

	/// 下一个自动分配的Chunk ID
	UPROPERTY(Config, VisibleAnywhere, Category = "Packaging|Advanced", meta = (DisplayName = "下一个Chunk ID"))
	int32 NextAutoChunkId;

	// == 版本比较设置 ==

	/// 基础版本目录
	UPROPERTY(Config, EditAnywhere, Category = "Version Diff", meta = (DisplayName = "基础版本目录", ContentDir))
	FDirectoryPath BaseVersionDirectory;

	/// 目标版本目录
	UPROPERTY(Config, EditAnywhere, Category = "Version Diff", meta = (DisplayName = "目标版本目录", ContentDir))
	FDirectoryPath TargetVersionDirectory;

	// == 高级设置 ==

	/// 显示详细日志
	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (DisplayName = "显示详细日志"))
	bool bVerboseLogging;

	/// 打包完成后打开输出目录
	UPROPERTY(Config, EditAnywhere, Category = "Advanced", meta = (DisplayName = "打包后打开输出目录"))
	bool bOpenOutputDirectoryAfterPackage;

	// == 最小包默认配置 ==

	/// 默认启用最小包模式
	UPROPERTY(Config, EditAnywhere, Category = "MinimalPackage", meta = (DisplayName = "默认启用最小包模式"))
	bool bDefaultEnableMinimalPackage;

	/// 默认依赖处理策略
	UPROPERTY(Config, EditAnywhere, Category = "MinimalPackage", meta = (DisplayName = "默认依赖策略"))
	EHotUpdateDependencyStrategy DefaultDependencyStrategy;

	/// 默认必须包含的目录
	UPROPERTY(Config, EditAnywhere, Category = "MinimalPackage", meta = (DisplayName = "默认必须包含的目录", ContentDir))
	TArray<FDirectoryPath> DefaultWhitelistDirectories;

	/// 获取设置实例
	static UHotUpdateEditorSettings* Get();

	/// 获取默认最小包配置
	FHotUpdateMinimalPackageConfig GetDefaultMinimalPackageConfig() const;

	/// 分配新的Chunk ID
	int32 AllocateChunkId();
};