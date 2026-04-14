// Copyright czm. All Rights Reserved.

#include "HotUpdateEditor.h"
#include "HotUpdateEditorStyle.h"
#include "HotUpdateContentBrowserExtension.h"
#include "Widgets/HotUpdateMainWindow.h"
#include "Widgets/HotUpdatePackagingPanel.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Framework/Docking/WorkspaceItem.h"

// 日志分类定义
DEFINE_LOG_CATEGORY(LogHotUpdateEditor);

#define LOCTEXT_NAMESPACE "FHotUpdateEditorModule"

static const FName HotUpdateTabName("HotUpdateTools");

// 定义待定数据静态成员
int32 FHotUpdatePendingData::InitialTab = 0;
TArray<FString> FHotUpdatePendingData::AssetPaths;
EHotUpdatePackageType FHotUpdatePendingData::PackageType = EHotUpdatePackageType::Asset;
bool FHotUpdatePendingData::bNeedReRegisterSpawner = false;

/** 注册 Nomad Tab Spawner */
static void RegisterHotUpdateTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		HotUpdateTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
		{
			// 使用 MajorTab 角色（参考 Session Frontend），使 SetMenuMultiBox 能嵌入标题栏
			const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
				.TabRole(ETabRole::MajorTab)
				.Label(LOCTEXT("HotUpdateTab", "热更新工具"))
				.OnTabClosed_Lambda([](TSharedRef<SDockTab>)
				{
					// 标记需要重新注册 spawner，确保下次打开时创建全新实例
					FHotUpdatePendingData::bNeedReRegisterSpawner = true;
				});

			// 创建主窗口控件，传入 MajorTab 和所属窗口
			TSharedRef<SHotUpdateMainWindow> MainWindow = SNew(SHotUpdateMainWindow, MajorTab, SpawnTabArgs.GetOwnerWindow());

			// 应用待定数据
			if (FHotUpdatePendingData::InitialTab > 0)
			{
				MainWindow->SetInitialTab(FHotUpdatePendingData::InitialTab);
				FHotUpdatePendingData::InitialTab = 0;
			}
			if (FHotUpdatePendingData::AssetPaths.Num() > 0)
			{
				MainWindow->SetAssetPaths(FHotUpdatePendingData::AssetPaths);
				MainWindow->SetPackageType(FHotUpdatePendingData::PackageType);
				FHotUpdatePendingData::AssetPaths.Empty();
			}

			MajorTab->SetContent(MainWindow);

			return MajorTab;
		})
	)
	.SetDisplayName(LOCTEXT("HotUpdateTabTitle", "热更新工具"))
	.SetTooltipText(LOCTEXT("HotUpdateTabTooltip", "打开热更新工具窗口"))
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.CookContent"))
	.SetAutoGenerateMenuEntry(false)
	.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
}

/** 全局打开热更新工具窗口 */
void HotUpdateOpenTab(int32 InitialTab)
{
	FHotUpdatePendingData::InitialTab = InitialTab;

	if (FHotUpdatePendingData::bNeedReRegisterSpawner)
	{
		// Tab 关闭后需要重新注册 spawner，确保 TryInvokeTab 创建全新实例
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HotUpdateTabName);
		RegisterHotUpdateTabSpawner();
		FHotUpdatePendingData::bNeedReRegisterSpawner = false;
	}

	FGlobalTabmanager::Get()->TryInvokeTab(HotUpdateTabName);
}

class FHotUpdateEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHotUpdateEditor, Log, TEXT("HotUpdateEditor module started"));

		// 初始化样式
		FHotUpdateEditorStyle::Initialize();

		// 注册Content Browser扩展
		FHotUpdateContentBrowserExtension::Register();

		// 注册菜单
		RegisterMenus();

		// 注册Tab
		RegisterHotUpdateTabSpawner();
	}

	virtual void ShutdownModule() override
	{
		// 取消注册 ToolMenus 回调
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		// 取消注册 Tab Spawner
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HotUpdateTabName);

		// 注销Content Browser扩展
		FHotUpdateContentBrowserExtension::Unregister();

		// 清理样式
		FHotUpdateEditorStyle::Shutdown();

		UE_LOG(LogHotUpdateEditor, Log, TEXT("HotUpdateEditor module shutdown"));
	}

private:
	void RegisterMenus()
	{
		// 使用 FToolMenuOwnerScoped 确保正确的所有权和清理
		FToolMenuOwnerScoped OwnerScoped(this);

		// 扩展工具菜单
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
		{
			FToolMenuOwnerScoped MenuOwnerScoped(this);

			// 在工具菜单下添加热更新菜单
			UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection("HotUpdate");

			Section.Label = LOCTEXT("HotUpdateSection", "热更新");
			Section.InsertPosition = FToolMenuInsert("Programming", EToolMenuInsertType::Before);

			// 热更新工具（统一入口）
			Section.AddMenuEntry(
				"HotUpdateTools",
				LOCTEXT("HotUpdateTools", "热更新工具"),
				LOCTEXT("HotUpdateToolsTooltip", "打开热更新工具窗口（包含打包和版本比较功能）"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.CookContent"),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					HotUpdateOpenTab(0);
				}))
			);
		}));

		// 注册右键菜单扩展
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
		{
			FToolMenuOwnerScoped MenuOwnerScoped(this);

			// 内容浏览器资产右键菜单
			UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
			FToolMenuSection& Section = AssetContextMenu->AddSection("HotUpdateOperations", LOCTEXT("HotUpdateOperations", "热更新"));

			Section.AddMenuEntry(
				"Asset_HotUpdatePackage",
				LOCTEXT("AssetHotUpdatePackage", "热更新打包"),
				LOCTEXT("AssetHotUpdatePackageTooltip", "将选中资源打包为热更新Pak文件"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.CookContent"),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					TArray<FAssetData> SelectedAssets;
					ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

					if (SelectedAssets.Num() > 0)
					{
						// 收集资源路径
						FHotUpdatePendingData::AssetPaths.Empty();
						for (const FAssetData& Asset : SelectedAssets)
						{
							FHotUpdatePendingData::AssetPaths.Add(Asset.PackageName.ToString());
						}
						FHotUpdatePendingData::PackageType = EHotUpdatePackageType::Asset;
						HotUpdateOpenTab(1);
					}
				}))
			);
		}));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHotUpdateEditorModule, HotUpdateEditor)