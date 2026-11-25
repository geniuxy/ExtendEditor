// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetActions/QuickAssetAction.h"

#include "Engine/Engine.h"
#include "Misc/MessageDialog.h"
#include "SMDebugHelper.h"
#include "EditorUtilityLibrary.h"
#include "EditorAssetLibrary.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UQuickAssetAction::DuplicateAssets(int32 NumOfDuplicates)
{
	if (NumOfDuplicates <= 0)
	{
		SMDebug::ShowMsgDialog(EAppMsgType::Ok, TEXT("复制资产时，请输入有效的复制数"));
		return;
	}

	TArray<FAssetData> SelectedAssetDatas = UEditorUtilityLibrary::GetSelectedAssetData();
	if (SelectedAssetDatas.IsEmpty())
	{
		SMDebug::ShowMsgDialog(EAppMsgType::Ok, TEXT("请先选择要复制的资产"));
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
		SMDebug::ShowNotifyInfo(FString::Printf(TEXT("成功复制%d个文件"), Counter));
	}
	else
	{
		SMDebug::ShowMsgDialog(EAppMsgType::Ok, TEXT("没有资产被复制"));
	}
}

void UQuickAssetAction::AddPrefixes()
{
	TArray<UObject*> SelectedObjects = UEditorUtilityLibrary::GetSelectedAssets();
	uint32 Counter = 0;

	for (UObject* SelectedObject : SelectedObjects)
	{
		if (!SelectedObject) continue;

		FString* PrefixFound = PrefixMap.Find(SelectedObject->GetClass());
		if (!PrefixFound || PrefixFound->IsEmpty())
		{
			SMDebug::Print(FString::Printf(TEXT("找不到前缀，类别class为%s"), *SelectedObject->GetClass()->GetName()));
			continue;
		}

		FString OldName = SelectedObject->GetName();
		if (OldName.StartsWith(*PrefixFound))
		{
			SMDebug::Print(FString::Printf(TEXT("%s已经有前缀了"), *OldName));
			continue;
		}

		if (SelectedObject->IsA<UMaterialInstanceConstant>())
		{
			OldName.RemoveFromStart("M_");
			OldName.RemoveFromEnd("_Inst");
		}

		const FString NewNameWithPrefix = *PrefixFound + OldName;
		UEditorUtilityLibrary::RenameAsset(SelectedObject, NewNameWithPrefix);
		++Counter;
	}

	if (Counter)
	{
		SMDebug::ShowNotifyInfo(FString::Printf(TEXT("成功添加%d个文件的前缀"), Counter));
	}
}

void UQuickAssetAction::RemoveUnusedAssets()
{
	TArray<FAssetData> SelectedAssetDataArray = UEditorUtilityLibrary::GetSelectedAssetData();
	TArray<FAssetData> UnusedAssetDataArray;
	TArray<FString> SkippedReferencedAssets;

	if (SelectedAssetDataArray.IsEmpty())
	{
		SMDebug::ShowMsgDialog(EAppMsgType::Ok, TEXT("请先选择要检查的资产"));
		return;
	}

	if (!FixUpRedirectors())
	{
		SMDebug::ShowMsgDialog(
			EAppMsgType::Ok,
			TEXT("重定向器没有全部修复成功，已停止删除。请先处理 Redirector Update Report 中的错误。"));
		return;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Refresh on-disk dependencies before deciding whether anything is unused.
	TArray<FString> ScanPaths{TEXT("/Game")};
	AssetRegistry.ScanPathsSynchronous(ScanPaths, true);

	for (const FAssetData& OldAssetData : SelectedAssetDataArray)
	{
		const FAssetData SelectedAssetData =
			AssetRegistry.GetAssetByObjectPath(OldAssetData.GetSoftObjectPath());
		if (!SelectedAssetData.IsValid())
		{
			continue;
		}

		// Include both soft and hard on-disk package references.
		TArray<FName> AssetReferrers;
		AssetRegistry.GetReferencers(SelectedAssetData.PackageName, AssetReferrers);
		AssetReferrers.Remove(SelectedAssetData.PackageName);

		if (!AssetReferrers.IsEmpty())
		{
			SkippedReferencedAssets.Add(SelectedAssetData.PackageName.ToString());
			continue;
		}

		// The registry can miss in-memory references. Use the editor delete flow's
		// reference analysis as a second check and fail closed on load errors.
		UObject* LoadedAsset = SelectedAssetData.GetAsset();
		if (!LoadedAsset)
		{
			SkippedReferencedAssets.Add(SelectedAssetData.PackageName.ToString());
			continue;
		}

		bool bReferencedInMemory = false;
		bool bReferencedByUndo = false;
		ObjectTools::GatherObjectReferencersForDeletion(
			LoadedAsset,
			bReferencedInMemory,
			bReferencedByUndo);

		if (bReferencedInMemory)
		{
			SkippedReferencedAssets.Add(SelectedAssetData.PackageName.ToString());
			continue;
		}

		UnusedAssetDataArray.Add(SelectedAssetData);
	}

	if (UnusedAssetDataArray.IsEmpty())
	{
		SMDebug::ShowMsgDialog(
			EAppMsgType::Ok,
			FString::Printf(
				TEXT("选中的Assets中没有完全无引用的资产，无法删除。已跳过%d个有引用或无法确认的资产。"),
				SkippedReferencedAssets.Num()));
		return;
	}

	// Never use the confirmation/force-delete path here. This performs a safe
	// delete and will not remove anything that is still referenced.
	const int32 NumOfAssetsDeleted = ObjectTools::DeleteAssets(UnusedAssetDataArray);
	if (NumOfAssetsDeleted == 0) return;

	if (SkippedReferencedAssets.IsEmpty())
	{
		SMDebug::ShowNotifyInfo(FString::Printf(TEXT("成功删除了%d个无引用资产"), NumOfAssetsDeleted));
	}
	else
	{
		SMDebug::ShowNotifyInfo(
			FString::Printf(
				TEXT("已删除%d个无引用资产，并跳过%d个有引用或无法确认的资产"),
				NumOfAssetsDeleted,
				SkippedReferencedAssets.Num()));
	}
}

bool UQuickAssetAction::FixUpRedirectors()
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// FixupReferencers can defer execution while asset discovery is running.
	// Wait so the caller never continues with stale asset data.
	AssetRegistry.WaitForCompletion();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Emplace("/Game");
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

	TArray<FAssetData> RedirectorAssets;
	AssetRegistry.GetAssets(Filter, RedirectorAssets);
	if (RedirectorAssets.IsEmpty())
	{
		return true;
	}

	TArray<UObject*> LoadedObjects;
	AssetViewUtils::FLoadAssetsSettings Settings;
	Settings.bFollowRedirectors = false;
	Settings.bAllowCancel = true;

	const AssetViewUtils::ELoadAssetsResult Result =
		AssetViewUtils::LoadAssetsIfNeeded(RedirectorAssets, LoadedObjects, Settings);
	if (Result != AssetViewUtils::ELoadAssetsResult::Success)
	{
		return false;
	}

	TArray<UObjectRedirector*> Redirectors;
	for (UObject* Object : LoadedObjects)
	{
		if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Object))
		{
			Redirectors.Add(Redirector);
		}
	}

	if (Redirectors.IsEmpty())
	{
		return true;
	}

	FAssetToolsModule& AssetToolsModule =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	IAssetTools& AssetTools = AssetToolsModule.Get();
	if (AssetTools.IsFixupReferencersInProgress())
	{
		return false;
	}

	AssetTools.FixupReferencers(Redirectors);

	// Remaining redirectors mean a reference could not be fixed, a package failed
	// to save, or the user chose to keep them. All cases are unsafe for deletion.
	TArray<FString> ScanPaths{TEXT("/Game")};
	AssetRegistry.ScanPathsSynchronous(ScanPaths, true);

	TArray<FAssetData> RemainingRedirectors;
	AssetRegistry.GetAssets(Filter, RemainingRedirectors);
	return RemainingRedirectors.IsEmpty();
}
