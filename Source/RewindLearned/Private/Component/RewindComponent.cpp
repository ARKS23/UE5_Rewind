// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindLearned/Public/Component/RewindComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RewindLearned/Public/GameMode/RewindGameMode.h"

// Sets default values for this component's properties
URewindComponent::URewindComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// 时间回溯需要基于最新的物理状态，物理模拟完成后可以避免“脏读数据”
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}


// Called when the game starts
void URewindComponent::BeginPlay()
{
	// 使用 Unreal Profiler 标记函数执行区间，便于在性能分析工具中定位耗时操作。
	TRACE_CPUPROFILER_EVENT_SCOPE(URewindComponent::BeginPlay);
	Super::BeginPlay();

	// 获得GameMode
	GameMode = Cast<ARewindGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		// 没有GameMode无法全体同步，禁用Tick
		SetComponentTickEnabled(false);
		return;
	}

	// 获取Owner相关的组件
	OwnerRootComponent = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (bSnapshotMovementVelocityAndMode)
	{
		OwnerMovementComponent = Character ? Cast<UCharacterMovementComponent>(Character->GetMovementComponent()) : nullptr;
	}

	if (bPauseAnimationDuringTimeScrubbing)
	{
		OwnerSkeletalMesh = Character ? Character->GetMesh() : nullptr;
	}

	// 绑定GameMode的全局事件
	GameMode->OnGlobalRewindStarted.AddUniqueDynamic(this, &URewindComponent::OnGlobalRewindStarted);
	GameMode->OnGlobalRewindCompleted.AddUniqueDynamic(this, &URewindComponent::OnGlobalRewindCompleted);
	GameMode->OnGlobalFastForwardStarted.AddUniqueDynamic(this, &URewindComponent::OnGlobalFastForwardStarted);
	GameMode->OnGlobalFastForwardCompleted.AddUniqueDynamic(this, &URewindComponent::URewindComponent::OnGlobalFastForwardCompleted);
	GameMode->OnGlobalTimeScrubStarted.AddUniqueDynamic(this, &URewindComponent::OnGlobalTimeScrubStarted);
	GameMode->OnGlobalTimeScrubCompleted.AddUniqueDynamic(this, &URewindComponent::URewindComponent::OnGlobalTimeScrubCompleted);

	// 分配环形缓冲区
	InitializeRingBuffers(GameMode->MaxRewindSeconds);
}



// Called every frame
void URewindComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void URewindComponent::SetIsRewindingEnabled(bool bEnabled)
{
	bIsRewindingEnabled = bEnabled;
	if (!bIsRewindingEnabled) // 关闭回溯功能后，则对所有正在进行的时间操作进行终止
	{
		if (bIsRewinding) {OnGlobalRewindStarted();}
		if (bIsFastForwarding) {OnGlobalFastForwardCompleted();}
		if (bIsTimeScrubbing) {OnGlobalTimeScrubCompleted();}
	}
	else
	{
		check(GameMode);
		/* 同步全局的状态（状态一致性：该操作让世界中的所有可时间操作的物体进行同步） */
		if (!bIsRewinding && GameMode->IsGlobalRewinding()) OnGlobalRewindStarted();
		if (!bIsFastForwarding && GameMode->IsGlobalFastForwarding()) OnGlobalFastForwardStarted();
		if (!bIsTimeScrubbing && GameMode->IsGlobalTimeScrubbing()) OnGlobalTimeScrubStarted();
	}
}

// MARK
void URewindComponent::OnGlobalRewindStarted()
{
	const bool bAlreadyManipulatingTime = IsTimeBeingManipulated();
	if (TryStartTimeManipulation(bIsRewinding, !bIsTimeScrubbing))
	{
		OnRewindStarted.Broadcast();
		if (!bAlreadyManipulatingTime) OnTimeManipulationStarted.Broadcast();
	}
}

void URewindComponent::OnGlobalFastForwardStarted()
{
	const bool bAlreadyManipulatingTime = IsTimeBeingManipulated();
	if (bIsTimeScrubbing && TryStartTimeManipulation(bIsFastForwarding, !bIsTimeScrubbing))
	{
		OnFastForwardStarted.Broadcast();
		if (!bAlreadyManipulatingTime) OnTimeManipulationStarted.Broadcast();
	}
}

void URewindComponent::OnGlobalTimeScrubStarted()
{
	const bool bAlreadyManipulatingTime = IsTimeBeingManipulated();
	if (TryStartTimeManipulation(bIsTimeScrubbing, false))
	{
		OnFastForwardStarted.Broadcast();
		if (!bAlreadyManipulatingTime) OnTimeManipulationStarted.Broadcast(); 
	}
}

void URewindComponent::OnGlobalRewindCompleted()
{
	
}

void URewindComponent::OnGlobalFastForwardCompleted()
{
}

void URewindComponent::OnGlobalTimeScrubCompleted()
{
}

void URewindComponent::InitializeRingBuffers(float MaxRewindSeconds)
{
	/* 初始化环形缓冲区 */

	// 确定环形缓冲区的最大snapshot数量
	MaxSnapshots = FMath::CeilToInt32(MaxRewindSeconds / SnapshotFrequencySeconds);

	// 根据确定的最大数量来分配对应的内存空间
	constexpr int OneMB = 1024 * 1024;
	constexpr int ThreeMB = OneMB * 3;

	if (!bSnapshotMovementVelocityAndMode) //非包含角色运动模式回溯
	{
		const uint32 SnapshotBytes = sizeof(FTransformAndVelocitySnapshot); // 确定一个snapshot数据结构需要的空间
		const uint32 TotalSnapshotBytes = SnapshotBytes * MaxSnapshots; // 需要的总空间
		// 该模式下要求总内存小于1MB
		ensureMsgf(
			TotalSnapshotBytes < OneMB,
			TEXT("Actor %s has rewind component that requested %d bytes of snapshots. Check snapshot frequency!"),
			*GetOwner()->GetName(),
			TotalSnapshotBytes
		);
		// 确定最终的MaxSnapshots
		MaxSnapshots = FMath::Min(static_cast<uint32>(OneMB / SnapshotBytes), MaxSnapshots);
	}
	else //包含角色运动模式回溯
	{
		const uint32 SnapshotBytes = sizeof(FTransformAndVelocitySnapshot) + sizeof(FMovementVelocityAndModeSnapshot); // 这里还需要组件运动信息
		const uint32 TotalSnapshotBytes = SnapshotBytes * MaxSnapshots;
		// 该模式下要求总内存小于3MB
		ensureMsgf(
			TotalSnapshotBytes < ThreeMB,
			TEXT("Actor %s has rewind component that requested %d bytes of snapshots. Check snapshot frequency!"),
			*GetOwner()->GetName(),
			TotalSnapshotBytes
		);
		MaxSnapshots = FMath::Min(static_cast<uint32>(ThreeMB / SnapshotBytes), MaxSnapshots);
	}

	// 初始化环形缓存区
	TransformAndVelocitySnapshots.Reserve(MaxSnapshots);
	if (bSnapshotMovementVelocityAndMode && OwnerMovementComponent) MovementVelocityAndModeSnapshots.Reserve(MaxSnapshots); //角色需要包含运动组件信息缓存的初始化
}

bool URewindComponent::TryStartTimeManipulation(bool& bStateToSet, bool bResetTimeSinceSnapshotsChanged)
{
	/*
	 * 输入参数：1. 需要设置的状态  2. 是否重置差值计时器
	 * 返回参数： 表示是否成功启用时间操作
	 */

	// 没有开启时间回溯操作权限 或 已经是目标状态了，直接返回false
	if (!bIsRewindingEnabled || bStateToSet) return false;

	bStateToSet = true; //设置成对应的目标状态
	if (bResetTimeSinceSnapshotsChanged) TimeSinceSnapshotsChanged = 0.0f;

	// 回溯过程关闭物理模拟，视觉上达到回溯的效果
	PausePhysics();

	// 在开始时间操控时记录“动画原本是否暂停”
	bAnimationsPausedAtStartOfTimeManipulation = bPausedAnimation;

	return true;
}

