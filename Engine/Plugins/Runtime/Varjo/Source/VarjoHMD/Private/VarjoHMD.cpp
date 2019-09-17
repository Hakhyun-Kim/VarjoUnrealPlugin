// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoHMD.h"
#include "VarjoDynamicResolution.h"
#include "Engine/GameEngine.h"
#include "Runtime/Engine/Public/UnrealEngine.h"
#include "HardwareInfo.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "CommonRenderResources.h"
#include "VarjoXRCamera.h"

DEFINE_LOG_CATEGORY(LogVarjoHMD);

const FName FVarjoHMD::VarjoSystemName(TEXT("VarjoHMD"));

FVarjoHMD::FVarjoHMD(const FAutoRegister& AutoRegister, IVarjoHMDPlugin* plugin)
	: FHeadMountedDisplayBase(nullptr)
	, FSceneViewExtensionBase(AutoRegister)
	, m_rendererModule(nullptr)
	, m_varjoHMDPlugin(plugin)
	, m_bridge(nullptr)
	, m_stereoEnabled(false)
	, m_currentLocation(FVector::ZeroVector)
	, m_currentOrientation(FQuat::Identity)
	, m_currentLocation_rt(FVector::ZeroVector)
	, m_currentOrientation_rt(FQuat::Identity)
	, m_VRSystem(nullptr)
	, m_HMDVisiblityStatus(HMDVisiblityStatus::HMDUnknown)
{
	for (unsigned int i = 0; i < 4; i++)
	{
		m_currentProjections[i] = FMatrix::Identity;
	}

	Startup();

	static const FName RendererModuleName("Renderer");
	m_rendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
}

FVarjoHMD::~FVarjoHMD()
{
	Shutdown();
}

FName FVarjoHMD::GetSystemName() const
{
	const FName VarjoHMDSystemName(TEXT("VarjoHMD"));
	return VarjoHMDSystemName;
}

EXRTrackedDeviceType FVarjoHMD::GetTrackedDeviceType(int32 DeviceId) const
{
	check(!DeviceId || m_VRSystem != nullptr);
	vr::TrackedDeviceClass DeviceClass = m_VRSystem->GetTrackedDeviceClass(DeviceId);

	char* modelName = new char[vr::k_unMaxPropertyStringSize];
	m_VRSystem->GetStringTrackedDeviceProperty(DeviceId, vr::Prop_RenderModelName_String, modelName, vr::k_unMaxPropertyStringSize);
	if (std::string(modelName).find("tracker") != std::string::npos)
	{
		return EXRTrackedDeviceType::Other; // To tell the difference between controllers and trackers
	}
	delete[] modelName;

	switch (DeviceClass)
	{
	case vr::TrackedDeviceClass_HMD:
		return EXRTrackedDeviceType::HeadMountedDisplay;
	case vr::TrackedDeviceClass_Controller:
		return EXRTrackedDeviceType::Controller;
	case vr::TrackedDeviceClass_TrackingReference:
		return EXRTrackedDeviceType::TrackingReference;
	case vr::TrackedDeviceClass_GenericTracker:
		return EXRTrackedDeviceType::Other;
	default:
		return EXRTrackedDeviceType::Invalid;
	}
}

bool FVarjoHMD::IsDeviceConnected(int32 DeviceId) const
{
	if (DeviceId == INDEX_NONE)
	{
		return false;
	}
	return m_trackingFrame.bDeviceIsConnected[DeviceId];
}

float FVarjoHMD::GetDeviceBatteryLevel(int32 DeviceId) const
{
	if (DeviceId == INDEX_NONE)
	{
		return -1.0f;
	}
	return m_trackingFrame.DeviceBatteryLevel[DeviceId];
}

bool FVarjoHMD::EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType)
{
	TrackedIds.Empty();
	if (m_VRSystem == nullptr)
	{
		return false;
	}

	if (DeviceType == EXRTrackedDeviceType::Any || DeviceType == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		TrackedIds.Add(IXRTrackingSystem::HMDDeviceId);
	}

	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		// Add only devices with a currently valid tracked pose
		if (m_trackingFrame.bPoseIsValid[i] && m_trackingFrame.bDeviceIsConnected[i] &&
			(DeviceType == EXRTrackedDeviceType::Any || m_trackingFrame.deviceType[i] == DeviceType))
		{
			TrackedIds.Add(i);
		}
	}
	return TrackedIds.Num() > 0;
}

bool FVarjoHMD::GetTrackingSensorProperties(int32 SensorId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties)
{
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FQuat::Identity;
	OutSensorProperties = FXRSensorProperties();

	if (SensorId > 0 && m_VRSystem == nullptr)
	{
		return false;
	}

	uint32 VarjoDeviceID = static_cast<uint32>(SensorId);
	if (VarjoDeviceID >= vr::k_unMaxTrackedDeviceCount)
	{
		return false;
	}

	if (SensorId == 0)
	{
		OutOrigin = m_currentLocation;
		OutOrientation = m_currentOrientation;
	}
	else
	{
		if (VarjoDeviceID >= vr::k_unMaxTrackedDeviceCount)
		{
			return false;
		}

		if (!m_trackingFrame.bPoseIsValid[VarjoDeviceID])
		{
			return false;
		}

		OutOrigin = m_trackingFrame.DevicePosition[VarjoDeviceID];
		OutOrientation = m_trackingFrame.DeviceOrientation[VarjoDeviceID];
	}

	OutSensorProperties.LeftFOV = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_FieldOfViewLeftDegrees_Float);
	OutSensorProperties.RightFOV = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_FieldOfViewRightDegrees_Float);
	OutSensorProperties.TopFOV = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_FieldOfViewTopDegrees_Float);
	OutSensorProperties.BottomFOV = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_FieldOfViewBottomDegrees_Float);

	OutSensorProperties.NearPlane = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_TrackingRangeMinimumMeters_Float) * m_worldToMetersScale;
	OutSensorProperties.FarPlane = m_VRSystem->GetFloatTrackedDeviceProperty(VarjoDeviceID, vr::Prop_TrackingRangeMaximumMeters_Float) * m_worldToMetersScale;

	OutSensorProperties.CameraDistance = FVector::Dist(FVector::ZeroVector, OutOrigin);
	return true;
}

void FVarjoHMD::UpdatePoses()
{
	check(IsInGameThread());

	m_worldToMetersScale = GetWorldToMetersScale();

	if (m_VRSystem == nullptr)
	{
		return;
	}

	vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];

	m_VRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, Poses, ARRAYSIZE(Poses));

	m_trackingFrame.bHaveVisionTracking = false;

	ExecuteOnRenderThread([this]() {
		m_currentOrientation = m_currentOrientation_rt;
		m_currentLocation = m_currentLocation_rt;
		}
	);

	// Controllers
	for (uint32 i = 1; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		m_trackingFrame.bHaveVisionTracking |= Poses[i].eTrackingResult == vr::ETrackingResult::TrackingResult_Running_OK;

		m_trackingFrame.bDeviceIsConnected[i] = Poses[i].bDeviceIsConnected;
		m_trackingFrame.DeviceBatteryLevel[i] = Poses[i].bDeviceIsConnected ? m_VRSystem->GetFloatTrackedDeviceProperty(i, vr::Prop_DeviceBatteryPercentage_Float) : 0.0f;
		m_trackingFrame.bPoseIsValid[i] = Poses[i].bPoseIsValid;
		m_trackingFrame.RawPoses[i] = Poses[i].mDeviceToAbsoluteTracking;
		m_trackingFrame.deviceType[i] = GetTrackedDeviceType(i);

		bool flip = ((m_trackingFrame.deviceType[i] == EXRTrackedDeviceType::Other) && (m_VRSystem->GetTrackedDeviceClass(i) == vr::TrackedDeviceClass_GenericTracker));
		PoseToOrientationAndPosition(m_trackingFrame.RawPoses[i], flip, m_trackingFrame.DeviceOrientation[i], m_trackingFrame.DevicePosition[i]);
	}
}

void FVarjoHMD::SetHMDVisibility(HMDVisiblityStatus status)
{
	m_HMDVisiblityStatus = status;
}

HMDVisiblityStatus FVarjoHMD::GetHMDVisibility()
{
	return m_HMDVisiblityStatus;
}

bool FVarjoHMD::IsTracking(int32 DeviceId)
{
	if (!DeviceId)
	{
		return true;
	}
	uint32 deviceId = static_cast<uint32>(DeviceId);
	bool bHasTrackedPose = false;
	if (m_VRSystem != nullptr)
	{
		if (deviceId < vr::k_unMaxTrackedDeviceCount)
		{
			bHasTrackedPose = m_trackingFrame.bPoseIsValid[deviceId];
		}
	}
	return bHasTrackedPose;
}

void FVarjoHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FVarjoHMD::ResetOrientation(float Yaw)
{
	FRotator ViewRotation;
	ViewRotation = m_currentOrientation.Rotator();
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;

	if (Yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= Yaw;
		ViewRotation.Normalize();
	}

	m_baseOrientation = ViewRotation.Quaternion();
}
void FVarjoHMD::ResetPosition()
{
	m_baseOffset = m_currentLocation;
}

void FVarjoHMD::SetBaseRotation(const FRotator& BaseRot)
{
	m_baseOrientation = BaseRot.Quaternion();
}
FRotator FVarjoHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FVarjoHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	m_baseOrientation = BaseOrient;
}

FQuat FVarjoHMD::GetBaseOrientation() const
{
	return m_baseOrientation;
}

bool FVarjoHMD::GetFloorToEyeTrackingTransform(FTransform& OutStandingToSeatedTransform) const
{
	bool bSuccess = false;
	if (m_VRSystem && ensure(IsInGameThread()))
	{
		vr::TrackedDevicePose_t SeatedPoses[vr::k_unMaxTrackedDeviceCount];
		m_VRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseSeated, 0.0f, SeatedPoses, ARRAYSIZE(SeatedPoses));
		vr::TrackedDevicePose_t StandingPoses[vr::k_unMaxTrackedDeviceCount];
		m_VRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseOrigin::TrackingUniverseStanding, 0.0f, StandingPoses, ARRAYSIZE(StandingPoses));

		const vr::TrackedDevicePose_t& SeatedHmdPose = SeatedPoses[vr::k_unTrackedDeviceIndex_Hmd];
		const vr::TrackedDevicePose_t& StandingHmdPose = StandingPoses[vr::k_unTrackedDeviceIndex_Hmd];
		if (SeatedHmdPose.bPoseIsValid && StandingHmdPose.bPoseIsValid)
		{
			const float WorldToMeters = GetWorldToMetersScale();

			FVector SeatedHmdPosition = FVector::ZeroVector;
			FQuat SeatedHmdOrientation = FQuat::Identity;
			PoseToOrientationAndPosition(SeatedHmdPose.mDeviceToAbsoluteTracking, false, SeatedHmdOrientation, SeatedHmdPosition);

			FVector StandingHmdPosition = FVector::ZeroVector;
			FQuat StandingHmdOrientation = FQuat::Identity;
			PoseToOrientationAndPosition(StandingHmdPose.mDeviceToAbsoluteTracking, false, StandingHmdOrientation, StandingHmdPosition);

			const FVector SeatedHmdFwd = SeatedHmdOrientation.GetForwardVector();
			const FVector SeatedHmdRight = SeatedHmdOrientation.GetRightVector();
			const FQuat StandingToSeatedRot = FRotationMatrix::MakeFromXY(SeatedHmdFwd, SeatedHmdRight).ToQuat() * StandingHmdOrientation.Inverse();

			const FVector StandingToSeatedOffset = SeatedHmdPosition - StandingToSeatedRot.RotateVector(StandingHmdPosition);
			OutStandingToSeatedTransform = FTransform(StandingToSeatedRot, StandingToSeatedOffset);
			bSuccess = true;
		}
	}
	return bSuccess;
}


float FVarjoHMD::GetWorldToMetersScale() const
{
	if (GWorld != nullptr)
	{
#if WITH_EDITOR
		// WorldToMeters scaling when using PIE.
		if (GIsEditor)
		{
			for (const FWorldContext& context : GEngine->GetWorldContexts())
			{
				if (context.WorldType == EWorldType::PIE)
				{
					return context.World()->GetWorldSettings()->WorldToMeters;
				}
			}
		}
#endif // WITH_EDITOR
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default.
	return 100.0f;
}


bool FVarjoHMD::IsHMDConnected()
{
	return true;
}

bool FVarjoHMD::IsHMDEnabled() const
{
	return true;
}

void FVarjoHMD::EnableHMD(bool bEnable = true)
{
}

bool FVarjoHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = "Varjo";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = 0;
	MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = 4096;
	MonitorDesc.ResolutionY = 3200;
	return true;
}

void FVarjoHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FVarjoHMD::GetInterpupillaryDistance() const
{
	const float DEFAULT_IPD = 0.064f;
	float ipd = DEFAULT_IPD;
	if (varjo_HasProperty(m_session, varjo_PropertyKey_GazeIPDEstimate))
	{
		double tmp = varjo_GetPropertyDouble(m_session, varjo_PropertyKey_GazeIPDEstimate) / 1000.0; // Convert to Unreal's units.
		if (tmp > 0.050f && tmp < 0.085f)
		{
			ipd = tmp;
		}
	}
	return ipd;
}

bool FVarjoHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

void FVarjoHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = 2048;

	// Left and right are swapped here.
	if (StereoPass == eSSP_LEFT_EYE)
	{
		X = 0;
		Y = 0;
		SizeY = 2048;
	}
	else if (StereoPass == eSSP_RIGHT_EYE)
	{
		X = 2048;
		Y = 0;
		SizeY = 2048;
	}
	else if (StereoPass == eSSP_LEFT_FOCUS)
	{
		X = 0;
		Y = 2048;
		SizeY = 1152;
	}
	else if (StereoPass == eSSP_RIGHT_FOCUS)
	{
		X = 2048;
		Y = 2048;
		SizeY = 1152;
	}
}

FMatrix FVarjoHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	check(IsStereoEnabled());

	int32_t i = 0;
	if (StereoPassType == eSSP_LEFT_EYE)
	{
		i = 0;
	}
	else if (StereoPassType == eSSP_RIGHT_EYE)
	{
		i = 1;
	}
	else if (StereoPassType == eSSP_LEFT_FOCUS)
	{
		i = 2;
	}
	else if (StereoPassType == eSSP_RIGHT_FOCUS)
	{
		i = 3;
	}
	return m_currentProjections[i];

}

EStereoscopicPass FVarjoHMD::GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const
{
	if (!bStereoRequested)
	{
		return eSSP_FULL;
	}
	else if (ViewIndex == 0)
	{
		return eSSP_LEFT_EYE;
	}
	else if (ViewIndex == 1)
	{
		return eSSP_RIGHT_EYE;
	}
	else if (ViewIndex == 2)
	{
		return static_cast<EStereoscopicPass>(eSSP_LEFT_FOCUS);
	}
	else if (ViewIndex == 3)
	{
		return static_cast<EStereoscopicPass>(eSSP_RIGHT_FOCUS);
	}
	else
	{
		return eSSP_LEFT_EYE;
	}
}

#ifdef VARJO_USE_CUSTOM_ENGINE
void FVarjoHMD::GetFocusViewPosAndSize(EStereoscopicPass stereoPass, float& x, float& y, float& width, float& height) const
{
	m_bridge->getFocusViewPosAndSize(stereoPass, x, y, width, height);
}
#endif

uint32 FVarjoHMD::GetViewIndexForPass(EStereoscopicPass StereoPassType) const
{
	switch (static_cast<int>(StereoPassType))
	{
	case eSSP_LEFT_EYE:
	case eSSP_FULL:
		return 0;

	case eSSP_RIGHT_EYE:
		return 1;

	case eSSP_LEFT_FOCUS:
		return 2;

	case eSSP_RIGHT_FOCUS:
		return 3;

	default:
		check(0);
		return -1;
	}
}

bool FVarjoHMD::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
{
	OutOrientation = FQuat::Identity;
	OutPosition = FVector::ZeroVector;
	if (DeviceId == IXRTrackingSystem::HMDDeviceId)
	{
		OutPosition = FVector(0, ((Eye == eSSP_LEFT_EYE || Eye == eSSP_LEFT_FOCUS) ? -.5 : .5) * GetInterpupillaryDistance() * m_worldToMetersScale, 0);
		return true;
	}
	else
	{
		return false;
	}
}

bool FVarjoHMD::GetHMDDistortionEnabled(EShadingPath) const
{
	return false;
}

void FVarjoHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());
	check(m_bridge);
	
	FRHIRenderPassInfo RPInfo(BackBuffer, ERenderTargetActions::DontLoad_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("VarjoHMD_RenderTexture"));
	{
		const uint32 viewportWidth = BackBuffer->GetSizeX();
		const uint32 viewportHeight = BackBuffer->GetSizeY();
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		RHICmdList.SetViewport(0, 0, 0, viewportWidth, viewportHeight, 1.0f);

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		const auto featureLevel = GMaxRHIFeatureLevel;
		auto shaderMap = GetGlobalShaderMap(featureLevel);

		TShaderMapRef<FScreenVS> vertexShader(shaderMap);
		TShaderMapRef<FScreenPS> pixelShader(shaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*vertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*pixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		pixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

		float resolutionFraction = m_bridge->getResolutionFraction();

		m_rendererModule->DrawRectangle(
			RHICmdList,
			0, 0, // X, Y
			viewportWidth * 2.0f / resolutionFraction, viewportHeight * (25.0f / 16.0f) / resolutionFraction, // SizeX, SizeY
			0.0f, 0.0f, // U, V
			1.0f, 1.0f, // SizeU, SizeV
			FIntPoint(viewportWidth, viewportHeight), // TargetSize
			FIntPoint(1, 1), // TextureSize
			*vertexShader,
			EDRF_Default);
	}
	RHICmdList.EndRenderPass();
}

void FVarjoHMD::StartupModule()
{
}

void FVarjoHMD::ShutdownModule()
{
}

void FVarjoHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

ETrackingStatus FVarjoHMD::GetControllerTrackingStatus(int32 DeviceId) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	if (DeviceId < vr::k_unMaxTrackedDeviceCount && m_trackingFrame.bPoseIsValid[DeviceId] && m_trackingFrame.bDeviceIsConnected[DeviceId])
	{
		TrackingStatus = ETrackingStatus::Tracked;
	}

	return TrackingStatus;
}

DECLARE_CYCLE_STAT(TEXT("Varjo UpdateHMDPose"), STAT_FVarjoHMD_UpdateHMDPose, STATGROUP_Varjo);

bool FVarjoHMD::UpdateHMDPose()
{
	SCOPE_CYCLE_COUNTER(STAT_FVarjoHMD_UpdateHMDPose);
	if (m_bridge->isInitialized() == false)
	{
		return false;
	}

	varjo_Matrix matrix = varjo_FrameGetPose(m_session, varjo_PoseType_Center);

	double x = matrix.value[12];
	double y = matrix.value[13];
	double z = matrix.value[14];
	FVector location = FVector(-z, x, y);

	FMatrix centerEyeMat;
	for (uint32_t col = 0; col < 4; ++col)
	{
		for (uint32_t row = 0; row < 4; ++row)
		{
			centerEyeMat.M[row][col] = matrix.value[col * 4 + row];
		}
	}
	centerEyeMat = centerEyeMat.Inverse();
	FQuat tempRot = FQuat(centerEyeMat);

	SetHMDPose(FQuat(-tempRot.Z, tempRot.X, tempRot.Y, -tempRot.W), location);

	return true;
}

bool FVarjoHMD::SetHMDPose(FQuat rotation, FVector location)
{
	if (IsInGameThread())
	{
		m_currentOrientation = rotation;
		m_currentLocation = location * m_worldToMetersScale;
	}
	else
	{
		m_currentOrientation_rt = rotation;
		m_currentLocation_rt = location * m_worldToMetersScale;
	}
	return false;
}


bool FVarjoHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (!DeviceId)
	{
		FVarjoXRCamera* pVarjoXRCamera = static_cast<FVarjoXRCamera*>(FVarjoHMD::GetXRCamera(0).Get());
		if (pVarjoXRCamera->HeadtrackingEnabled && m_bridge->isInitialized())
		{
			if (IsInGameThread())
			{
				CurrentOrientation = m_baseOrientation.Inverse() * m_currentOrientation;
				CurrentPosition = m_baseOrientation.Inverse().RotateVector(m_currentLocation - m_baseOffset);
			}
			else
			{
				CurrentOrientation = m_baseOrientation.Inverse() * m_currentOrientation_rt;
				CurrentPosition = m_baseOrientation.Inverse().RotateVector(m_currentLocation_rt - m_baseOffset);
			}
		}
		else
		{
			CurrentOrientation = FQuat::Identity;
			CurrentPosition = FVector::ZeroVector;
			return false;
		}
	}
	else
	{
		uint32 deviceID = static_cast<uint32>(DeviceId);
		bool bHasValidPose = false;

		if (deviceID < vr::k_unMaxTrackedDeviceCount)
		{
			bHasValidPose = m_trackingFrame.bPoseIsValid[deviceID] && m_trackingFrame.bDeviceIsConnected[deviceID];

			if (bHasValidPose)
			{
				CurrentOrientation = m_trackingFrame.DeviceOrientation[deviceID];
				CurrentPosition = m_trackingFrame.DevicePosition[deviceID];
			}
			else
			{
				CurrentOrientation = FQuat::Identity;
				CurrentPosition = FVector::ZeroVector;
			}
		}
		else
		{
			CurrentOrientation = FQuat::Identity;
			CurrentPosition = FVector::ZeroVector;
		}

		return bHasValidPose;
	}
	return true;
}

DECLARE_CYCLE_STAT(TEXT("Varjo OnStartGameFrame"), STAT_FVarjoHMD_OnStartGameFrame, STATGROUP_Varjo);
bool FVarjoHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	SCOPE_CYCLE_COUNTER(STAT_FVarjoHMD_OnStartGameFrame);
	if (m_bridge != nullptr && WorldContext.GameViewport)
	{
		m_bridge->handleVarjoEvents(WorldContext.GameViewport);
	}
	UpdatePoses();
	return true;
}

void FVarjoHMD::PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, bool InFlip, FQuat& OutOrientation, FVector& OutPosition) const
{
	FMatrix Pose = ToFMatrix(InPose);

	// Workaround for the SteamVR bug: Trackers are normally classified as controllers except when they are idle and then their orientation flips 90 degrees
	if (InFlip) Pose = FMatrix(FPlane(-Pose.M[0][0], -Pose.M[0][1], -Pose.M[0][2], -Pose.M[0][3]),
							   FPlane(-Pose.M[2][0], -Pose.M[2][1], -Pose.M[2][2], -Pose.M[2][3]),
							   FPlane(-Pose.M[1][0], -Pose.M[1][1], -Pose.M[1][2], -Pose.M[1][3]),
							   FPlane( Pose.M[3][0],  Pose.M[3][1],  Pose.M[3][2],  Pose.M[3][3]));

	FQuat Orientation(Pose);

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;

	FVector Position = FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) * m_worldToMetersScale - m_baseOffset;
	OutPosition = m_baseOrientation.Inverse().RotateVector(Position);

	OutOrientation = m_baseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FVarjoHMD::SetProjections(const FMatrix(&CurrentProjections)[4])
{
	check(IsInRenderingThread());

	for (int i = 0; i < 4; ++i)
	{
		m_currentProjections[i] = CurrentProjections[i];
	}
}


void FVarjoHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	FHeadMountedDisplayBase::CalculateStereoViewOffset(StereoPassType, ViewRotation, WorldToMeters, ViewLocation);
}

//---------------------------------------------------------------------------
//
// IStereoRendering
//
//---------------------------------------------------------------------------

bool FVarjoHMD::IsStereoEnabled() const
{
 	return m_stereoEnabled;
}

bool FVarjoHMD::OnStereoTeardown()
{
	Shutdown();
	GEngine->ChangeDynamicResolutionStateAtNextFrame(FDynamicResolutionHeuristicProxy::CreateDefaultState());
	FlushRenderingCommands();
	return true;
}

bool FVarjoHMD::OnStereoStartup()
{
	if (!Startup())
	{
		return false;
	}
	
	int32 resX = 4096;
	int32 resY = 3200;
	MonitorInfo MonitorDesc;
	if (GetHMDMonitorInfo(MonitorDesc))
	{
		resX = MonitorDesc.ResolutionX;
		resY = MonitorDesc.ResolutionY;
	}
	FSystemResolution::RequestResolutionChange(resX, resY, EWindowMode::WindowedFullscreen);

	m_dynamicResolutionState = MakeShareable(new FVarjoDynamicResolutionState());
	GEngine->ChangeDynamicResolutionStateAtNextFrame(m_dynamicResolutionState);
	return true;
}

static void SetConsolveVariable(TCHAR * consoleVarName, int32 value)
{
	IConsoleVariable* consoleVar = IConsoleManager::Get().FindConsoleVariable(consoleVarName);
	if (consoleVar)
	{
		consoleVar->Set(value);
	}
}

bool FVarjoHMD::EnableStereo(bool bStereo)
{
	if (m_stereoEnabled == bStereo)
	{
		return true;
	}

	m_stereoEnabled = bStereo;

	// Enable FPS back to normal after Varjo System UI(low fps)
	GEngine->bForceDisableFrameRateSmoothing = bStereo;

	// By default unreal buffers 1 frame, which in some cases may break compositor (cause stuttering).
	SetConsolveVariable(TEXT("r.OneFrameThreadLag"), bStereo == false);
#if RHI_RAYTRACING
	SetConsolveVariable(TEXT("r.RayTracing.Shadows"), bStereo == false);
	SetConsolveVariable(TEXT("r.RayTracing.Reflections"), bStereo == false);
#ifndef VARJO_USE_CUSTOM_ENGINE
	SetConsolveVariable(TEXT("r.RayTracing.AmbientOcclusion"), bStereo ? 0:-1);
#endif

	//To Turn off all RT feature
	//SetConsolveVariable(TEXT("r.RayTracing.GlobalIllumination"), bStereo ? 0:-1);
	//SetConsolveVariable(TEXT("r.RayTracing.Translucency"), bStereo ? 0:-1);
#endif

//#ifndef VARJO_USE_CUSTOM_ENGINE
	static const auto InstancedStereoCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
	const bool bIsInstancedStereoEnabled = (InstancedStereoCVar && InstancedStereoCVar->GetValueOnAnyThread() != 0);

	if (bIsInstancedStereoEnabled)
	{
		UE_LOG(LogVarjoHMD, Error, TEXT("Varjo Plugin not supported with instanced stereo rendering enabled, please use Varjo Unreal Engine instead - https://varjo.com/use-center/developers/unreal/using-varjos-unreal-engine/"));
	}
//#endif

	if (bStereo)
	{
		return OnStereoStartup();
	}
	else
	{
		return OnStereoTeardown();
	}
}

bool FVarjoHMD::Startup()
{
	//already Startup
	if (m_session != nullptr)
	{
		return true;
	}

	vr::EVRInitError VRInitErr = vr::VRInitError_None;
	// Attempt to initialize the VRSystem device
	m_VRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Utility);
	if ((m_VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogVarjoHMD, Log, TEXT("Failed to initialize OpenVR with code %d"), (int32)VRInitErr);
	}

	// Make sure that the version of the HMD we're compiled against is correct.  This will fill out the proper vtable!
	m_VRSystem = (vr::IVRSystem*)(*FVarjoHMD::VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &VRInitErr);
	if ((m_VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogVarjoHMD, Log, TEXT("Failed to initialize OpenVR (version mismatch) with code %d"), (int32)VRInitErr);
	}
	
	FString RHIString;
	{
		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();
		FString RHILookup = NAME_RHI.ToString() + TEXT("=");
		if (!FParse::Value(*HardwareDetails, *RHILookup, RHIString))
		{
			UE_LOG(LogVarjoHMD, Warning, TEXT("%s is not currently supported by VarjoHMD plugin!"), *RHIString);
			return false;
		}
	}

	m_session = varjo_SessionInit();
	if (!m_session)
	{
		UE_LOG(LogVarjoHMD, Warning, TEXT("Varjo software not installed or running!"));
	}

	if (RHIString == TEXT("D3D11"))
	{
		m_bridge = new VarjoCustomPresentD3D11(this);
	}
	else if (RHIString == TEXT("D3D12"))
	{
		m_bridge = new VarjoCustomPresentD3D12(this);
	}
	m_bridge->Init();
	return true;
}

void FVarjoHMD::Shutdown()
{
	if (m_bridge != nullptr)
	{
		m_bridge->Shutdown();
	}
	if (m_VRSystem != nullptr)
	{
		vr::VR_Shutdown();
		m_VRSystem = nullptr;
	}
	if (m_session != nullptr)
	{
		varjo_SessionShutDown(m_session);
		m_session = nullptr;
	}
}


DECLARE_CYCLE_STAT(TEXT("Varjo OnBeginRendering"), STAT_FVarjoHMD_OnBeginRendering_RenderThread, STATGROUP_Varjo);
void FVarjoHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& /* unused */, FSceneViewFamily& ViewFamily)
{
	SCOPE_CYCLE_COUNTER(STAT_FVarjoHMD_OnBeginRendering_RenderThread);
	check(IsInRenderingThread());
	if (m_bridge == nullptr)
	{
		return;
	}

	m_bridge->BeginRendering();
	
	FMatrix invViewMatrix = ViewFamily.Views[0]->ViewMatrices.GetInvViewMatrix();
	FMatrix right = ViewFamily.Views[1]->ViewMatrices.GetInvViewMatrix();

	// Translate to the center of the eyes
	invViewMatrix.M[3][0] = (invViewMatrix.M[3][0] + right.M[3][0]) * 0.5f;
	invViewMatrix.M[3][1] = (invViewMatrix.M[3][1] + right.M[3][1]) * 0.5f;
	invViewMatrix.M[3][2] = (invViewMatrix.M[3][2] + right.M[3][2]) * 0.5f;

	invViewMatrix.RemoveScaling();

	InvViewQuat = invViewMatrix.ToQuat();
	InvViewOrigin = invViewMatrix.GetOrigin();

	if (m_dynamicResolutionState.IsValid())
	{
		m_bridge->setResolutionFraction(m_dynamicResolutionState->GetResolutionFraction());
	}
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FVarjoHMD::GetXRCamera(int32 DeviceId)
{
	if (!XRCamera.IsValid())
	{
		TSharedRef<FVarjoXRCamera, ESPMode::ThreadSafe> NewCamera = FSceneViewExtensions::NewExtension<FVarjoXRCamera>(*this, DeviceId);
		XRCamera = NewCamera;
	}

	return XRCamera;
}

//---------------------------------------------------------------------------
//
// FXRRenderTargetManager
//
//---------------------------------------------------------------------------

void FVarjoHMD::UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const FViewport & Viewport, FRHIViewport *const ViewportRHI)
{
	if(m_bridge != nullptr && m_bridge->isInitialized())
	{
		m_bridge->UpdateViewport(Viewport, ViewportRHI);
	}
}

FXRRenderBridge* FVarjoHMD::GetActiveRenderBridge_GameThread(bool /* bUseSeparateRenderTarget */)
{
	check(IsInGameThread());
	if (m_bridge && m_bridge->isInitialized())
	{
		return m_bridge;
	}
	else
	{
		return nullptr;
	}
}

void FVarjoHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	InOutSizeX = 4096;
	InOutSizeY = 3200;
}

bool FVarjoHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	const uint32 InSizeX = Viewport.GetSizeXY().X;
	const uint32 InSizeY = Viewport.GetSizeXY().Y;
	const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

	uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
	CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
	if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
	{
		return true;
	}

	return false;
}

bool FVarjoHMD::GetButtonEvent(int& button, bool& pressed) const
{
	return m_bridge ? m_bridge->getButtonEvent(button, pressed): false;
}

void FVarjoHMD::SetHeadtrackingEnabled(bool enabled)
{
	FVarjoXRCamera* pVarjoXRCamera = static_cast<FVarjoXRCamera*>(FVarjoHMD::GetXRCamera(0).Get());
	pVarjoXRCamera->HeadtrackingEnabled = enabled;
}

bool FVarjoHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	if (m_bridge && m_bridge->isInitialized())
	{
		return m_bridge->CreateRenderTargetTexture(OutTargetableTexture, OutShaderResourceTexture);
	}
	else
	{
		FRHIResourceCreateInfo CreateInfo;
		RHICreateTargetableShaderResource2D(4096, 3200, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, OutTargetableTexture, OutShaderResourceTexture);
		return true;
	}
}

void FVarjoHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());

	InViewFamily.EngineShowFlags.ScreenPercentage = true;
	
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(ANSI_TO_TCHAR("r.ForwardShading"));
	if (CVar->GetInt() == 1)
	{
		//Can't change UV in ForwardShading so far
		InViewFamily.EngineShowFlags.SetScreenSpaceAO(false);
	}

#ifndef VARJO_USE_CUSTOM_ENGINE
	InViewFamily.EngineShowFlags.Vignette = 0;
#endif
}

void FVarjoHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	int32_t viewIndex = 0;
	if (StereoPass == eSSP_LEFT_EYE)
	{
		viewIndex = 0;
	}
	else if (StereoPass == eSSP_RIGHT_EYE)
	{
		viewIndex = 1;
	}
	else if (StereoPass == eSSP_LEFT_FOCUS)
	{
		viewIndex = 2;
	}
	else if (StereoPass == eSSP_RIGHT_FOCUS)
	{
		viewIndex = 3;
	}
	else
	{
		return;
	}
	m_bridge ? m_bridge->renderOcclusionMesh(RHICmdList, viewIndex):0;
}
