// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetActions/QuickAssetAction.h"

#include "DebugHelper.h"
#include "EditorUtilityLibrary.h"
#include "EditorAssetLibrary.h"

void UQuickAssetAction::DuplicateAssets(int32 NumOfDuplicates)
{
	if (NumOfDuplicates <= 0)
	{
		SMDebug::Print(TEXT("复制资产时，请输入有效的复制数"));
		return;
	}

	TArray<FAssetData> SelectedAssetDatas = UEditorUtilityLibrary::GetSelectedAssetData();
	if (SelectedAssetDatas.IsEmpty())
	{
		SMDebug::Print(TEXT("请先选择要复制的资产"));
		return;
	}

	int32 Counter = 0;

	for (const FAssetData& SelectedAssetData : SelectedAssetDatas)
	{
		if (!SelectedAssetData.IsValid())
		{
			continue;
		}

		// PackageName = such as /Game/Path/Package
		const FString SourcePackageName = SelectedAssetData.PackageName.ToString();
		const FString SourceAssetName = SelectedAssetData.AssetName.ToString();

		for (int32 i = 0; i < NumOfDuplicates; ++i)
		{
			const FString NewAssetName = FString::Printf(TEXT("%s_%d"), *SourceAssetName, i);

			FString SourcePackagePath = SelectedAssetData.PackagePath.ToString();
			const FString NewPathName = FPaths::Combine(SourcePackagePath, NewAssetName);

			if (UEditorAssetLibrary::DoesAssetExist(NewPathName))
			{
				continue;
			}

			if (UEditorAssetLibrary::DuplicateAsset(SourcePackageName, NewPathName))
			{
				if (UEditorAssetLibrary::SaveAsset(NewPathName, false))
				{
					++Counter;
				}
			}
		}
	}

	if (Counter > 0)
	{
		SMDebug::Print(FString::Printf(TEXT("成功复制%d个文件"), Counter));
	}
	else
	{
		SMDebug::Print(TEXT("没有资产被复制"));
	}
}
