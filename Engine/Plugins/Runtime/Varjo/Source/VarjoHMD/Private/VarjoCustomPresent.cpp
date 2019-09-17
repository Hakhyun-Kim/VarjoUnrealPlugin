// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoCustomPresent.h"
#include "VarjoHMD.h"
#include "Engine/TextureRenderTarget2D.h" // to include ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
#include "Engine/World.h"
#include "SceneView.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

VarjoCustomPresent::VarjoCustomPresent(FVarjoHMD* varjoHMD)
	: m_session(varjoHMD->m_session)
	, m_texture(nullptr)
	, m_graphicsInfo(nullptr)
	, m_frameInfo(nullptr)
	, m_submitInfo(nullptr)
	, m_event(nullptr)
	, m_varjoHMD(varjoHMD)
	, m_varjoOcclusionMesh(nullptr)
{
}

VarjoCustomPresent::~VarjoCustomPresent()
{
	Shutdown();
}

void VarjoCustomPresent::Init()
{
	if (m_session == nullptr)
	{
		return;
	}

	ExecuteOnRenderThread([this]() {
		varjoInit();
		setupOcclusionMeshes();
		m_event = varjo_AllocateEvent();
		});
}

void VarjoCustomPresent::handleVarjoEvents(UGameViewportClient* gameViewportClient)
{
	m_buttonEventExists = false;
	while (varjo_PollEvent(m_session, m_event))
	{
		switch (m_event->header.type)
		{
		case varjo_EventType_Button:
			//only gets event when application is showing, otherwise button events will be used internal System UI
			if (m_isForeground)
			{
				m_buttonEventExists = true;
				m_buttonEvent = m_event->data.button;
			}
			break;
		case varjo_EventType_Visibility:
			if (m_event->data.visibility.visible == varjo_False)
			{
				gameViewportClient->bDisableWorldRendering = true;
			}
			else if (m_event->data.visibility.visible == varjo_True)
			{
				gameViewportClient->bDisableWorldRendering = false;
			}
			m_varjoHMD->SetHMDVisibility(m_event->data.visibility.visible == varjo_True ? HMDVisiblityStatus::HMDVisible: HMDVisiblityStatus::HMDNotVisible);
			break;
		case varjo_EventType_Foreground:
			m_isForeground = m_event->data.foreground.isForeground == varjo_True;
			break;
		default:
			break;
		}
	}
}

bool VarjoCustomPresent::isInitialized() const
{
	return m_session != nullptr;
}

void VarjoCustomPresent::OnBackBufferResize()
{
}

bool VarjoCustomPresent::Present(int& InOutSyncInterval)
{
	check(IsInRenderingThread() || IsInRHIThread());
	if (isInitialized()) varjoSubmit();

	InOutSyncInterval = 0; // VSync off
	return true;
}

DECLARE_CYCLE_STAT(TEXT("Varjo WaitSync"), STAT_VarjoCustomPresent_WaitSync, STATGROUP_Varjo);

void VarjoCustomPresent::BeginRendering()
{
	if (m_inFrame)
	{
		int SyncInterval = 0;
		Present(SyncInterval);
	}
	WaitSync();
	varjo_BeginFrame(m_session, m_submitInfo);
	m_inFrame = true;
}

void VarjoCustomPresent::WaitSync()
{
	{
		SCOPE_CYCLE_COUNTER(STAT_VarjoCustomPresent_WaitSync);
		varjo_WaitSync(m_session, m_frameInfo);

		if (isInitialized())
		{
			varjo_Error error = varjo_GetError(m_session);
			UE_CLOG(error != varjo_NoError, LogHMD, Log, TEXT("%s"), TEXT("varjo_Sync failed."));
			UE_CLOG(error != varjo_NoError, LogHMD, Verbose, TEXT("%s"), ANSI_TO_TCHAR(varjo_GetErrorDesc(error)));
		}
	}

	// Set projections and pose
	FMatrix projections[4] = { FMatrix::Identity, FMatrix::Identity, FMatrix::Identity, FMatrix::Identity };

	if (isInitialized())
	{
		varjo_AlignedView alignedViews[4];
		for (uint32_t i = 0; i < 4; ++i)
		{
			double* projMat = m_frameInfo->views[i].projectionMatrix;
			projections[i] = FMatrix(
				FPlane(projMat[0], 0.0f, 0.0f, 0.0f),
				FPlane(0.0f, projMat[5], 0.0f, 0.0f),
				FPlane(-projMat[8], -projMat[9], 0.0f, -projMat[11]),
				FPlane(0.0f, 0.0f, GNearClippingPlane, 0.0f)
			);
			alignedViews[i] = varjo_GetAlignedView(projMat);
			if (i > 1)
			{
				varjo_AlignedView focusAV = alignedViews[i];
				varjo_AlignedView contextAV = alignedViews[i - 2];
				m_focusX[i - 2] = (contextAV.projectionLeft - focusAV.projectionLeft) / (contextAV.projectionLeft + contextAV.projectionRight);
				m_focusY[i - 2] = (contextAV.projectionTop - focusAV.projectionTop) / (contextAV.projectionBottom + contextAV.projectionTop);
				m_focusWidth[i - 2] = (focusAV.projectionLeft + focusAV.projectionRight) / (contextAV.projectionLeft + contextAV.projectionRight);
				m_focusHeight[i - 2] = (focusAV.projectionBottom + focusAV.projectionTop) / (contextAV.projectionBottom + contextAV.projectionTop);
			}
		}
	}
	else
	{
		projections[0] = FReversedZPerspectiveMatrix(0.663048f, 0.737469f, 1.0f, 1.0f, GNearClippingPlane, GNearClippingPlane);
	}

	double uninitializedView[16] = { 0 };
	const double* vm1 = isInitialized() ? m_frameInfo->views[0].viewMatrix : uninitializedView;
	const double* vm2 = isInitialized() ? m_frameInfo->views[1].viewMatrix : uninitializedView;
	double x = (vm1[12] + vm2[12]) * 0.5f;
	double y = (vm1[13] + vm2[13]) * 0.5f;
	double z = (vm1[14] + vm2[14]) * 0.5f;
	FVector location = FVector(z, -x, -y);

	FMatrix rotMat;
	for (uint32_t col = 0; col < 4; ++col)
	{
		for (uint32_t row = 0; row < 4; ++row)
		{
			rotMat.M[row][col] = vm1[col * 4 + row];
		}
	}
	FQuat tempRot = FQuat(rotMat);
	FQuat rotation = FQuat(-tempRot.Z, tempRot.X, tempRot.Y, -tempRot.W);

	m_varjoHMD->SetHMDPose(rotation, rotation.RotateVector(location));
	m_varjoHMD->SetProjections(projections);
}

void VarjoCustomPresent::setupOcclusionMeshes()
{
	for (int i = 0; i < 4; ++i)
	{
		m_varjoOcclusionMesh = varjo_CreateOcclusionMesh(m_session, i, varjo_WindingOrder_Clockwise);
		int vertexCount = m_varjoOcclusionMesh->vertexCount;
		if (vertexCount > 0)
		{
			TArray<FVector2D> positions;
			positions.SetNumUninitialized(vertexCount);
			for (int vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx)
			{
				positions[vertexIdx] = FVector2D((m_varjoOcclusionMesh->vertices[vertexIdx].x + 1.0f) * 0.5f,
					(m_varjoOcclusionMesh->vertices[vertexIdx].y + 1.0f) * 0.5f);
			}
			m_occlusionMeshes[i].BuildMesh(&positions[0], vertexCount, FHMDViewMesh::MT_HiddenArea);
		}
	}
}

void VarjoCustomPresent::renderOcclusionMesh(FRHICommandList& RHICmdList, int viewIndex)
{
	check(IsInRenderingThread());
	const FHMDViewMesh& Mesh = m_occlusionMeshes[viewIndex];
	if (!Mesh.IsValid()) return;
	RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
}

void VarjoCustomPresent::FinishRendering()
{
}

void VarjoCustomPresent::SetNeedReinitRendererAPI()
{
}

bool VarjoCustomPresent::NeedsNativePresent()
{
	return true;
}

void VarjoCustomPresent::Reset()
{
}

void VarjoCustomPresent::Shutdown()
{
	if (isInitialized() == false)
	{
		return;
	}

	ExecuteOnRenderThread([this]() {
		if (m_event)
		{
			varjo_FreeEvent(m_event);
			m_event = nullptr;
		}

		if (m_varjoOcclusionMesh)
		{
			varjo_FreeOcclusionMesh(m_varjoOcclusionMesh);
			m_varjoOcclusionMesh = nullptr;
		}

		if (m_frameInfo)
		{
			varjo_FreeFrameInfo(m_frameInfo);
			m_frameInfo = nullptr;
		}

		if (m_submitInfo)
		{
			varjo_FreeSubmitInfo(m_submitInfo);
			m_submitInfo = nullptr;
		}

		if (m_graphicsInfo != nullptr)
		{
			varjo_D3D11ShutDown(m_session);
			m_graphicsInfo = nullptr;
		}

		m_session = nullptr;
		});

	FlushRenderingCommands();
}

void VarjoCustomPresent::PostPresent()
{
}

bool VarjoCustomPresent::getButtonEvent(int& button, bool& pressed) const
{
	if (m_buttonEventExists)
	{
		button = m_buttonEvent.buttonId;
		pressed = (m_buttonEvent.pressed == varjo_True);
		return true;
	}

	return false;
}

TArray<ID3D11Texture2D*> GetSwapChainTextures(varjo_Session* session, varjo_GraphicsInfo* info) {
	TArray<ID3D11Texture2D*> d3dTextures;
	for (int32_t i = 0; i < info->swapChainTextureCount; i++)
	{
		ID3D11Texture2D* texture = varjo_ToD3D11Texture(info->swapChainTextures[i]);
		d3dTextures.Add(texture);
	}

	return d3dTextures;
}

TArray<FTextureRHIRef> VarjoCustomPresent::CreateTextures(const TArray<ID3D11Texture2D*>& d3dTextures) const
{
	TArray<FTextureRHIRef> rhiTextures;
	for (int i = 0; i < d3dTextures.Num(); i++)
	{
		const FTextureRHIRef rhiTexture = CreateTexture(d3dTextures[i]);
		rhiTextures.Add(rhiTexture);
	}
	return rhiTextures;
}

VarjoTextureSetPtr VarjoCustomPresent::CreateTextureSet()
{
	if (isInitialized() == false)
	{
		FlushRenderingCommands();
		if (isInitialized() == false)
		{
			UE_LOG(LogHMD, Fatal, TEXT("Varjo is not initialized."));
			return nullptr;
		}
	}
	TArray<ID3D11Texture2D*> d3dTextures = GetSwapChainTextures(m_session, m_graphicsInfo);
	const FTextureRHIRef rhiTexture = CreateTexture(d3dTextures[0]);
	const TArray<FTextureRHIRef> rhiTextures = CreateTextures(d3dTextures);
	return MakeShareable(new VarjoTextureSet(m_session, this, rhiTexture, rhiTextures));
}

void VarjoCustomPresent::getFocusViewPosAndSize(EStereoscopicPass stereoPass, float& x, float& y, float& width, float& height) const
{
	switch (static_cast<int>(stereoPass))
	{
		case eSSP_LEFT_FOCUS:
			x = m_focusX[0];
			y = m_focusY[0];
			width = m_focusWidth[0];
			height = m_focusHeight[0];
			break;
		case eSSP_RIGHT_FOCUS:
			x = m_focusX[1];
			y = m_focusY[1];
			width = m_focusWidth[1];
			height = m_focusHeight[1];
			break;
		default:
			x = 0.0f;
			y = 0.0f;
			width = 1.0f;
			height = 1.0f;
			break;
	}
}
