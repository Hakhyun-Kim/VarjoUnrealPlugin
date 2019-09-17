// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "VarjoCustomPresent.h"

class VarjoCustomPresentD3D11 : public VarjoCustomPresent
{
public:
	VarjoCustomPresentD3D11(class FVarjoHMD* varjoHMD);
	~VarjoCustomPresentD3D11() override;

	void varjoInit() override;
	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI);
	virtual void varjoSubmit() override;
	FTextureRHIRef CreateTexture(ID3D11Texture2D* d3dTexture) const override;
	void AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture) override;
	virtual bool CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) override;
};
