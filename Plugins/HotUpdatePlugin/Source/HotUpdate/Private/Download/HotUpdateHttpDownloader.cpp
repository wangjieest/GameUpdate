// Copyright czm. All Rights Reserved.

#include "Download/HotUpdateHttpDownloader.h"
#include "HotUpdate.h"
#include "Core/HotUpdateSettings.h"
#include "Core/HotUpdateFileUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"

UHotUpdateHttpDownloader::UHotUpdateHttpDownloader()
	: MaxConcurrentDownloads(3)
	, ChunkSize(4 * 1024 * 1024) // 4MB
	, MaxRetryCount(3)
	, RetryInterval(2.0f)
		, DownloadTimeout(300.0f)
		, bEnableResume(true)
	, bIsDownloading(false)
	, bIsPaused(false)
	, ActiveRequestCount(0)
	, DownloadStartTime(0.0)
	, LastProgressUpdateTime(0.0)
	, LastDownloadedBytes(0)
{
}

void UHotUpdateHttpDownloader::Initialize(int32 InMaxConcurrentDownloads, int32 InChunkSizeMB)
{
	MaxConcurrentDownloads = InMaxConcurrentDownloads;
	ChunkSize = (int64)InChunkSizeMB * 1024 * 1024;

	// 从 Settings 读取重试配置和超时设置
	UHotUpdateSettings* Settings = UHotUpdateSettings::Get();
	if (Settings)
	{
		MaxRetryCount = Settings->MaxRetryCount;
		RetryInterval = Settings->RetryInterval;
		DownloadTimeout = Settings->DownloadTimeout;
		bEnableResume = Settings->bEnableResume;
	}

	UE_LOG(LogHotUpdate, Log, TEXT("HttpDownloader initialized: MaxConcurrent=%d, ChunkSize=%lld, MaxRetry=%d"),
		MaxConcurrentDownloads, ChunkSize, MaxRetryCount);
}

void UHotUpdateHttpDownloader::AddDownloadTask(const FString& Url, const FString& SavePath, int64 ExpectedSize)
{
	TSharedPtr<FDownloadTask> Task = MakeShareable(new FDownloadTask());
	Task->Url = Url;
	Task->SavePath = SavePath;
	Task->TempPath = GetTempFilePath(SavePath);
	Task->ExpectedSize = ExpectedSize;
	Task->DownloadedSize = 0;
	Task->ResumeOffset = 0;
	Task->bIsCompleted = false;
	Task->bSuccess = false;
	Task->bSupportsResume = false;
	Task->RetryCount = 0;

	// 检查是否存在未完成的临时文件（断点续传）
	if (bEnableResume)
	{
		Task->ResumeOffset = GetExistingTempFileSize(Task->TempPath);
		if (Task->ResumeOffset > 0)
		{
			UE_LOG(LogHotUpdate, Log, TEXT("Found partial download, will resume from offset %lld: %s"), Task->ResumeOffset, *SavePath);
		}
	}

	PendingTasks.Add(Task);
	UE_LOG(LogHotUpdate, Verbose, TEXT("Added download task: %s -> %s"), *Url, *SavePath);
}

void UHotUpdateHttpDownloader::AddDownloadTasks(const TArray<FHotUpdateFileInfo>& Files, const FString& BaseUrl, const FString& SaveDir)
{
	for (const FHotUpdateFileInfo& File : Files)
	{
		FString FullUrl = BaseUrl / File.FilePath;
		FString SavePath = SaveDir / File.FilePath;
		AddDownloadTask(FullUrl, SavePath, File.FileSize);
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Added %d download tasks"), Files.Num());
}

void UHotUpdateHttpDownloader::AddContainerDownloadTasks(const TArray<FHotUpdateContainerInfo>& Containers, const FString& BaseUrl, const FString& SaveDir)
{
	for (const FHotUpdateContainerInfo& Container : Containers)
	{
		// 下载 .utoc 文件
		if (!Container.UtocPath.IsEmpty() && Container.UtocSize > 0)
		{
			FString FullUrl = BaseUrl.IsEmpty() ? Container.CustomDownloadUrl : BaseUrl / Container.UtocPath;
			FString SavePath = SaveDir / Container.UtocPath;
			AddDownloadTask(FullUrl, SavePath, Container.UtocSize);
			UE_LOG(LogHotUpdate, Log, TEXT("Added container utoc download: %s (%lld bytes)"), *Container.UtocPath, Container.UtocSize);
		}

		// 下载 .ucas 文件（如果有独立文件）
		if (!Container.UcasPath.IsEmpty() && Container.UcasSize > 0)
		{
			FString FullUrl = BaseUrl.IsEmpty() ? Container.CustomDownloadUrl : BaseUrl / Container.UcasPath;
			FString SavePath = SaveDir / Container.UcasPath;
			AddDownloadTask(FullUrl, SavePath, Container.UcasSize);
			UE_LOG(LogHotUpdate, Log, TEXT("Added container ucas download: %s (%lld bytes)"), *Container.UcasPath, Container.UcasSize);
		}
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Added %d container download tasks"), Containers.Num());
}

void UHotUpdateHttpDownloader::StartDownload()
{
	if (bIsDownloading)
	{
		UE_LOG(LogHotUpdate, Warning, TEXT("Download already in progress"));
		return;
	}

	bIsDownloading = true;
	bIsPaused = false;
	DownloadStartTime = FPlatformTime::Seconds();
	LastProgressUpdateTime = DownloadStartTime;
	LastDownloadedBytes = 0;

	CurrentProgress = FHotUpdateProgress();
	CurrentProgress.TotalFiles = PendingTasks.Num();

	// 计算总大小
	for (const TSharedPtr<FDownloadTask>& Task : PendingTasks)
	{
		CurrentProgress.TotalBytes += Task->ExpectedSize;
	}

	UE_LOG(LogHotUpdate, Log, TEXT("Starting download: %d files, %lld bytes"), CurrentProgress.TotalFiles, CurrentProgress.TotalBytes);

	ProcessNextTask();
}

void UHotUpdateHttpDownloader::PauseDownload()
{
	bIsPaused = true;
	UE_LOG(LogHotUpdate, Log, TEXT("Download paused"));
}

void UHotUpdateHttpDownloader::ResumeDownload()
{
	bIsPaused = false;
	ProcessNextTask();
	UE_LOG(LogHotUpdate, Log, TEXT("Download resumed"));
}

void UHotUpdateHttpDownloader::CancelDownload()
{
	bIsDownloading = false;
	bIsPaused = false;

	// 取消所有活跃请求
	for (TSharedPtr<IHttpRequest>& Request : ActiveRequests)
	{
		if (Request.IsValid())
		{
			Request->CancelRequest();
		}
	}
	ActiveRequests.Empty();

	// 清理任务
	PendingTasks.Empty();
	ActiveTasks.Empty();
	CompletedTasks.Empty();

	UE_LOG(LogHotUpdate, Log, TEXT("Download cancelled"));
}

void UHotUpdateHttpDownloader::ProcessNextTask()
{
	if (!bIsDownloading || bIsPaused)
	{
		return;
	}

	// 检查是否所有任务完成
	if (PendingTasks.Num() == 0 && ActiveTasks.Num() == 0)
	{
		bIsDownloading = false;

		// 检查是否所有任务成功
		bool bAllSuccess = true;
		for (const TSharedPtr<FDownloadTask>& Task : CompletedTasks)
		{
			if (!Task->bSuccess)
			{
				bAllSuccess = false;
				break;
			}
		}

		UE_LOG(LogHotUpdate, Log, TEXT("Download complete. Success: %s"), bAllSuccess ? TEXT("true") : TEXT("false"));
		OnComplete.Broadcast(bAllSuccess, bAllSuccess ? TEXT("") : TEXT("Some files failed to download"));
		return;
	}

	// 启动新任务（不超过最大并发数）
	while (ActiveRequestCount < MaxConcurrentDownloads && PendingTasks.Num() > 0)
	{
		TSharedPtr<FDownloadTask> Task = PendingTasks[0];
		PendingTasks.RemoveAt(0);
		ActiveTasks.Add(Task);

		// 确保目录存在
		UHotUpdateFileUtils::EnsureDirectoryExists(FPaths::GetPath(Task->SavePath));

		// 创建 HTTP 请求
		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Task->Url);
		Request->SetVerb(TEXT("GET"));
		Request->SetTimeout(DownloadTimeout);

		// 断点续传：检查是否有部分下载的文件
		if (Task->ResumeOffset > 0)
		{
			Request->AppendToHeader(TEXT("Range"), *FString::Printf(TEXT("bytes=%lld-"), Task->ResumeOffset));
			Task->DownloadedSize = Task->ResumeOffset;
			UE_LOG(LogHotUpdate, Log, TEXT("Resuming download from byte %lld: %s"), Task->ResumeOffset, *Task->Url);
		}

		Request->OnProcessRequestComplete().BindUObject(this, &UHotUpdateHttpDownloader::HandleRequestComplete, Task);

		// 注意: UE5.7 中 IHttpRequest 已移除 OnRequestProgress 接口
		// 进度更新在 HandleRequestComplete 中进行，暂不支持实时下载进度回调

		ActiveRequests.Add(Request);
		ActiveRequestCount++;

		UE_LOG(LogHotUpdate, Verbose, TEXT("Starting download: %s"), *Task->Url);
		Request->ProcessRequest();
	}
}

void UHotUpdateHttpDownloader::HandleRequestComplete(TSharedPtr<IHttpRequest> Request, TSharedPtr<IHttpResponse> Response, bool bSuccess, TSharedPtr<FDownloadTask> Task)
{
	ActiveRequestCount--;

	// 从活跃请求列表移除
	for (int32 i = ActiveRequests.Num() - 1; i >= 0; i--)
	{
		if (ActiveRequests[i] == Request)
		{
			ActiveRequests.RemoveAt(i);
			break;
		}
	}

	// 检查响应状态
	int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
	bool bIsPartialContent = (ResponseCode == 206);  // Partial Content
	bool bIsFullContent = (ResponseCode >= 200 && ResponseCode < 300 && ResponseCode != 206);

	bool bRequestSuccess = bSuccess && Response.IsValid() && (bIsPartialContent || bIsFullContent);

	if (bRequestSuccess)
	{
		// 获取响应内容
		const TArray<uint8>& Content = Response->GetContent();
		int64 DataSize = Content.Num();

		bool bSaveSuccess = false;

		if (bIsPartialContent && Task->ResumeOffset > 0)
		{
			// 断点续传：追加到现有文件
			bSaveSuccess = AppendDataToFile(Task->TempPath, Content);
			if (bSaveSuccess)
			{
				UE_LOG(LogHotUpdate, Log, TEXT("Resumed download completed: %s (total: %lld bytes)"), *Task->SavePath, Task->ResumeOffset + DataSize);
			}
		}
		else
		{
			// 全新下载：直接保存
			bSaveSuccess = FFileHelper::SaveArrayToFile(Content, *Task->TempPath);
			if (bSaveSuccess)
			{
				UE_LOG(LogHotUpdate, Verbose, TEXT("Downloaded: %s (%lld bytes)"), *Task->SavePath, DataSize);
			}
		}

		if (bSaveSuccess)
		{
			// 重命名临时文件为最终文件
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (PlatformFile.FileExists(*Task->SavePath))
			{
				PlatformFile.DeleteFile(*Task->SavePath);
			}

			if (PlatformFile.MoveFile(*Task->SavePath, *Task->TempPath))
			{
				Task->DownloadedSize = Task->ResumeOffset + DataSize;
				Task->bIsCompleted = true;
				Task->bSuccess = true;
			}
			else
			{
				UE_LOG(LogHotUpdate, Error, TEXT("Failed to move temp file to final location: %s"), *Task->SavePath);
				bRequestSuccess = false;
			}
		}
		else
		{
			UE_LOG(LogHotUpdate, Error, TEXT("Failed to save file: %s"), *Task->TempPath);
			bRequestSuccess = false;
		}
	}
	else
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to download: %s (Response Code: %d)"), *Task->Url, ResponseCode);
	}

	// 处理失败：检查是否需要重试
	if (!bRequestSuccess)
	{
		Task->RetryCount++;
		if (Task->RetryCount <= MaxRetryCount)
		{
			UE_LOG(LogHotUpdate, Warning, TEXT("Download failed, retrying (%d/%d) after %.1fs: %s"),
				Task->RetryCount, MaxRetryCount, RetryInterval, *Task->Url);

			// 从活跃任务移除，延迟后重新加入待下载队列
			ActiveTasks.Remove(Task);

			// 使用定时器延迟重试
			FTimerHandle RetryTimerHandle;
			FTimerDelegate RetryDelegate;
			RetryDelegate.BindUObject(this, &UHotUpdateHttpDownloader::RetryTask, Task);

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(RetryTimerHandle, RetryDelegate, RetryInterval, false);
			}
			else
			{
				// 如果没有 World，直接重新加入队列
				RetryTask(Task);
			}

			return;
		}

		// 超过重试次数，标记为失败
		Task->bIsCompleted = true;
		Task->bSuccess = false;
		UE_LOG(LogHotUpdate, Error, TEXT("Download failed after %d retries: %s"), MaxRetryCount, *Task->Url);
	}

	// 移动到完成列表
	ActiveTasks.Remove(Task);
	CompletedTasks.Add(Task);

	CurrentProgress.CurrentFileIndex = CompletedTasks.Num();
	OnFileComplete.Broadcast(Task->SavePath, Task->bSuccess);

	UpdateProgress();
	ProcessNextTask();
}

void UHotUpdateHttpDownloader::UpdateProgress()
{
	// 计算已下载字节数
	int64 TotalDownloaded = 0;
	for (const TSharedPtr<FDownloadTask>& Task : ActiveTasks)
	{
		TotalDownloaded += Task->DownloadedSize;
	}
	for (const TSharedPtr<FDownloadTask>& Task : CompletedTasks)
	{
		TotalDownloaded += Task->DownloadedSize;
	}

	CurrentProgress.DownloadedBytes = TotalDownloaded;

	// 计算速度和剩余时间
	double CurrentTime = FPlatformTime::Seconds();
	double ElapsedTime = CurrentTime - LastProgressUpdateTime;

	if (ElapsedTime > 0.5) // 每 0.5 秒更新一次速度
	{
		int64 BytesSinceLastUpdate = TotalDownloaded - LastDownloadedBytes;
		CurrentProgress.DownloadSpeed = (float)(BytesSinceLastUpdate / ElapsedTime);

		if (CurrentProgress.DownloadSpeed > 0)
		{
			int64 RemainingBytes = CurrentProgress.TotalBytes - TotalDownloaded;
			CurrentProgress.RemainingTime = (float)(RemainingBytes / CurrentProgress.DownloadSpeed);
		}

		LastProgressUpdateTime = CurrentTime;
		LastDownloadedBytes = TotalDownloaded;
	}

	OnProgress.Broadcast(CurrentProgress);
}

void UHotUpdateHttpDownloader::RetryTask(TSharedPtr<FDownloadTask> Task)
{
	if (!bIsDownloading)
	{
		return;
	}

	// 重置断点续传偏移（保留已下载的临时文件用于续传）
	Task->ResumeOffset = GetExistingTempFileSize(Task->TempPath);
	Task->DownloadedSize = Task->ResumeOffset;

	PendingTasks.Add(Task);
	ProcessNextTask();
}

FString UHotUpdateHttpDownloader::GetTempFilePath(const FString& OriginalPath) const
{
	return OriginalPath + TEXT(".tmp");
}

int64 UHotUpdateHttpDownloader::GetExistingTempFileSize(const FString& TempPath) const
{
	return IFileManager::Get().FileSize(*TempPath);
}

bool UHotUpdateHttpDownloader::AppendDataToFile(const FString& FilePath, const TArray<uint8>& Data)
{
	// 使用追加模式打开文件
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FilePath, FILEWRITE_Append);
	if (!FileWriter)
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to open file for appending: %s"), *FilePath);
		return false;
	}

	FileWriter->Serialize(const_cast<uint8*>(Data.GetData()), Data.Num());

	// 检查写入是否成功
	if (FileWriter->IsError())
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Failed to write data to file: %s"), *FilePath);
		delete FileWriter;
		return false;
	}

	// 关闭文件前检查是否有错误
	bool bCloseSuccess = !FileWriter->IsError();
	delete FileWriter;

	if (!bCloseSuccess)
	{
		UE_LOG(LogHotUpdate, Error, TEXT("Error detected when closing file: %s"), *FilePath);
		return false;
	}

	return true;
}