// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

static const bool USE_OCCLUSION_MESH = true;

#define eSSP_LEFT_FOCUS eSSP_LEFT_EYE_SIDE
#define eSSP_RIGHT_FOCUS eSSP_RIGHT_EYE_SIDE

/** Name of the current OpenVR SDK version in use (matches directory name) */
#define OPENVR_SDK_VER TEXT("OpenVRv1_5_17")

#include "VarjoCustomPresentD3D11.h"
#include "VarjoCustomPresentD3D12.h"
#include "HeadMountedDisplayBase.h"
#include "IVarjoHMDPlugin.h"
#include "XRRenderTargetManager.h"
#include "IMotionController.h"
#include "SceneViewExtension.h"
#include "openvr.h"
#include "VarjoHMD_Types.h"
#include "XRThreadUtils.h"

typedef void*(VR_CALLTYPE *pVRGetGenericInterface)(const char* pchInterfaceVersion, vr::HmdError* peError);
DECLARE_STATS_GROUP(TEXT("Varjo"), STATGROUP_Varjo, STATCAT_Advanced);

class FVarjoHMD
	: public FHeadMountedDisplayBase
	, public FXRRenderTargetManager
	, public FSceneViewExtensionBase
{
public:
	FVarjoHMD(const FAutoRegister& AutoRegister, IVarjoHMDPlugin* plugin);
	virtual ~FVarjoHMD();
	// IModuleInterface implementation 
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// FXRTrackingSystemBase
	virtual FName GetSystemName() const override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType = EXRTrackedDeviceType::Any) override;
	virtual bool GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties) override;
	virtual bool IsTracking(int32 DeviceId) override;
	virtual bool DoesSupportPositionalTracking() const override { return true; };
	virtual bool HasValidTrackingPosition() override { return m_trackingFrame.bHaveVisionTracking; };

	virtual void ResetOrientationAndPosition(float yaw) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;
	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;
	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;
	virtual float GetWorldToMetersScale() const override;
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual bool GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	void SetProjections(const FMatrix(&CurrentProjections)[4]);

	// IHeadMountedDisplay 
	virtual IHeadMountedDisplay* GetHMDDevice() override { return this; }
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override { return SharedThis(this); }
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual bool IsHMDConnected() override;
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool enable) override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual EStereoscopicPass GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const override;
#ifdef VARJO_USE_CUSTOM_ENGINE
	virtual void GetFocusViewPosAndSize(EStereoscopicPass stereoPass, float& x, float& y, float& width, float& height) const override;
#endif
	virtual uint32 GetViewIndexForPass(EStereoscopicPass StereoPassType) const override;
	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override;
	EHMDWornState::Type GetHMDWornState() { return EHMDWornState::Worn; }
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;
	virtual bool HasHiddenAreaMesh() const override { return USE_OCCLUSION_MESH; }
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool GetFloorToEyeTrackingTransform(FTransform & OutStandingToSeatedTransform) const override;

	// FHeadMountedDisplayBase (public IStereoRendering related)
	virtual bool IsStereoEnabled() const override;
	bool OnStereoTeardown();
	bool OnStereoStartup();
	virtual bool EnableStereo(bool bStereo) override;
	virtual bool IsSpectatorScreenActive() const override { return true; }
	virtual void OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override { if (bStereoRequested) return 4; else return 1; }
	virtual class TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > GetXRCamera(int32 DeviceId) override;
#ifdef VARJO_USE_CUSTOM_ENGINE
	virtual bool IsISRPrimaryView(EStereoscopicPass Pass) override { return Pass == eSSP_LEFT_EYE || Pass == eSSP_LEFT_FOCUS; }
#endif

	// FXRRenderTargetManager
	virtual bool ShouldUseSeparateRenderTarget() const override { return m_stereoEnabled; }
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
	virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }
	virtual FXRRenderBridge* GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget) override;

	// ISceneViewExtension
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {};

	void SetHeadtrackingEnabled(bool enabled);

	// VarjoHMDFunctionLibrary
	VARJOHMD_API bool GetButtonEvent(int& button, bool& pressed) const;
	VARJOHMD_API bool IsDeviceConnected(int32 DeviceId) const;
	VARJOHMD_API float GetDeviceBatteryLevel(int32 DeviceId) const;

	VARJOHMD_API ETrackingStatus GetControllerTrackingStatus(int32 DeviceId) const;
	bool UpdateHMDPose();
	bool SetHMDPose(FQuat rotation, FVector location);
	EXRTrackedDeviceType GetTrackedDeviceType(int32 DeviceId) const;

	vr::IVRSystem* GetVRSystem() const { return m_VRSystem; }
	void SetHMDVisibility(HMDVisiblityStatus status);
	HMDVisiblityStatus GetHMDVisibility();

	static pVRGetGenericInterface VRGetGenericInterfaceFn;

	static const FName VarjoSystemName;

	varjo_Session* m_session = nullptr;
	FQuat InvViewQuat;
	FVector InvViewOrigin;
	
private:
	static FORCEINLINE FMatrix ToFMatrix(const vr::HmdMatrix34_t& tm) {
		// Rows and columns are swapped between vr::HmdMatrix34_t and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], 0.0f),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], 0.0f),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], 0.0f),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], 1.0f));
	}

	FVarjoHMD();
	bool Startup();
	void Shutdown();
	void PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, bool InFlip, FQuat& OutOrientation, FVector& OutPosition) const;

	float IPD();

	struct FTrackingFrame
	{
		bool bDeviceIsConnected[vr::k_unMaxTrackedDeviceCount];
		bool bPoseIsValid[vr::k_unMaxTrackedDeviceCount];
		FVector DevicePosition[vr::k_unMaxTrackedDeviceCount];
		FQuat DeviceOrientation[vr::k_unMaxTrackedDeviceCount];
		float DeviceBatteryLevel[vr::k_unMaxTrackedDeviceCount];
		EXRTrackedDeviceType deviceType[vr::k_unMaxTrackedDeviceCount];
		bool bHaveVisionTracking;

		vr::HmdMatrix34_t RawPoses[vr::k_unMaxTrackedDeviceCount];

		FTrackingFrame()
			: bHaveVisionTracking(false)
		{
			const uint32 MaxDevices = vr::k_unMaxTrackedDeviceCount;

			FMemory::Memzero(bDeviceIsConnected, MaxDevices * sizeof(bool));
			FMemory::Memzero(bPoseIsValid, MaxDevices * sizeof(bool));
			FMemory::Memzero(DevicePosition, MaxDevices * sizeof(FVector));


			for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
			{
				DeviceOrientation[i] = FQuat::Identity;
			}

			FMemory::Memzero(RawPoses, MaxDevices * sizeof(vr::HmdMatrix34_t));
		}
	};
	void UpdatePoses();

	FTrackingFrame m_trackingFrame;

	IRendererModule* m_rendererModule;
	IVarjoHMDPlugin* m_varjoHMDPlugin;
	TRefCountPtr<VarjoCustomPresent> m_bridge;
	bool m_stereoEnabled;
	FVector m_currentLocation;
	FQuat m_currentOrientation;

	FVector m_currentLocation_rt;
	FQuat m_currentOrientation_rt;

	FMatrix m_currentProjections[4];
	vr::IVRSystem* m_VRSystem;
	FVector m_baseOffset = FVector::ZeroVector;
	FQuat m_baseOrientation = FQuat::Identity;
	float m_worldToMetersScale = 100.0f;
	TSharedPtr<class FVarjoDynamicResolutionState> m_dynamicResolutionState;

	class VarjoGaze* m_gaze;
	HMDVisiblityStatus m_HMDVisiblityStatus;
};
