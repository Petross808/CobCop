// Fill out your copyright notice in the Description page of Project Settings.


#include "EditorWidgetManager.h"

// Sets default values
AEditorWidgetManager::AEditorWidgetManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	EditorScreenWidget = SNew(AEditorScreenWidget);
}

void AEditorWidgetManager::Initialize()
{
	EditorScreenWidget->Initialize();
}

void AEditorWidgetManager::Dispose()
{
	EditorScreenWidget->Dispose();
}

void AEditorWidgetManager::AddWidget(USceneComponent* component)
{
	EditorScreenWidget->AddWidgetToEditor(component);
}

void AEditorWidgetManager::RemoveWidget(USceneComponent* component)
{
	EditorScreenWidget->RemoveWidgetFromEditor(component);
}

