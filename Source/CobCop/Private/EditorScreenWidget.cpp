// Fill out your copyright notice in the Description page of Project Settings.


#include "EditorScreenWidget.h"
#include "ILevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneViewExtension.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Layout/Geometry.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/SBox.h"
#include "EditorWidgetManager.h"


AEditorScreenWidget::FComponentEntry::FComponentEntry() : Slot(nullptr)
{
}

AEditorScreenWidget::FComponentEntry::~FComponentEntry()
{
	Widget.Reset();
	ContainerWidget.Reset();
}

void AEditorScreenWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SAssignNew(Canvas, SConstraintCanvas)
		];
}

void AEditorScreenWidget::Initialize()
{
	if (GEngine == nullptr) return;

	FLevelEditorViewportClient* current = GCurrentLevelEditingViewportClient;

	if (current == nullptr) return;

	const TSharedPtr<ILevelEditor> levelEditor = current->ParentLevelEditor.Pin();

	if (levelEditor.IsValid())
	{
		for (auto& viewport : levelEditor->GetViewports())
		{
			if (viewport == current->GetEditorViewportWidget())
			{
				EditorViewport = viewport;
				break;
			}
		}
	}

	if (!EditorViewport.IsValid()) return;

	EditorViewport->AddOverlayWidget(this->AsShared());
}

void AEditorScreenWidget::Dispose()
{
	if (EditorViewport.IsValid())
	{
		EditorViewport->RemoveOverlayWidget(this->AsShared());
	}

	Canvas.Reset();
	ComponentMap.Reset();
}

void AEditorScreenWidget::AddWidgetToEditor(USceneComponent* Component)
{
	if (Component)
	{
		FComponentEntry& Entry = ComponentMap.FindOrAdd(FObjectKey(Component));
		Entry.Component = Component;
		Entry.WidgetComponent = Cast<UWidgetComponent>(Component);
		Entry.WidgetComponent->InitWidget();

		TSharedRef<SWidget> Widget = Entry.WidgetComponent->GetWidget()->TakeWidget();

		Entry.Widget = Widget;

		Canvas->AddSlot()
			.Expose(Entry.Slot)
			[
				SAssignNew(Entry.ContainerWidget, SBox)
					[
						Widget
					]
			];
	}
}

void AEditorScreenWidget::RemoveWidgetFromEditor(USceneComponent* Component)
{
	if (ensure(Component))
	{
		if (FComponentEntry* EntryPtr = ComponentMap.Find(Component))
		{
			if (!EntryPtr->bRemoving)
			{
				RemoveEntryFromCanvas(*EntryPtr);
				ComponentMap.Remove(Component);
			}
		}
	}
}

bool AEditorScreenWidget::GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData) const
{
	if ((Viewport == NULL) || (Viewport->GetSizeXY().X == 0) || (Viewport->GetSizeXY().Y == 0))
	{
		return false;
	}

	int32 X = Viewport->GetInitialPositionXY().X;
	int32 Y = Viewport->GetInitialPositionXY().Y;

	uint32 SizeX = Viewport->GetSizeXY().X;
	uint32 SizeY = Viewport->GetSizeXY().Y;

	FIntRect UnconstrainedRectangle = FIntRect(X, Y, X + SizeX, Y + SizeY);

	ProjectionData.SetViewRectangle(UnconstrainedRectangle);

	FMinimalViewInfo viewInfo;

	viewInfo.Location = EditorViewport->GetViewportClient()->GetViewLocation();
	viewInfo.Rotation = EditorViewport->GetViewportClient()->GetViewRotation();

	// Create the view matrix
	ProjectionData.ViewOrigin = viewInfo.Location;
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(viewInfo.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

#if WITH_EDITOR
	ULevelEditorPlaySettings* PlaySettingsConfig = GetMutableDefault<ULevelEditorPlaySettings>();
	if (PlaySettingsConfig != nullptr && PlaySettingsConfig->DeviceToEmulate.IsEmpty())
	{
		if (GEditor != nullptr && GEditor->IsFeatureLevelPreviewActive() && GEditor->GetAllowConstrainedAspectRatioInPreview())
		{
			FSlateApplication::Get().OnConstrainedAspectRatioChanged.Broadcast(GEditor->PreviewPlatform.GetConstrainedAspectRatio());

			FMargin Safezones(GEditor->PreviewPlatform.GetSafeZones());
			if (Safezones != FSlateApplication::Get().GetCustomSafeZone())
			{
				FSlateApplication::Get().SetCustomSafeZone(Safezones);
				FSlateApplication::Get().OnDebugSafeZoneChanged.Broadcast(Safezones, true);
			}
		}
		else
		{
			FSlateApplication::Get().OnConstrainedAspectRatioChanged.Broadcast(0.f);

			if (FMargin() != FSlateApplication::Get().GetCustomSafeZone())
			{
				FSlateApplication::Get().ResetCustomSafeZone();
				FSlateApplication::Get().OnDebugSafeZoneChanged.Broadcast(FMargin(), true);
			}
		}
	}
#endif


	// Create the projection matrix (and possibly constrain the view rectangle)
	FMinimalViewInfo::CalculateProjectionMatrixGivenView(viewInfo, AspectRatio_MaintainXFOV, Viewport, /*inout*/ ProjectionData);

	for (auto& ViewExt : GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(Viewport)))
	{
		ViewExt->SetupViewProjectionMatrix(ProjectionData);
	};


	return true;
}

void AEditorScreenWidget::RemoveEntryFromCanvas(AEditorScreenWidget::FComponentEntry& Entry)
{
	// Mark the component was being removed, so we ignore any other remove requests for this component.
	Entry.bRemoving = true;

	if (TSharedPtr<SWidget> ContainerWidget = Entry.ContainerWidget)
	{
		Canvas->RemoveSlot(ContainerWidget.ToSharedRef());
	}
}

void AEditorScreenWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!EditorViewport.IsValid()) return;

	const FGeometry& viewportGeometry = EditorViewport->GetPaintSpaceGeometry();

	// cache projection data here and avoid calls to UWidgetLayoutLibrary.ProjectWorldLocationToWidgetPositionWithDistance
	FSceneViewProjectionData ProjectionData;
	FMatrix ViewProjectionMatrix;

	bool bHasProjectionData = GetProjectionData(EditorViewport->GetActiveViewport(), /*out*/ ProjectionData);
	if (bHasProjectionData)
	{
		ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
	}

	for (auto It = ComponentMap.CreateIterator(); It; ++It)
	{
		FComponentEntry& Entry = It.Value();

		if (USceneComponent* SceneComponent = Entry.Component.Get())
		{
			FVector WorldLocation = SceneComponent->GetComponentLocation();

			FVector2D ScreenPosition2D;
			const bool bProjected = [&Entry, bHasProjectionData, &WorldLocation, &ScreenPosition2D, &ProjectionData, &ViewProjectionMatrix]()
				{
					if (!bHasProjectionData)
					{
						return false;
					}

					return FSceneView::ProjectWorldToScreen(WorldLocation, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition2D);
				}();

			if (bProjected)
			{
				const double ViewportDist = FVector::Dist(ProjectionData.ViewOrigin, WorldLocation);
				const FVector2D RoundedPosition2D(FMath::RoundToDouble(ScreenPosition2D.X), FMath::RoundToDouble(ScreenPosition2D.Y));

				// If the root widget has pixel snapping disabled, then don't pixel snap the screen coordinates either otherwise
				// it'll always jump between pixels. This saves needing an explicit flag on the widget component, and is probably 
				// a better delegation of responsibility anyway, since changing the widget type can change the snapping as it wants
				bool bDisablePixelSnapping = Entry.Widget->GetPixelSnapping() == EWidgetPixelSnapping::Disabled;
				const FVector2D ScreenPositionToUse = bDisablePixelSnapping ? ScreenPosition2D : RoundedPosition2D;
				
				UObject* contextObject = EditorViewport->GetWorld();
				
				FVector2D ViewportPosition2D;
				USlateBlueprintLibrary::ScreenToViewport(contextObject, ScreenPositionToUse, OUT ViewportPosition2D);

				auto ViewportSize = EditorViewport->GetActiveViewport()->GetSizeXY();
				const FVector2D ViewportPositionScaled = viewportGeometry.GetLocalSize() * (ScreenPositionToUse / ViewportSize);
				
				const FVector ViewportPosition(ViewportPositionScaled.X, ViewportPositionScaled.Y, ViewportDist);

				FString a = ViewportPosition.ToString();
				UE_LOGFMT(LogTemp, Warning, "{pos}", a);

				Entry.ContainerWidget->SetVisibility(EVisibility::SelfHitTestInvisible);

				if (SConstraintCanvas::FSlot* CanvasSlot = Entry.Slot)
				{
					FVector2D AbsoluteProjectedLocation = viewportGeometry.LocalToAbsolute(FVector2D(ViewportPosition.X, ViewportPosition.Y));
					FVector2D LocalPosition = AllottedGeometry.AbsoluteToLocal(AbsoluteProjectedLocation);

					if (Entry.WidgetComponent)
					{
						LocalPosition = Entry.WidgetComponent->ModifyProjectedLocalPosition(viewportGeometry, LocalPosition);

						FVector2D ComponentDrawSize = Entry.WidgetComponent->GetDrawSize();
						FVector2D ComponentPivot = Entry.WidgetComponent->GetPivot();

						CanvasSlot->SetAutoSize(ComponentDrawSize.IsZero() || Entry.WidgetComponent->GetDrawAtDesiredSize());
						CanvasSlot->SetOffset(FMargin(ViewportPosition.X, ViewportPosition.Y, ComponentDrawSize.X, ComponentDrawSize.Y));
						CanvasSlot->SetAnchors(FAnchors(0, 0, 0, 0));
						CanvasSlot->SetAlignment(ComponentPivot);
						CanvasSlot->SetZOrder(static_cast<float>(-ViewportPosition.Z));
					}
					else
					{
						CanvasSlot->SetAutoSize(true);
						CanvasSlot->SetOffset(FMargin(LocalPosition.X, LocalPosition.Y, 0, 0));
						CanvasSlot->SetAnchors(FAnchors(0, 0, 0, 0));
						CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
						CanvasSlot->SetZOrder(static_cast<float>(-ViewportPosition.Z));
					}
				}
			}
			else
			{
				Entry.ContainerWidget->SetVisibility(EVisibility::Collapsed);
			}
		}
		else
		{
			RemoveEntryFromCanvas(Entry);
			It.RemoveCurrent();
			continue;
		}
	}
}