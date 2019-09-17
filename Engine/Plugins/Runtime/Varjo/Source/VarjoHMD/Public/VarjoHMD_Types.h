// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "HeadMountedDisplayBase.h"
#include "IVarjoHMDPlugin.h"
#include "XRRenderTargetManager.h"
#include "IMotionController.h"
#include "SceneViewExtension.h"
#include "VarjoHMD_Types.generated.h"

UENUM(BlueprintType)
enum class HMDVisiblityStatus : uint8
{
	HMDUnknown,
	HMDNotVisible,
	HMDVisible
};

/** Up to 8 motion controller devices supported (two VR motion controllers per Unreal controller, one for either the left or right hand.) */
#define MAX_VARJOVR_CONTROLLER_PAIRS 4

VARJOHMD_API DECLARE_LOG_CATEGORY_EXTERN(LogVarjoHMD, Log, All);