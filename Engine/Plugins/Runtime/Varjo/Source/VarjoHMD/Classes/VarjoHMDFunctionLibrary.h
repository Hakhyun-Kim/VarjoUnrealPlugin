// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "IMotionController.h"
#include "VarjoHMD_Types.h"
#include "VarjoHMDFunctionLibrary.generated.h"

/** Defines the class of tracked devices in VarjoHMD */
UENUM(BlueprintType)
enum class EVarjoVRTrackedDeviceType : uint8
{
	/** Represents a Varjo VR Controller */
	Controller,

	/** Represents a static tracking reference device, such as a Lighthouse or tracking camera */
	TrackingReference,

	/** Misc. device types, for future expansion */
	Other,

	/** DeviceId is invalid */
	Invalid
};

/**
 * VarjoHMD Extensions Function Library
 */
UCLASS()
class VARJOHMD_API UVarjoHMDFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* Used for polling button events
	*
	* @param     button     (out) Which button caused the event
	* @param     pressed    (out) True if the button is pressed
	* @param     True if there was a button event
	*/
	UFUNCTION(BlueprintPure, Category = "VarjoHMD")
	static bool GetButtonEvent(int& button, bool& pressed);
	
	/**
	 * Returns an array of the currently tracked device IDs
	 *
	 * @param    DeviceType          Which class of device (e.g. controller, tracking devices) to get Device Ids for
	 * @param    OutTrackedDeviceIds (out) Array containing the ID of each device that's currently tracked
	 */
	UFUNCTION(BlueprintPure, Category="VarjoHMD")
	static void GetValidTrackedDeviceIds(EVarjoVRTrackedDeviceType DeviceType, TArray<int32>& OutTrackedDeviceIds);

	/**
	 * Gets the orientation and position (in device space) of the device with the specified ID
	 *
	 * @param    DeviceId        Id of the device to get tracking info for
	 * @param    OutPosition     (out) Current position of the device
	 * @param    OutOrientation  (out) Current orientation of the device
	 * @return   True if the specified device id had a valid tracking pose this frame, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "VarjoHMD")
	static bool GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation);

	/**
	 * Given a controller index and a hand, returns the position and orientation of the controller
	 *
	 * @param    ControllerIndex Index of the controller to get the tracked device ID for
	 * @param    Hand            Which hand's controller to get the position and orientation for
	 * @param    OutPosition     (out) Current position of the device
	 * @param    OutRotation     (out) Current rotation of the device
	 * @return   True if the specified controller index has a valid tracked device ID
	 */
	UFUNCTION(BlueprintPure, Category = "VarjoHMD", meta = (DeprecatedFunction, DeprecationMessage = "Use motion controller components instead"))
	static bool GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation);

	UFUNCTION(BlueprintPure, Category = "VarjoHMD")
	static HMDVisiblityStatus GetHMDVisility();

	UFUNCTION(BlueprintCallable, Category = "VarjoHMD")
	static void SetHeadtrackingEnabled(bool Enabled);

	/**
	 * Sets whether Unreal should send the depth buffer to the Varjo stack or not
	 * Warning: Works only with Varjo's custom UnrealEngine
	 * @param	Enabled		True if Unreal should send the depth buffer, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "VarjoHMD")
	static void SetDepthSubmissionEnabled(bool Enabled);
};
