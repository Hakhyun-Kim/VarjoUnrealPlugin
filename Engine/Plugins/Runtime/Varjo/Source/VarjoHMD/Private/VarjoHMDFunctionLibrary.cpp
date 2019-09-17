// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoHMDFunctionLibrary.h"
#include "VarjoHMD.h"
#include "XRMotionControllerBase.h"
#include "Engine/Engine.h"

UVarjoHMDFunctionLibrary::UVarjoHMDFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

static FVarjoHMD* GetVarjoHMD()
{
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FVarjoHMD::VarjoSystemName))
	{
		return static_cast<FVarjoHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

IMotionController* GetVarjoMotionController()
{
	static FName DeviceTypeName(TEXT("VarjoVRController"));
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (IMotionController* MotionController : MotionControllers)
	{
		if (MotionController->GetMotionControllerDeviceTypeName() == DeviceTypeName)
		{
			return MotionController;
		}
	}
	return nullptr;
}

bool UVarjoHMDFunctionLibrary::GetButtonEvent(int& button, bool& pressed)
{
	FVarjoHMD* VarjoHMD = GetVarjoHMD();
	if (VarjoHMD)
	{
		return VarjoHMD->GetButtonEvent(button, pressed);
	}
	return false;
}

void UVarjoHMDFunctionLibrary::GetValidTrackedDeviceIds(EVarjoVRTrackedDeviceType DeviceType, TArray<int32>& OutTrackedDeviceIds)
{
	OutTrackedDeviceIds.Empty();

	EXRTrackedDeviceType XRDeviceType = EXRTrackedDeviceType::Invalid;
	switch (DeviceType)
	{
	case EVarjoVRTrackedDeviceType::Controller:
		XRDeviceType = EXRTrackedDeviceType::Controller;
		break;
	case EVarjoVRTrackedDeviceType::TrackingReference:
		XRDeviceType = EXRTrackedDeviceType::TrackingReference;
		break;
	case EVarjoVRTrackedDeviceType::Other:
		XRDeviceType = EXRTrackedDeviceType::Other;
		break;
	case EVarjoVRTrackedDeviceType::Invalid:
		XRDeviceType = EXRTrackedDeviceType::Invalid;
		break;
	default:
		break;
	}


	FVarjoHMD* VarjoHMD = GetVarjoHMD();
	if (VarjoHMD)
	{
		VarjoHMD->EnumerateTrackedDevices(OutTrackedDeviceIds, XRDeviceType);
	}
}

bool UVarjoHMDFunctionLibrary::GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

	FVarjoHMD* VarjoHMD = GetVarjoHMD();
	if (VarjoHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = VarjoHMD->GetCurrentPose(DeviceId, DeviceOrientation, OutPosition);
		OutOrientation = DeviceOrientation.Rotator();
	}

	return RetVal;
}

bool UVarjoHMDFunctionLibrary::GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

	IMotionController* VarjoMotionController = GetVarjoMotionController();
	if (VarjoMotionController)
	{
		// Note: the Varjo motion controller ignores the WorldToMeters scale argument.
		RetVal = static_cast<FXRMotionControllerBase*>(VarjoMotionController)->GetControllerOrientationAndPosition(ControllerIndex, Hand, OutOrientation, OutPosition, -1.0f);
	}

	return RetVal;
}

HMDVisiblityStatus UVarjoHMDFunctionLibrary::GetHMDVisility()
{
	FVarjoHMD* VarjoHMD = GetVarjoHMD();
	if (VarjoHMD)
	{
		return VarjoHMD->GetHMDVisibility();
	}
	return HMDVisiblityStatus::HMDUnknown;
}

void UVarjoHMDFunctionLibrary::SetHeadtrackingEnabled(bool Enabled)
{
	FVarjoHMD* VarjoHMD = GetVarjoHMD();
	if (VarjoHMD)
	{
		VarjoHMD->SetHeadtrackingEnabled(Enabled);
	}
}

