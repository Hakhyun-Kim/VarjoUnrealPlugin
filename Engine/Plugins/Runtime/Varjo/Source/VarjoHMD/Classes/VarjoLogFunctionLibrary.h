// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VarjoHMD_Types.h"
#include "VarjoLogFunctionLibrary.generated.h"

/**
 * VarjoLog Extensions Function Library
 */
UCLASS(ClassGroup = Varjo)
class VARJOHMD_API UVarjoLogFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Prints a string to the log, and optionally, to the screen, only happens in Develpment Mode & LogVarjoHMD has Verbose level. e.g Log LogVarjoHMD Verbose
	 * If Print To Log is true, it will be visible in the Output Log window.  Otherwise it will be logged only as 'Verbose', so it generally won't show up.
	 *
	 * @param	InString		The string to log out
	 * @param	bPrintToScreen	Whether or not to print the output to the screen
	 * @param	bPrintToLog		Whether or not to print the output to the log
	 * @param	bPrintToConsole	Whether or not to print the output to the console
	 * @param	TextColor		Whether or not to print the output to the console
	 * @param	Duration		The display duration (if Print to Screen is True). Using negative number will result in loading the duration time from the config.
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", CallableWithoutWorldContext, Keywords = "log print", AdvancedDisplay = "2", DevelopmentOnly), Category = "VarjoHMD|Utilities|String")
	static void PrintVerboseLog(UObject* WorldContextObject, const FString& InString = FString(TEXT("Hello")), bool bPrintToScreen = true, bool bPrintToLog = true, FLinearColor TextColor = FLinearColor(0.0, 0.66, 1.0), float Duration = 2.f);

	UFUNCTION(BlueprintCallable, meta = (DevelopmentOnly), Category = "VarjoHMD")
	static void EnableVarjoVerboseLog(bool Enable);
};
