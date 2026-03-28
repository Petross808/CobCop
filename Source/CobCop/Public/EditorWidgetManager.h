// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EditorScreenWidget.h"
#include "Templates/SharedPointer.h"
#include "EditorWidgetManager.generated.h"

UCLASS(Blueprintable)
class COBCOP_API AEditorWidgetManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEditorWidgetManager();

	UFUNCTION(BlueprintCallable)
	void Initialize();

	UFUNCTION(BlueprintCallable)
	void Dispose();

	UFUNCTION(BlueprintCallable)
	void AddWidget(USceneComponent* component);

	UFUNCTION(BlueprintCallable)
	void RemoveWidget(USceneComponent* component);

private:
	TSharedPtr<AEditorScreenWidget> EditorScreenWidget;
};
