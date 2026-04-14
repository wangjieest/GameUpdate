// Copyright czm. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Manifest/HotUpdateManifest.h"
#include "HotUpdateManifestParser.generated.h"

/**
 * Manifest 解析器
 */
UCLASS(BlueprintType)
class HOTUPDATE_API UHotUpdateManifestParser : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HotUpdate|Manifest")
	static bool LoadFromFile(const FString& FilePath, FHotUpdateManifest& OutManifest);

	UFUNCTION(BlueprintCallable, Category = "HotUpdate|Manifest")
	static bool ParseFromJson(const FString& JsonString, FHotUpdateManifest& OutManifest);

	UFUNCTION(BlueprintCallable, Category = "HotUpdate|Manifest")
	static bool SaveToFile(const FString& FilePath, const FHotUpdateManifest& Manifest);

	UFUNCTION(BlueprintCallable, Category = "HotUpdate|Manifest")
	static FString ToJsonString(const FHotUpdateManifest& Manifest);
};