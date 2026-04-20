// Copyright czm. All Rights Reserved.

#include "HotUpdateContentBrowserExtension.h"
#include "HotUpdateEditor.h"
#include "Widgets/HotUpdateMainWindow.h"
#include "Widgets/HotUpdatePackagingPanel.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SWindow.h"
#include "Editor.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "HotUpdateContentBrowserExtension"


void FHotUpdateContentBrowserExtension::Register()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	// 注册路径视图菜单扩展
	ContentBrowserModule.GetAllPathViewContextMenuExtenders().Add(
		FContentBrowserMenuExtender_SelectedPaths::CreateLambda([](const TArray<FString>& SelectedPaths)
		{
			TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);

			Extender->AddMenuExtension(
				"PathContextOperators",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateLambda([SelectedPaths](FMenuBuilder& MenuBuilder)
				{
					MenuBuilder.BeginSection("HotUpdateSection", LOCTEXT("HotUpdateSection", "热更新"));
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("PackageDirectory", "热更新打包"),
							LOCTEXT("PackageDirectoryTooltip", "将选中目录打包为热更新Pak文件"),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([SelectedPaths]()
							{
								ExecuteHotUpdatePackage(SelectedPaths);
							}))
						);
					}
					MenuBuilder.EndSection();
				})
			);

			return Extender.ToSharedRef();
		})
	);

	// 注册资产视图菜单扩展
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateLambda([](const TArray<FAssetData>& SelectedAssets)
		{
			TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);

			Extender->AddMenuExtension(
				"AssetContextOperators",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateLambda([SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					MenuBuilder.BeginSection("HotUpdateSection", LOCTEXT("HotUpdateSection", "热更新"));
					{
						// 获取选中的资源路径
						TArray<FString> AssetPaths = GetSelectedAssetPaths(SelectedAssets);

					}
					MenuBuilder.EndSection();
				})
			);

			return Extender.ToSharedRef();
		})
	);

	UE_LOG(LogHotUpdateEditor, Log, TEXT("Content Browser扩展已注册"));
}

void FHotUpdateContentBrowserExtension::Unregister()
{
	UE_LOG(LogHotUpdateEditor, Log, TEXT("Content Browser扩展已注销"));
}

void FHotUpdateContentBrowserExtension::ExecuteHotUpdatePackage(TArray<FString> PackagePaths)
{
	if (PackagePaths.Num() == 0)
	{
		return;
	}

	// 将 UE 包路径解析为磁盘 uasset 文件路径
	TArray<FString> UassetFilePaths;
	for (const FString& PackagePath : PackagePaths)
	{
		FString ResolvedPath;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, ResolvedPath, TEXT("")))
		{
			FString AbsolutePath = FPaths::ConvertRelativePathToFull(ResolvedPath);
			if (FPaths::FileExists(AbsolutePath + TEXT(".umap")))
				UassetFilePaths.Add(AbsolutePath + TEXT(".umap"));
			else
				UassetFilePaths.Add(AbsolutePath + TEXT(".uasset"));
		}
	}

	// 设置待定数据
	FHotUpdatePendingData::UassetFilePaths = UassetFilePaths;

	// 使用全局函数打开 Tab，自动处理关闭后重新注册的逻辑
	HotUpdateOpenTab(2);
}

TArray<FString> FHotUpdateContentBrowserExtension::GetSelectedAssetPaths(const TArray<FAssetData>& SelectedAssets)
{
	TArray<FString> Paths;
	for (const FAssetData& Asset : SelectedAssets)
	{
		Paths.Add(Asset.PackageName.ToString());
	}
	return Paths;
}

#undef LOCTEXT_NAMESPACE