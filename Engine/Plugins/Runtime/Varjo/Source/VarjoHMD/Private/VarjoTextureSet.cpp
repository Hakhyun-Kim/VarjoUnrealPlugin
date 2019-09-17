// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoTextureSet.h"
#include "Varjo.h"
#include "Varjo_types_d3d11.h"
#include "VarjoCustomPresent.h"

VarjoTextureSet::VarjoTextureSet(
	varjo_Session* session,
	VarjoCustomPresent* customPresent,
	const FTextureRHIRef& InRhiTexture,
	const TArray<FTextureRHIRef>& InRHITextureSwapChain)
	: RHITexture(InRhiTexture)
	, RHITextureSwapChain(InRHITextureSwapChain)
	, m_currentIndex(0)
	, m_session(session)
{
	customPresent->AliasTextureResources(RHITexture, RHITextureSwapChain[m_currentIndex]);
}

void VarjoTextureSet::UpdateSwapChainIndex(VarjoCustomPresent *customPresent)
{
	m_currentIndex = varjo_GetSwapChainCurrentIndex(m_session);
	if (m_currentIndex < RHITextureSwapChain.Num())
	{
		customPresent->AliasTextureResources(RHITexture, RHITextureSwapChain[m_currentIndex]);
	}
}
