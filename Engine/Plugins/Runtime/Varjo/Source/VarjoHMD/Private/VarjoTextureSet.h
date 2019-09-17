// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "Core/Public/Templates/SharedPointer.h"
//#include "VarjoHMDPrivateRHI.h"

class VarjoCustomPresent;
struct varjo_D3D11Info;
struct varjo_Session;

class VarjoTextureSet : public TSharedFromThis<VarjoTextureSet, ESPMode::ThreadSafe>
{
public:
	VarjoTextureSet(varjo_Session* session,
		VarjoCustomPresent* present,
		const FTextureRHIRef& InRhiTexture,
		const TArray<FTextureRHIRef>& InRHITextureSwapChain);
	FRHITexture* GetTexture() const { return RHITexture.GetReference(); }
	FRHITexture2D* GetTexture2D() const { return RHITexture->GetTexture2D(); }
	void UpdateSwapChainIndex(VarjoCustomPresent *customPresent);
	int GetCurrentIndex() const { return m_currentIndex; };

private:
	FTextureRHIRef RHITexture;
	TArray<FTextureRHIRef> RHITextureSwapChain;
	int m_currentIndex = 0;
	varjo_Session *m_session;
};

typedef TSharedPtr<VarjoTextureSet, ESPMode::ThreadSafe> VarjoTextureSetPtr;
