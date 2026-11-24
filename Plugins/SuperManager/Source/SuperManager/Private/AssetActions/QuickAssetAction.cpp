// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetActions/QuickAssetAction.h"

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

	FixUpRedirectors();

	for (FAssetData SelectedAssetData : SelectedAssetDataArray)
	{
		TArray<FString> AssetReferrers =
			UEditorAssetLibrary::FindPackageReferencersForAsset(SelectedAssetData.GetObjectPathString());

		if (AssetReferrers.IsEmpty())
		{
			UnusedAssetDataArray.Add(SelectedAssetData);
		}
	}

	if (UnusedAssetDataArray.IsEmpty())
	{
		SMDebug::ShowMsgDialog(EAppMsgType::Ok, TEXT("选中的Assets中没有没被使用过的，无法通过该方法删除"));
		return;
	}

	const int32 NumOfAssetsDeleted = ObjectTools::DeleteAssets(UnusedAssetDataArray);
	if (NumOfAssetsDeleted == 0) return;

	SMDebug::ShowNotifyInfo(FString::Printf(TEXT("成功删除了%d个没使用过的文件"), NumOfAssetsDeleted));
}

void UQuickAssetAction::FixUpRedirectors()
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Emplace("/Game");
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	if (AssetList.IsEmpty()) return;

	TArray<FString> ObjectPaths;
	for (const FAssetData& AssetData : AssetList)
	{
		ObjectPaths.Add(AssetData.GetObjectPathString());
	}

	TArray<UObject*> Objects;

	AssetViewUtils::FLoadAssetsSettings Settings;
	Settings.bFollowRedirectors = false;
	Settings.bAllowCancel = true;

	AssetViewUtils::ELoadAssetsResult Result = AssetViewUtils::LoadAssetsIfNeeded(ObjectPaths, Objects, Settings);
	if (Result != AssetViewUtils::ELoadAssetsResult::Cancelled)
	{
		TArray<UObjectRedirector*> Redirectors;
		for (UObject* Object : Objects)
		{
			Redirectors.Add(CastChecked<UObjectRedirector>(Object));
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetToolsModule.Get().FixupReferencers(Redirectors);
	}
}
