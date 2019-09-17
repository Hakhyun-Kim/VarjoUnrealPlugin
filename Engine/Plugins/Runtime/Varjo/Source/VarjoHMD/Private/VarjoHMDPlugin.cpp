// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoHMDPlugin.h"

#include "VarjoHMD.h"

FVarjoHMDPlugin::FVarjoHMDPlugin()
	: m_varjoLibHandle(nullptr)
	, m_openvr_apiHandle(nullptr)
{
}

void FVarjoHMDPlugin::StartupModule()
{
	IHeadMountedDisplayModule::StartupModule();
}

void FVarjoHMDPlugin::ShutdownModule()
{
	IHeadMountedDisplayModule::ShutdownModule();

	if (m_varjoLibHandle)
	{
		FPlatformProcess::FreeDllHandle(m_varjoLibHandle);
		m_varjoLibHandle = nullptr;
	}
	if (m_openvr_apiHandle)
	{
		FPlatformProcess::FreeDllHandle(m_openvr_apiHandle);
		m_openvr_apiHandle = nullptr;
	}
}

bool FVarjoHMDPlugin::IsHMDConnected()
{
	// HMD connection detection not supported for versions prior to 1.4 
	if (varjo_GetVersion() < VARJO_MAKE_VERSION(1, 4, 0, 0))
		return true;

	auto session = varjo_SessionInit();

	if (!session)
		return false;

	bool isHMDConnected = false;

	varjo_SyncProperties(session);
	if (varjo_HasProperty(session, varjo_PropertyKey_HMDConnected))
		isHMDConnected = (varjo_GetPropertyBool(session, varjo_PropertyKey_HMDConnected) == varjo_True);

	varjo_SessionShutDown(session);
	return isHMDConnected;
}

uint64 FVarjoHMDPlugin::GetGraphicsAdapterLuid()
{
	return 0;
}

pVRGetGenericInterface FVarjoHMD::VRGetGenericInterfaceFn = nullptr;

bool FVarjoHMDPlugin::LoadDll(FString path)
{
	FPlatformProcess::PushDllDirectory(*path);
	m_varjoLibHandle = FPlatformProcess::GetDllHandle(*(path + TEXT("/VarjoLib.dll")));
	FPlatformProcess::PopDllDirectory(*path);
	return m_varjoLibHandle != nullptr;
}

bool FVarjoHMDPlugin::EnsureVarjoDllLoaded()
{
	if (m_varjoLibHandle)
	{
		return true;
	}
	
#ifdef VARJO_USE_CUSTOM_ENGINE
	FString enginePath = FPaths::EngineDir() / FString(TEXT("Plugins/Runtime/Varjo/ThirdParty/Win64"));
	FString userPath = FPaths::ProjectDir() / FString(TEXT("Plugins/Varjo/ThirdParty/Win64"));
	if (LoadDll(enginePath) == false)
	{
		LoadDll(userPath);
	}
#else
	FString userPath = FPaths::ProjectDir() / FString(TEXT("Plugins/Varjo/ThirdParty/Win64"));
	FString marketplacePath = FPaths::EngineDir() / FString(TEXT("Plugins/Marketplace/Varjo/ThirdParty/Win64"));
	if (LoadDll(userPath) == false) {
		LoadDll(marketplacePath);
	}
#endif
	ensureMsgf(m_varjoLibHandle, TEXT("Failed to load VarjoLib.dll"));
	return m_varjoLibHandle != nullptr;
}

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FVarjoHMDPlugin::CreateTrackingSystem()
{
	FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win64/"), OPENVR_SDK_VER);

	FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
	if (!m_openvr_apiHandle)
	{
		m_openvr_apiHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
	}
	FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
	ensureMsgf(m_openvr_apiHandle, TEXT("Failed to load openvr_api.dll"));

	FVarjoHMD::VRGetGenericInterfaceFn = (pVRGetGenericInterface)FPlatformProcess::GetDllExport(m_openvr_apiHandle, TEXT("VR_GetGenericInterface"));

	// Note:  If this fails to compile, it's because you merged a new OpenVR version, and didn't put in the module hacks marked with @epic in openvr.h
	vr::VR_SetGenericInterfaceCallback(FVarjoHMD::VRGetGenericInterfaceFn);

	// Verify that we've bound correctly to the DLL functions
	if (!FVarjoHMD::VRGetGenericInterfaceFn)
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to GetProcAddress() on openvr_api.dll"));
	}

	m_hmd = FSceneViewExtensions::NewExtension<FVarjoHMD>(this);
	return m_hmd;
}; 

FString FVarjoHMDPlugin::GetModuleKeyName() const
{
	return FString(TEXT("VarjoHMD"));
}

vr::IVRSystem* FVarjoHMDPlugin::GetVRSystem() const
{
	if (m_hmd.IsValid())
	{
		return m_hmd->GetVRSystem();
	}

	return nullptr;
}

bool FVarjoHMDPlugin::PreInit()
{
	if (EnsureVarjoDllLoaded() == false)
		return false;

	if (varjo_IsAvailable() != varjo_True)
		return false;

	return true;
}

IMPLEMENT_MODULE(FVarjoHMDPlugin, VarjoHMD)
