// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "DynamicResolutionProxy.h"
#include "DynamicResolutionState.h"
#include "SceneView.h"

class FVarjoDynamicResolutionDriver : public ISceneViewFamilyScreenPercentage
{
public:

	FVarjoDynamicResolutionDriver(const FDynamicResolutionHeuristicProxy* InProxy, const FSceneViewFamily& InViewFamily);

	virtual float GetPrimaryResolutionFractionUpperBound() const override;
	virtual ISceneViewFamilyScreenPercentage* Fork_GameThread(const class FSceneViewFamily& ForkedViewFamily) const override;
	virtual void ComputePrimaryResolutionFractions_RenderThread(TArray<FSceneViewScreenPercentageConfig>& OutViewScreenPercentageConfigs) const override;

private:
	const FDynamicResolutionHeuristicProxy* Proxy;
	const FSceneViewFamily& ViewFamily;
};

class FVarjoDynamicResolutionStateProxy
{
public:
	FVarjoDynamicResolutionStateProxy();

	void Reset();
	void BeginFrame(FRHICommandList& RHICmdList, float PrevGameThreadTimeMs);
	void ProcessEvent(FRHICommandList& RHICmdList, EDynamicResolutionStateEvent Event);
	void Finish();

	FDynamicResolutionHeuristicProxy Heuristic;

private:
	struct InFlightFrameQueries
	{
		FRHIPooledRenderQuery BeginFrameQuery;
		FRHIPooledRenderQuery BeginDynamicResolutionQuery;
		FRHIPooledRenderQuery EndDynamicResolutionQuery;
		FRHIPooledRenderQuery EndFrameQuery;

		uint64 HeuristicHistoryEntry;

		InFlightFrameQueries()
			: HeuristicHistoryEntry(FDynamicResolutionHeuristicProxy::kInvalidEntryId)
		{ }
	};

	FRenderQueryPoolRHIRef RenderQueryPool;
	TArray<InFlightFrameQueries> InFlightFrames;
	int32 CurrentFrameInFlightIndex;
	bool bUseTimeQueriesThisFrame;
	void HandLandedQueriesToHeuristic(bool bWait);
	void FindNewInFlightIndex();
};

class FVarjoDynamicResolutionState : public IDynamicResolutionState
{
public:
	FVarjoDynamicResolutionState();
	~FVarjoDynamicResolutionState() override;

	virtual bool IsSupported() const override;
	virtual void ResetHistory() override;
	virtual void SetEnabled(bool bEnable) override;
	virtual bool IsEnabled() const override;
	virtual float GetResolutionFractionApproximation() const override;
	virtual float GetResolutionFractionUpperBound() const override;
	float GetResolutionFraction() const;
	virtual void ProcessEvent(EDynamicResolutionStateEvent Event) override;
	virtual void SetupMainViewFamily(class FSceneViewFamily& ViewFamily) override;

private:
	FVarjoDynamicResolutionStateProxy * const Proxy;
	bool bIsEnabled;
	bool bRecordThisFrame;
};
