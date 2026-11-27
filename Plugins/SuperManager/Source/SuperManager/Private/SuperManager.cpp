// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperManager.h"

#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "FSuperManagerModule"

void FSuperManagerModule::StartupModule()
{
	InitCBMenuExtension();
}

void FSuperManagerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FSuperManagerModule::InitCBMenuExtension()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FContentBrowserMenuExtender_SelectedPaths>& ContextMenuExtenders =
		ContentBrowserModule.GetAllPathViewContextMenuExtenders();

	FContentBrowserMenuExtender_SelectedPaths CustomCBMenuDelegate;
	ContextMenuExtenders.Add(FContentBrowserMenuExtender_SelectedPaths::CreateRaw(
		this, &FSuperManagerModule::CustomCBMenuExtender)
	);
}

TSharedRef<FExtender> FSuperManagerModule::CustomCBMenuExtender(const TArray<FString>& SelectedPaths)
{
	TSharedRef<FExtender> MenuExtender (new FExtender());

	if (SelectedPaths.Num() > 0)
	{
		MenuExtender->AddMenuExtension(
			FName("Delete"),
			EExtensionHook::After,
			TSharedPtr<FUICommandList>(),
			FMenuExtensionDelegate::CreateRaw(this, &FSuperManagerModule::AddCBMenuEntry)
		);
	}
	
	return MenuExtender;
}

void FSuperManagerModule::AddCBMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Delete Unused Assets")),
		FText::FromString(TEXT("删除该文件夹下所有未使用的Assets")),
		FSlateIcon(),
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnDeleteUnusedAssetButtonClicked)
	);
}

void FSuperManagerModule::OnDeleteUnusedAssetButtonClicked()
{
	
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSuperManagerModule, SuperManager)
