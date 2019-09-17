// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoCustomPresentD3D11.h"
#include "VarjoHMD.h"
#include "VarjoHMDPrivateRHI.h"

VarjoCustomPresentD3D11::VarjoCustomPresentD3D11(class FVarjoHMD* varjoHMD) :
	VarjoCustomPresent(varjoHMD)
{
}

VarjoCustomPresentD3D11::~VarjoCustomPresentD3D11()
{
}

void VarjoCustomPresentD3D11::varjoInit()
{
	// Get device used by Unreal
	m_device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
	m_device->GetImmediateContext(&m_deviceContext);

	// Init varjo d3d11
	varjo_SwapChainConfig defaultScc = varjo_GetDefaultSwapChainConfig(m_session);

	varjo_SwapChainConfig2 scConfig{ varjo_TextureFormat_B8G8R8A8_SRGB, defaultScc.numberOfTextures, defaultScc.textureWidth, defaultScc.textureHeight, 1 };
	m_swapChain = varjo_D3D11CreateSwapChain(m_session, m_device, &scConfig);

	varjo_SwapChainConfig2 depthScConfig{ varjo_DepthTextureFormat_D32_FLOAT, defaultScc.numberOfTextures, defaultScc.textureWidth, defaultScc.textureHeight, 1 };
	m_depthSwapChain = varjo_D3D11CreateSwapChain(m_session, m_device, &depthScConfig);

	m_frameInfo = varjo_CreateFrameInfo(m_session);

	m_textureCount = defaultScc.numberOfTextures;

	varjo_LayoutDefaultViewports(m_session, m_viewports);
}

void VarjoCustomPresentD3D11::BeginRendering()
{
	VarjoCustomPresent::BeginRendering();

	int32_t scIndex;
	varjo_AcquireSwapChainImage(m_swapChain, &scIndex);
	AliasTextureResources(m_aliasTexture, m_textures[scIndex]);
}

void VarjoCustomPresentD3D11::FinishRendering(FRHICommandListImmediate& RHICmdList)
{
	m_depthSCAcquired = false;
	if (m_submitDepth && m_depthTexture.IsValid())
	{
		int32_t scIndex = -1;
		varjo_AcquireSwapChainImage(m_depthSwapChain, &scIndex);
		m_depthSCAcquired = true;
		if (0 <= scIndex && scIndex < m_depthTextures.Num())
		{
			m_varjoHMD->CopyDepthTexture_RenderThread(RHICmdList, m_depthTextures[scIndex], m_depthTexture);
		}
	}
}

void VarjoCustomPresentD3D11::varjoSubmit()
{
	varjo_ReleaseSwapChainImage(m_swapChain);
	if (m_depthSCAcquired)
	{
		varjo_ReleaseSwapChainImage(m_depthSwapChain);
	}

	// Check that all OK
	varjo_Error error = varjo_GetError(m_session);
	if (error != varjo_NoError)
	{
		UE_LOG(LogHMD, Log, TEXT("Error existed before varjoSubmit. Skip submit. Error code: %d."), error);
		return;
	}

	if (m_inFrame)
	{
		varjo_LayerMultiProj layer;
		layer.header.type = varjo_LayerMultiProjType;
		layer.header.flag = varjo_LayerFlagNone;
		layer.space = varjo_SpaceLocal;
		layer.viewCount = VIEW_COUNT;
		varjo_LayerMultiProjView views[VIEW_COUNT]{};
		varjo_ViewExtensionDepth depthViews[VIEW_COUNT]{};
		for (int i = 0; i < VIEW_COUNT; i++)
		{
			memcpy(views[i].projection.value, m_frameInfo->views[i].projectionMatrix, 16 * sizeof(double));
			memcpy(views[i].view.value, m_frameInfo->views[i].viewMatrix, 16 * sizeof(double));
			views[i].viewport.swapChain = m_swapChain;
			views[i].viewport.x = m_viewports[i].x * m_resolutionFraction;
			views[i].viewport.y = m_viewports[i].y * m_resolutionFraction;
			views[i].viewport.width = m_viewports[i].width * m_resolutionFraction;
			views[i].viewport.height = m_viewports[i].height * m_resolutionFraction;
			views[i].viewport.arrayIndex = 0;
			views[i].extension = m_submitDepth ? (varjo_ViewExtension*)& depthViews[i] : nullptr;

			if (m_submitDepth)
			{
				depthViews[i].header.type = varjo_ViewExtensionDepthType;
				depthViews[i].header.next = nullptr;
				depthViews[i].minDepth = 0.0f;
				depthViews[i].maxDepth = 1.0f;
				depthViews[i].nearZ = std::numeric_limits<float>::infinity();
				depthViews[i].farZ = GNearClippingPlane / m_varjoHMD->GetWorldToMetersScale();
				depthViews[i].viewport.swapChain = m_depthSwapChain;
				depthViews[i].viewport.x = views[i].viewport.x;
				depthViews[i].viewport.y = views[i].viewport.y;
				depthViews[i].viewport.width = views[i].viewport.width;
				depthViews[i].viewport.height = views[i].viewport.height;
				depthViews[i].viewport.arrayIndex = 0;
			}
		}
		layer.views = &views[0];
		varjo_LayerHeader* layerPtrs[1]{ &layer.header };

		varjo_SubmitInfoLayers submitInfoLayers;
		submitInfoLayers.flags = varjo_SubmitFlag_Async;
		submitInfoLayers.frameNumber = m_frameInfo->frameNumber;
		submitInfoLayers.layerCount = 1;
		submitInfoLayers.layers = layerPtrs;

		varjo_EndFrameWithLayers(m_session, &submitInfoLayers);
		m_inFrame = false;
	}

	error = varjo_GetError(m_session);
	if (error != varjo_NoError)
	{
		UE_LOG(LogHMD, Log, TEXT("varjoSubmit failed, error code: %d."), error);
		return;
	}
}

void VarjoCustomPresentD3D11::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	if (m_texture != nullptr)
	{
		m_texture->Release();
	}
	m_texture = (ID3D11Texture2D*)RT->GetNativeResource();
	m_texture->AddRef();

	InViewportRHI->SetCustomPresent(this);
}

FTextureRHIRef VarjoCustomPresentD3D11::CreateTexture(ID3D11Texture2D* d3dTexture) const
{
	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	const uint32 TexCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
	return DynamicRHI->RHICreateTexture2DFromResource(PF_B8G8R8A8, TexCreateFlags, FClearValueBinding::Black, d3dTexture).GetReference();
}

#ifdef VARJO_USE_CUSTOM_ENGINE
FTextureRHIRef VarjoCustomPresentD3D11::CreateDepthTexture(ID3D11Texture2D* d3dTexture) const
{
	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	const uint32 TexCreateFlags = TexCreate_ShaderResource | TexCreate_DepthStencilTargetable;
	return DynamicRHI->RHICreateTexture2DFromResource(PF_Depth, TexCreateFlags, FClearValueBinding::Black, d3dTexture).GetReference();
}
#endif

void VarjoCustomPresentD3D11::AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture)
{
	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}

bool VarjoCustomPresentD3D11::CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture,
	FTexture2DRHIRef& OutShaderResourceTexture)
{
	for (uint32_t i = 0; i < m_textureCount; i++)
	{
		m_textures.Add(CreateTexture(varjo_ToD3D11Texture(varjo_GetSwapChainImage(m_swapChain, i)))->GetTexture2D());
	}
	OutTargetableTexture = OutShaderResourceTexture = m_aliasTexture = CreateTexture(varjo_ToD3D11Texture(varjo_GetSwapChainImage(m_swapChain, 0)))->GetTexture2D();
	return true;
}

bool VarjoCustomPresentD3D11::CreateDepthTargetTexture(FTexture2DRHIRef& OutTargetableTexture,
	FTexture2DRHIRef& OutShaderResourceTexture)
{
#ifdef VARJO_USE_CUSTOM_ENGINE
	if (m_depthTexture.IsValid())
	{
		return false;
	}

	for (uint32_t i = 0; i < m_textureCount; i++)
	{
		m_depthTextures.Add(CreateDepthTexture(varjo_ToD3D11Texture(varjo_GetSwapChainImage(m_depthSwapChain, i)))->GetTexture2D());
	}

	FRHIResourceCreateInfo CreateInfo;
	CreateInfo.ClearValueBinding = FClearValueBinding(0.0f);
	RHICreateTargetableShaderResource2D(4096, 3200, PF_DepthStencil, 1, TexCreate_None, TexCreate_DepthStencilTargetable, false, CreateInfo, OutTargetableTexture, OutShaderResourceTexture);
	m_depthTexture = OutTargetableTexture;
	return true;
#else
	return false;
#endif
}