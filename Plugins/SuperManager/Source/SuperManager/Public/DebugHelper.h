#pragma once

namespace SMDebug
{
	inline void Print(const FString& Message, const FColor& Color)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, Message);
		}
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message)
	}
}
