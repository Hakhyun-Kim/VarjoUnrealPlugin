// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoXRCamera.h"
#include "VarjoHMD.h"
#include "ScreenRendering.h"
#include "IVarjoHMDPlugin.h"

bool FVarjoXRCamera::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (HeadtrackingEnabled)
	{
		return FDefaultXRCamera::UpdatePlayerCamera(CurrentOrientation, CurrentPosition);
	}
	else
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
		return true;
	}
}