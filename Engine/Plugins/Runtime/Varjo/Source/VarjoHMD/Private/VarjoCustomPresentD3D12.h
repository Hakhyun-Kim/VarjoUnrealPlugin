// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "VarjoCustomPresent.h"

#include <d3d12.h>
#include <d3d11on12.h>

class VarjoCustomPresentD3D12 : public VarjoCustomPresent
{
public:
	VarjoCustomPresentD3D12(class FVarjoHMD* varjoHMD);
	~VarjoCustomPresentD3D12() override;

	void varjoInit() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI);
	virtual void varjoSubmit() override;
	FTextureRHIRef CreateTexture(ID3D11Texture2D* d3dTexture) const override { return {}; };
	void AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture) override {};
	virtual bool CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) override;

private:
	ID3D11On12Device* m_d3d11On12Device;
	ID3D12Resource* m_d3d12Texture{nullptr};
};
