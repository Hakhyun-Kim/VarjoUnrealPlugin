// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "DefaultXRCamera.h"
#include "VarjoHMD.h"

class FVarjoXRCamera : public FDefaultXRCamera
{
public:
	FVarjoXRCamera(const FAutoRegister& AutoRegister, FVarjoHMD& InTrackingSystem, int32 InDeviceId)
		: FDefaultXRCamera(AutoRegister, &InTrackingSystem, InDeviceId)
	{}

	bool UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	bool HeadtrackingEnabled = true;
};
