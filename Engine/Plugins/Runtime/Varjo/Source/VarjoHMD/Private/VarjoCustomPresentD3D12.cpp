// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoCustomPresentD3D12.h"
#include "D3D12RHIPrivate.h"
#include "XRThreadUtils.h"

VarjoCustomPresentD3D12::VarjoCustomPresentD3D12(class FVarjoHMD* varjoHMD) :
	VarjoCustomPresent(varjoHMD)
{
}

VarjoCustomPresentD3D12::~VarjoCustomPresentD3D12()
{
}

void VarjoCustomPresentD3D12::varjoInit()
{
	ExecuteOnRHIThread([this]() {

		typedef HRESULT(STDAPICALLTYPE *D3D11On12CreateDevice)(
			IUnknown* pDevice,
			UINT Flags,
			CONST D3D_FEATURE_LEVEL* pFeatureLevels,
			UINT FeatureLevels,
			IUnknown* CONST* ppCommandQueues,
			UINT NumQueues,
			UINT NodeMask,
			ID3D11Device** ppDevice,
			ID3D11DeviceContext** ppImmediateContext,
			D3D_FEATURE_LEVEL* pChosenFeatureLevel);

		void* d3d11DllHandle = FWindowsPlatformProcess::GetDllHandle(TEXT("d3d11.dll"));
		if (d3d11DllHandle == nullptr)
		{
			UE_LOG(LogHMD, Log, TEXT("d3d11.dll could not found."));
			return;
		}
		D3D11On12CreateDevice D3D11On12CreateDeviceFunc = (D3D11On12CreateDevice)FPlatformProcess::GetDllExport(d3d11DllHandle, TEXT("D3D11On12CreateDevice"));

		if (D3D11On12CreateDeviceFunc == nullptr)
		{
			UE_LOG(LogHMD, Log, TEXT("D3D11On12CreateDeviceFunc could not found."));
			return;
		}

		static bool initDone = false;
		ID3D12Device* d3d12Device = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());

		FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
		ID3D12CommandQueue* commandQueue = DynamicRHI->RHIGetD3DCommandQueue();
		UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		HRESULT hr = D3D11On12CreateDeviceFunc(
			d3d12Device,
			d3d11DeviceFlags,
			nullptr,
			0,
			reinterpret_cast<IUnknown**>(&commandQueue),
			1,
			0,
			&m_device,
			&m_deviceContext,
			nullptr
		);
		check(SUCCEEDED(hr));

		hr = m_device->QueryInterface(__uuidof(ID3D11On12Device), (void**)&m_d3d11On12Device);
		check(SUCCEEDED(hr));
		m_graphicsInfo = varjo_D3D11Init(m_session, m_device, varjo_TextureFormat_B8G8R8A8_SRGB, nullptr);
		m_frameInfo = varjo_CreateFrameInfo(m_session);
		m_submitInfo = varjo_CreateSubmitInfo(m_session);
		varjo_LayoutDefaultViewports(m_session, m_submitInfo->viewports);
	});
}

void VarjoCustomPresentD3D12::varjoSubmit()
{
	// Check that all OK
	varjo_Error error = varjo_GetError(m_session);
	if (error != varjo_NoError)
	{
		UE_LOG(LogHMD, Log, TEXT("Error existed before varjoSubmit. Skip submit. Error code: %d."), error);
		return;
	}

	varjo_Texture varjoTexture = varjo_FromD3D11Texture(m_texture);
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
}

void VarjoCustomPresentD3D12::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	ExecuteOnRenderThread([&]() {
		ExecuteOnRHIThread([&]() {

			ID3D12Resource* d3d12Texture = reinterpret_cast<ID3D12Resource*>(RT->GetNativeResource());

			if (d3d12Texture == m_d3d12Texture)
			{
				return;
			}
			m_d3d12Texture = d3d12Texture;

			if (m_texture != nullptr)
			{
				m_texture->Release();
			}
			D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE };
			m_d3d11On12Device->CreateWrappedResource(
				m_d3d12Texture,
				&d3d11Flags,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				IID_PPV_ARGS(&m_texture)
			);
		});
	});

	InViewportRHI->SetCustomPresent(this);
}

bool VarjoCustomPresentD3D12::CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture,
	FTexture2DRHIRef& OutShaderResourceTexture)
{
	FRHIResourceCreateInfo CreateInfo;
	RHICreateTargetableShaderResource2D(4096, 3200, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, OutTargetableTexture, OutShaderResourceTexture);
	return true;
}
