// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindableActor/RewindableStaticMeshActor.h"

#include "Component/RewindComponent.h"


ARewindableStaticMeshActor::ARewindableStaticMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// 网格体物理设置
	GetStaticMeshComponent()->Mobility = EComponentMobility::Movable;
	GetStaticMeshComponent()->SetSimulatePhysics(true);

	// 回溯组件初始化和设置频率
	RewindComponent = CreateDefaultSubobject<URewindComponent>(TEXT("RewindComponent"));
	RewindComponent->SnapshotFrequencySeconds = 1.0f / 30.0f;
}
