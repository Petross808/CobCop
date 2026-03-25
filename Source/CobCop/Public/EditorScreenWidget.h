// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SLevelViewport.h"
#include "Widgets/SWidget.h"
#include "Components/WidgetComponent.h"
#include "UnrealClient.h"
#include "SceneView.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class COBCOP_API AEditorScreenWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(AEditorScreenWidget)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

	void Initialize();
	void Dispose();
	void AddWidgetToEditor(USceneComponent* Component);
	void RemoveWidgetFromEditor(USceneComponent* Component);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedPtr<SLevelViewport> EditorViewport;

	bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData) const;

	class FComponentEntry
	{
	public:
		FComponentEntry();
		~FComponentEntry();

	public:

		bool bRemoving = false;
		TWeakObjectPtr<USceneComponent> Component;
		class UWidgetComponent* WidgetComponent;

		TSharedPtr<SWidget> ContainerWidget;
		TSharedPtr<SWidget> Widget;
		SConstraintCanvas::FSlot* Slot;
	};

	void RemoveEntryFromCanvas(AEditorScreenWidget::FComponentEntry& Entry);

	TMap<FObjectKey, FComponentEntry> ComponentMap;
	TSharedPtr<SConstraintCanvas> Canvas;
};
