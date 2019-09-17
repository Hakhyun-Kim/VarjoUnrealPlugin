// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "Engine/GameEngine.h"
#include "Runtime/Engine/Public/UnrealEngine.h"
#include "IVarjoHMDPlugin.h"

class FVarjoHMDPlugin : public IVarjoHMDPlugin
{
public:
	FVarjoHMDPlugin();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool IsHMDConnected() override;
	virtual uint64 GetGraphicsAdapterLuid() override;
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem();
	virtual FString GetModuleKeyName() const override;
	virtual vr::IVRSystem* GetVRSystem() const override;
	virtual bool PreInit() override;

private:
	bool LoadDll(FString path);
	bool EnsureVarjoDllLoaded();
	TSharedPtr< class FVarjoHMD, ESPMode::ThreadSafe > m_hmd;
	void* m_varjoLibHandle;
	void* m_openvr_apiHandle;
};
