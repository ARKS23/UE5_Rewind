// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "RewindableStaticMeshActor.generated.h"

class URewindComponent;

UCLASS()
class REWINDLEARNED_API ARewindableStaticMeshActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// 回溯组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rewind")
	URewindComponent* RewindComponent;

	// 构造函数中进行一些初始化操作
	ARewindableStaticMeshActor();
};
