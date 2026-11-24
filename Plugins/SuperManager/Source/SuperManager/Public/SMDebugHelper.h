#pragma once

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace SMDebug
{
	inline void Print(const FString& Message)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::MakeRandomColor(), Message);
		}
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message)
	}

	/** 自定义弹窗 */
	inline EAppReturnType::Type ShowMsgDialog(
		EAppMsgType::Type MsgType,
		const FString& Message,
		const FString& Title = TEXT("Warning"))
	{
#if WITH_EDITOR
		return FMessageDialog::Open(MsgType, FText::FromString(Message), FText::FromString(Title));
#else
		return EAppReturnType::Ok;
#endif
	}

	/** 自定义右下角通知 */
	inline void ShowNotifyInfo(const FString& Message)
	{
#if WITH_EDITOR
		FNotificationInfo NotifyInfo(FText::FromString(Message));
		NotifyInfo.bUseLargeFont = true;
		NotifyInfo.ExpireDuration = 7.f;

		FSlateNotificationManager::Get().AddNotification(NotifyInfo);
#endif
	}
}
