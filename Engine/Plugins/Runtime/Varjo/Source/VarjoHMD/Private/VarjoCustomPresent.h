// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "HeadMountedDisplayBase.h"
#include "XRRenderBridge.h"

// Varjo API
#include "Varjo.h"
#include "Varjo_layers.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
__pragma (warning(push)) __pragma(warning(disable: 4005)) /* macro redefinition */
// Varjo API
#include "Varjo_d3d11.h"

// D3D11 Specific
#include <d3dcommon.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")

__pragma (warning(pop))
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class VarjoCustomPresent : public FXRRenderBridge
{
public:
	VarjoCustomPresent(class FVarjoHMD* varjoHMD);
	virtual ~VarjoCustomPresent();

	void Init();
	virtual void varjoInit() = 0;
	virtual void varjoSubmit() = 0;
	bool isInitialized() const;
	void OnBackBufferResize() override;
	bool Present(int& InOutSyncInterval) override;
	virtual void BeginRendering();
	void WaitSync();
	virtual void FinishRendering(FRHICommandListImmediate& RHICmdList);
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) = 0;
	virtual void SetNeedReinitRendererAPI();
	virtual bool NeedsNativePresent() override;
	virtual void Reset();
	virtual void Shutdown();
	void PostPresent();
	virtual bool CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) = 0;
	virtual bool CreateDepthTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) = 0;
	virtual void AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture) = 0;
	void setResolutionFraction(float resolutionFraction) { m_resolutionFraction = resolutionFraction; };
	float getResolutionFraction() const { return m_resolutionFraction; };
	bool getButtonEvent(int& button, bool& pressed) const;
	void renderOcclusionMesh(FRHICommandList& RHICmdList, int viewIndex);
	void handleVarjoEvents(UGameViewportClient* gameViewportClient);
	void getFocusViewPosAndSize(EStereoscopicPass stereoPass, float& x, float& y, float& width, float& height) const;
	void SetDepthSubmissionEnabled(bool enabled) { m_submitDepth = enabled; };

protected:
	virtual FTextureRHIRef CreateTexture(ID3D11Texture2D* d3dTexture) const = 0;
	class FVarjoHMD* m_varjoHMD;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;
	varjo_Session* m_session;
	varjo_GraphicsInfo* m_graphicsInfo;
	varjo_SubmitInfo* m_submitInfo;
	varjo_SwapChain* m_swapChain;
	varjo_SwapChain* m_depthSwapChain;
	ID3D11Texture2D* m_texture;
	float m_resolutionFraction = 1.0f;
	bool m_inFrame = false;
	bool m_submitDepth = false;

	// Varjo API related
	varjo_FrameInfo* m_frameInfo;

private:
	void setupOcclusionMeshes();

	varjo_Event* m_event;
	varjo_Mesh2Df* m_varjoOcclusionMesh;
	FHMDViewMesh m_occlusionMeshes[4];
	bool m_buttonEventExists = false;
	varjo_EventButton m_buttonEvent;
	bool m_isForeground = true;

	// Normalized position and size of focus views, with respect to context views.
	float m_focusX[2]{ 0.0f };
	float m_focusY[2]{ 0.0f };
	float m_focusWidth[2]{ 1.0f };
	float m_focusHeight[2]{ 1.0f };
};
