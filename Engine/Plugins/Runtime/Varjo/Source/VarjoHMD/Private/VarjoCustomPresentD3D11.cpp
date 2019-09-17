// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoCustomPresentD3D11.h"
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
	m_graphicsInfo = varjo_D3D11Init(m_session, m_device, varjo_TextureFormat_B8G8R8A8_SRGB, nullptr);
	m_frameInfo = varjo_CreateFrameInfo(m_session);
	m_submitInfo = varjo_CreateSubmitInfo(m_session);
	m_submitInfo->flags = varjo_SubmitFlag_Async;
	varjo_LayoutDefaultViewports(m_session, m_submitInfo->viewports);
}

void VarjoCustomPresentD3D11::varjoSubmit()
{
	// Check that all OK
	varjo_Error error = varjo_GetError(m_session);
	if (error != varjo_NoError)
	{
		UE_LOG(LogHMD, Log, TEXT("Error existed before varjoSubmit. Skip submit. Error code: %d."), error);
		return;
	}

	varjo_Texture varjoTexture = m_graphicsInfo->swapChainTextures[varjo_GetSwapChainCurrentIndex(m_session)];
	int viewCount = m_graphicsInfo->viewCount;
	for (int32_t i = 0; i < viewCount; i++)
	{
		m_submitInfo->textures[i] = varjoTexture;

		m_submitInfo->viewports[i].x *= m_resolutionFraction;
		m_submitInfo->viewports[i].y *= m_resolutionFraction;
		m_submitInfo->viewports[i].width *= m_resolutionFraction;
		m_submitInfo->viewports[i].height *= m_resolutionFraction;
	}

	if (m_inFrame)
	{
		varjo_EndFrame(m_session, m_frameInfo, m_submitInfo);
		m_inFrame = false;
	}
	
	error = varjo_GetError(m_session);
	if (error != varjo_NoError)
	{
		UE_LOG(LogHMD, Log, TEXT("varjoSubmit failed, error code: %d."), error);
		return;
	}

	if (m_textureSet.IsValid())
	{
		m_textureSet->UpdateSwapChainIndex(this);
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

void VarjoCustomPresentD3D11::AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture)
{
	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}

bool VarjoCustomPresentD3D11::CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture,
	FTexture2DRHIRef& OutShaderResourceTexture)
{
	if (!m_textureSet.IsValid())
	{
		UE_LOG(LogHMD, Log, TEXT("Creating texture set..."));
		m_textureSet = CreateTextureSet();
	}

	OutTargetableTexture = OutShaderResourceTexture = m_textureSet->GetTexture2D();
	return true;
}
