// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoLogFunctionLibrary.h"
#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

UVarjoLogFunctionLibrary::UVarjoLogFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVarjoLogFunctionLibrary::PrintVerboseLog(UObject* WorldContextObject, const FString& InString, bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration)
{
	if (UE_LOG_ACTIVE(LogVarjoHMD, Verbose))
	{
		UKismetSystemLibrary::PrintString(WorldContextObject, InString, bPrintToScreen, bPrintToLog, TextColor, Duration);
	}
}

void UVarjoLogFunctionLibrary::EnableVarjoVerboseLog(bool Enable)
{
	if (Enable)
	{
		UE_SET_LOG_VERBOSITY(LogVarjoHMD, Verbose);
	}
	else
	{
		UE_SET_LOG_VERBOSITY(LogVarjoHMD, Log);
	}
}
