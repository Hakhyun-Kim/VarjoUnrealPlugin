// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#include "VarjoDynamicResolution.h"
#include "DynamicResolutionProxy.h"
#include "DynamicResolutionState.h"
#include "ScreenRendering.h"

FVarjoDynamicResolutionDriver::FVarjoDynamicResolutionDriver(const FDynamicResolutionHeuristicProxy* InProxy, const FSceneViewFamily& InViewFamily)
	: Proxy(InProxy)
	, ViewFamily(InViewFamily)
{
	check(IsInGameThread());
}

float FVarjoDynamicResolutionDriver::GetPrimaryResolutionFractionUpperBound() const
{
	if (!ViewFamily.EngineShowFlags.ScreenPercentage)
	{
		return 1.0f;
	}

	return Proxy->GetResolutionFractionUpperBound();
}

ISceneViewFamilyScreenPercentage* FVarjoDynamicResolutionDriver::Fork_GameThread(const class FSceneViewFamily& ForkedViewFamily) const
{
	check(IsInGameThread());

	return new FVarjoDynamicResolutionDriver(Proxy, ForkedViewFamily);
}

void FVarjoDynamicResolutionDriver::ComputePrimaryResolutionFractions_RenderThread(TArray<FSceneViewScreenPercentageConfig>& OutViewScreenPercentageConfigs) const
{
	check(IsInRenderingThread());

	if (!ViewFamily.EngineShowFlags.ScreenPercentage)
	{
		return;
	}

	float GlobalResolutionFraction = Proxy->QueryCurentFrameResolutionFraction_RenderThread();

	for (int32 i = 0; i < OutViewScreenPercentageConfigs.Num(); i++)
	{
		OutViewScreenPercentageConfigs[i].PrimaryResolutionFraction = GlobalResolutionFraction;
	}
}

FVarjoDynamicResolutionStateProxy::FVarjoDynamicResolutionStateProxy()
{
	check(IsInGameThread());
	InFlightFrames.SetNum(4);
	CurrentFrameInFlightIndex = -1;
	bUseTimeQueriesThisFrame = false;
}

void FVarjoDynamicResolutionStateProxy::Reset()
{
	check(IsInRenderingThread());

	// Reset heuristic.
	Heuristic.Reset_RenderThread();

	// Set invalid heuristic's entry id on all inflight frames.
	for (auto& InFlightFrame : InFlightFrames)
	{
		InFlightFrame.HeuristicHistoryEntry = FDynamicResolutionHeuristicProxy::kInvalidEntryId;
	}

	RenderQueryPool = RHICreateRenderQueryPool(RQT_AbsoluteTime);
}

void FVarjoDynamicResolutionStateProxy::BeginFrame(FRHICommandList& RHICmdList, float PrevGameThreadTimeMs)
{
	check(IsInRenderingThread());

	// Query render thread time Ms.
	float PrevRenderThreadTimeMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);

	// If RHI does not support GPU busy time queries, fall back to what stat unit does.
	float PrevFrameGPUTimeMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

	uint64 HistoryEntryId = Heuristic.CreateNewPreviousFrameTimings_RenderThread(
		PrevGameThreadTimeMs, PrevRenderThreadTimeMs);

	Heuristic.CommitPreviousFrameGPUTimings_RenderThread(HistoryEntryId,
		/* TotalFrameGPUBusyTimeMs = */ PrevFrameGPUTimeMs,
		/* DynamicResolutionGPUBusyTimeMs = */ PrevFrameGPUTimeMs,
		/* bGPUTimingsHaveCPUBubbles = */ !GRHISupportsFrameCyclesBubblesRemoval);

	Heuristic.RefreshCurentFrameResolutionFraction_RenderThread();

	// Set a non insane value for internal checks to pass as if GRHISupportsGPUBusyTimeQueries == true.
	CurrentFrameInFlightIndex = 0;
}

void FVarjoDynamicResolutionStateProxy::ProcessEvent(FRHICommandList& RHICmdList, EDynamicResolutionStateEvent Event)
{
	check(IsInRenderingThread());

	if (bUseTimeQueriesThisFrame)
	{
		InFlightFrameQueries& InFlightFrame = InFlightFrames[CurrentFrameInFlightIndex];

		FRHIPooledRenderQuery* QueryPtr = nullptr;
		switch (Event)
		{
		case EDynamicResolutionStateEvent::BeginDynamicResolutionRendering:
			QueryPtr = &InFlightFrame.BeginDynamicResolutionQuery; break;
		case EDynamicResolutionStateEvent::EndDynamicResolutionRendering:
			QueryPtr = &InFlightFrame.EndDynamicResolutionQuery; break;
		case EDynamicResolutionStateEvent::EndFrame:
			QueryPtr = &InFlightFrame.EndFrameQuery; break;
		default: check(0);
		}

		*QueryPtr = RenderQueryPool->AllocateQuery();
		RHICmdList.EndRenderQuery(QueryPtr->GetQuery());
	}

	// Clobber CurrentFrameInFlightIndex for internal checks.
	if (Event == EDynamicResolutionStateEvent::EndFrame)
	{
		CurrentFrameInFlightIndex = -1;
		bUseTimeQueriesThisFrame = false;
	}
}

void FVarjoDynamicResolutionStateProxy::Finish()
{
	check(IsInRenderingThread());

	// Wait for all queries to land.
	HandLandedQueriesToHeuristic(/* bWait = */ true);
}

void FVarjoDynamicResolutionStateProxy::HandLandedQueriesToHeuristic(bool bWait)
{
	check(IsInRenderingThread());

	bool ShouldRefreshHeuristic = false;

	for (int32 i = 0; i < InFlightFrames.Num(); i++)
	{
		// If current in flight frame queries, ignore them since have not called EndRenderQuery().
		if (i == CurrentFrameInFlightIndex)
		{
			continue;
		}

		InFlightFrameQueries& InFlightFrame = InFlightFrames[i];

		// Results in microseconds.
		uint64 BeginFrameResult = 0;
		uint64 BeginDynamicResolutionResult = 0;
		uint64 EndDynamicResolutionResult = 0;
		uint64 EndFrameResult = 0;

		int32 LandingCount = 0;
		int32 QueryCount = 0;
		if (InFlightFrame.BeginFrameQuery.IsValid())
		{
			LandingCount += RHIGetRenderQueryResult(
				InFlightFrame.BeginFrameQuery.GetQuery(), BeginFrameResult, bWait) ? 1 : 0;
			QueryCount += 1;
		}

		if (InFlightFrame.BeginDynamicResolutionQuery.IsValid())
		{
			LandingCount += RHIGetRenderQueryResult(
				InFlightFrame.BeginDynamicResolutionQuery.GetQuery(), BeginDynamicResolutionResult, bWait) ? 1 : 0;
			QueryCount += 1;
		}

		if (InFlightFrame.EndDynamicResolutionQuery.IsValid())
		{
			LandingCount += RHIGetRenderQueryResult(
				InFlightFrame.EndDynamicResolutionQuery.GetQuery(), EndDynamicResolutionResult, bWait) ? 1 : 0;
			QueryCount += 1;
		}

		if (InFlightFrame.EndFrameQuery.IsValid())
		{
			LandingCount += RHIGetRenderQueryResult(
				InFlightFrame.EndFrameQuery.GetQuery(), EndFrameResult, bWait) ? 1 : 0;
			QueryCount += 1;
		}

		check(QueryCount == 0 || QueryCount == 4);

		// If all queries have landed, then hand the results to the heuristic.
		if (LandingCount == 4)
		{
			Heuristic.CommitPreviousFrameGPUTimings_RenderThread(
				InFlightFrame.HeuristicHistoryEntry,
				/* TotalFrameGPUBusyTimeMs = */ float(EndFrameResult - BeginFrameResult) / 1000.0f,
				/* DynamicResolutionGPUBusyTimeMs = */ float(EndDynamicResolutionResult - BeginDynamicResolutionResult) / 1000.0f,
				/* bGPUTimingsHaveCPUBubbles = */ !GRHISupportsGPUTimestampBubblesRemoval);

			// Reset this in-flight frame queries to be reused.
			InFlightFrame = InFlightFrameQueries();

			ShouldRefreshHeuristic = true;
		}
	}

	// Refresh the heuristic.
	if (ShouldRefreshHeuristic)
	{
		Heuristic.RefreshCurentFrameResolutionFraction_RenderThread();
	}
}

void FVarjoDynamicResolutionStateProxy::FindNewInFlightIndex()
{
	check(IsInRenderingThread());
	check(CurrentFrameInFlightIndex == -1);

	for (int32 i = 0; i < InFlightFrames.Num(); i++)
	{
		auto& InFlightFrame = InFlightFrames[i];
		if (!InFlightFrame.BeginFrameQuery.IsValid())
		{
			CurrentFrameInFlightIndex = i;
			break;
		}
	}

	// Allocate a new in-flight frame in the unlikely event.
	if (CurrentFrameInFlightIndex == -1)
	{
		CurrentFrameInFlightIndex = InFlightFrames.Num();
		InFlightFrames.Add(InFlightFrameQueries());
	}
}

FVarjoDynamicResolutionState::FVarjoDynamicResolutionState()
	: Proxy(new FVarjoDynamicResolutionStateProxy())
{
	check(IsInGameThread());
	bIsEnabled = false;
	bRecordThisFrame = false;
}

FVarjoDynamicResolutionState::~FVarjoDynamicResolutionState()
{
	check(IsInGameThread());

	// Deletes the proxy on the rendering thread to make sure we don't delete before a recommand using it has finished.
	FVarjoDynamicResolutionStateProxy* P = Proxy;
	ENQUEUE_RENDER_COMMAND(DeleteDynamicResolutionProxy)(
		[P](class FRHICommandList&)
	{
		P->Finish();
		delete P;
	});
}

bool FVarjoDynamicResolutionState::IsSupported() const
{
	return true;
}

void FVarjoDynamicResolutionState::ResetHistory()
{
	check(IsInGameThread());
	FVarjoDynamicResolutionStateProxy* P = Proxy;
	ENQUEUE_RENDER_COMMAND(DynamicResolutionResetHistory)(
		[P](class FRHICommandList&)
	{
		P->Reset();
	});

}

void FVarjoDynamicResolutionState::SetEnabled(bool bEnable)
{
	check(IsInGameThread());
	bIsEnabled = bEnable;
}

bool FVarjoDynamicResolutionState::IsEnabled() const
{
	check(IsInGameThread());
	return bIsEnabled;
}

float FVarjoDynamicResolutionState::GetResolutionFractionApproximation() const
{
	check(IsInGameThread());
	return Proxy->Heuristic.GetResolutionFractionApproximation_GameThread();
}

float FVarjoDynamicResolutionState::GetResolutionFractionUpperBound() const
{
	check(IsInGameThread());
	return Proxy->Heuristic.GetResolutionFractionUpperBound();
}

float FVarjoDynamicResolutionState::GetResolutionFraction() const
{
	check(IsInRenderingThread());
	return Proxy->Heuristic.QueryCurentFrameResolutionFraction_RenderThread();
}

void FVarjoDynamicResolutionState::ProcessEvent(EDynamicResolutionStateEvent Event)
{
	check(IsInGameThread());

	if (Event == EDynamicResolutionStateEvent::BeginFrame)
	{
		check(bRecordThisFrame == false);
		bRecordThisFrame = bIsEnabled;
	}

	// Early return if not recording this frame.
	if (!bRecordThisFrame)
	{
		return;
	}

	if (Event == EDynamicResolutionStateEvent::BeginFrame)
	{
		// Query game thread time in milliseconds.
		float PrevGameThreadTimeMs = FPlatformTime::ToMilliseconds(GGameThreadTime);

		FVarjoDynamicResolutionStateProxy* P = Proxy;
		ENQUEUE_RENDER_COMMAND(DynamicResolutionBeginFrame)(
			[PrevGameThreadTimeMs, P](class FRHICommandList& RHICmdList)
		{
			P->BeginFrame(RHICmdList, PrevGameThreadTimeMs);
		});
	}
	else
	{
		// Forward event to render thread.
		FVarjoDynamicResolutionStateProxy* P = Proxy;
		ENQUEUE_RENDER_COMMAND(DynamicResolutionBeginFrame)(
			[P, Event](class FRHICommandList& RHICmdList)
		{
			P->ProcessEvent(RHICmdList, Event);
		});

		if (Event == EDynamicResolutionStateEvent::EndFrame)
		{
			// Only record frames that have a BeginFrame event.
			bRecordThisFrame = false;
		}
	}
}

void FVarjoDynamicResolutionState::SetupMainViewFamily(class FSceneViewFamily& ViewFamily)
{
	check(IsInGameThread());

	if (bIsEnabled)
	{
		ViewFamily.SetScreenPercentageInterface(new FVarjoDynamicResolutionDriver(&Proxy->Heuristic, ViewFamily));
	}
}
