#pragma once

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
}
