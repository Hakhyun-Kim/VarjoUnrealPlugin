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
	virtual void FinishRendering(FRHICommandListImmediate& RHICmdList) override;
	virtual void varjoSubmit() override;
	virtual FTextureRHIRef CreateTexture(ID3D11Texture2D* d3dTexture) const override;
	virtual void AliasTextureResources(FRHITexture* DestTexture, FRHITexture* SrcTexture) override;
	virtual bool CreateRenderTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) override;
	virtual bool CreateDepthTargetTexture(FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture) override;
	virtual void BeginRendering() override;

private:
	static const int32_t VIEW_COUNT = 4;

#ifdef VARJO_USE_CUSTOM_ENGINE
	FTextureRHIRef CreateDepthTexture(ID3D11Texture2D* d3dTexture) const;
#endif

	uint32_t m_textureCount = 0;
	FTexture2DRHIRef m_aliasTexture;
	TArray<FTexture2DRHIRef> m_textures;
	FTexture2DRHIRef m_depthTexture;
	TArray<FTexture2DRHIRef> m_depthTextures;
	varjo_Viewport m_viewports[VIEW_COUNT];
	bool m_depthSCAcquired = false;
};
