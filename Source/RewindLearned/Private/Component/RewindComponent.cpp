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
	if (TryStopTimeManipulation(bIsRewinding, !bIsTimeScrubbing, false))
	{
		bLastTimeManipulationWasRewind = true; // 用于方向控制，后续维护lastIndex和插值需要用到

		OnRewindCompleted.Broadcast();
		if (!IsTimeBeingManipulated()) OnTimeManipulationCompleted.Broadcast();
	}
}

bool URewindComponent::HandleInsufficientSnapshots()
{
	/* 该函数处理插值snapshot不够的情况(小于或等于1), 若不够则返回true */
	// 防止数据错位：“物理快照”和“移动快照”两个缓冲区的数量必须“永远”保持一致。
	check(!bSnapshotMovementVelocityAndMode || TransformAndVelocitySnapshots.Num() != MovementVelocityAndModeSnapshots.Num());

	// 零快照的情况
	if (LatestSnapshotIndex < 0 || TransformAndVelocitySnapshots.Num() == 0) return true;
	
	// 一快照的情况： 无法进行插值，则直接使用仅剩的一个快照
	if (TransformAndVelocitySnapshots.Num() == 1)
	{
		ApplySnapshot(TransformAndVelocitySnapshots[0], false);
		if (bSnapshotMovementVelocityAndMode) ApplySnapshot(MovementVelocityAndModeSnapshots[0], true);
		return true;
	}

	// 此时快照数量 > 2,调试检查越界情况
	check(LatestSnapshotIndex >= 0 && LatestSnapshotIndex < TransformAndVelocitySnapshots.Num());
	return false;
}

void URewindComponent::InterpolateAndApplySnapshots(bool bRewinding)
{
	/* 寻找并计算两个snapshot之间的平滑插值状态，并应用至Actor */
	// 前置安全性检查
	constexpr int MinSnapshotForInterpolation = 2; // 设置至少2个snapshot进行线性插值
	check(TransformAndVelocitySnapshots.Num() >= MinSnapshotForInterpolation);
	check(bRewinding && LatestSnapshotIndex < TransformAndVelocitySnapshots.Num() - 1 || bIsFastForwarding && LatestSnapshotIndex > 0);

	// 确定插值源和目标
	// LatestSnapshotIndex 始终是“目标”快照。
	// PreviousIndex 是“源”快照。
	// - 回溯时 (bRewinding=true)：我们从索引 5 移向 4。Latest=4, Previous=5 (4+1)。
	// - 快进时 (bRewinding=false)：我们从索引 4 移向 5。Latest=5, Previous=4 (5-1)。
	const int PreviousIndex = bRewinding ? LatestSnapshotIndex + 1 : LatestSnapshotIndex - 1;

	// 进行混合
	{
		const FTransformAndVelocitySnapshot &PreviousSnapshot = TransformAndVelocitySnapshots[PreviousIndex];
		const FTransformAndVelocitySnapshot &NextSnapshot = TransformAndVelocitySnapshots[LatestSnapshotIndex];

		// Alpha = 当前的插值进度 / 两个快照间的总时长
		// NextSnapshot.TimeSinceLastSnapshot 存储了 A 和 B 之间的时间间隔。
		// TimeSinceSnapshotsChanged 存储了我们在这段间隔中“走”了多远。
		const float Alpha = TimeSinceSnapshotsChanged / NextSnapshot.TimeSinceLastSnapshot;
		
		const FTransformAndVelocitySnapshot BlendSnapshotResult = BlendSnapshots(PreviousSnapshot, NextSnapshot, Alpha);
		ApplySnapshot(BlendSnapshotResult, false);
	}

	if (bSnapshotMovementVelocityAndMode) // 角色的运动状态选项
	{
		const FMovementVelocityAndModeSnapshot &PreviousSnapshot = MovementVelocityAndModeSnapshots[PreviousIndex];
		const FMovementVelocityAndModeSnapshot &NextSnapshot = MovementVelocityAndModeSnapshots[LatestSnapshotIndex];

		const float Alpha = TimeSinceSnapshotsChanged / NextSnapshot.TimeSinceLastSnapshot;

		const FMovementVelocityAndModeSnapshot BlendSnapshotResult = BlendSnapshots(PreviousSnapshot, NextSnapshot, Alpha);
		ApplySnapshot(BlendSnapshotResult, true);
	}
}

FTransformAndVelocitySnapshot URewindComponent::BlendSnapshots(const FTransformAndVelocitySnapshot& A,
                                                               const FTransformAndVelocitySnapshot& B, float Alpha)
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f); // 安全检查
	FTransformAndVelocitySnapshot BlendSnapshot;

	// 进行混合Transform
	BlendSnapshot.Transform.Blend(A.Transform, B.Transform, Alpha);

	// 混合连续值
	BlendSnapshot.LinearVelocity = FMath::Lerp(A.LinearVelocity, B.LinearVelocity, Alpha);
	BlendSnapshot.AngularVelocityInRadians = FMath::Lerp(A.AngularVelocityInRadians, B.AngularVelocityInRadians, Alpha);

	return BlendSnapshot;
}

FMovementVelocityAndModeSnapshot URewindComponent::BlendSnapshots(const FMovementVelocityAndModeSnapshot& A,
	const FMovementVelocityAndModeSnapshot& B, float Alpha)
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f); // 安全检查
	FMovementVelocityAndModeSnapshot BlendSnapshot;

	// 混合连续值
	BlendSnapshot.MovementVelocity = FMath::Lerp(A.MovementVelocity, B.MovementVelocity, Alpha);

	// 混合离散值: Alpha为0.5的阈值判断
	BlendSnapshot.MovementMode = Alpha < 0.5f ? A.MovementMode : B.MovementMode;

	return BlendSnapshot;
}

void URewindComponent::ApplySnapshot(const FTransformAndVelocitySnapshot& Snapshot, bool bApplyPhysics)
{
	/* 回溯到对应的Transform和速度 */
	GetOwner()->SetActorTransform(Snapshot.Transform);
	if (OwnerRootComponent && bApplyPhysics) // 回到正常世界时间流逝时调用该函数传入的bApplyPhysics为true
	{
		OwnerRootComponent->SetPhysicsLinearVelocity(Snapshot.LinearVelocity);
		OwnerRootComponent->SetPhysicsAngularVelocityInRadians(Snapshot.AngularVelocityInRadians);
	}
}

void URewindComponent::ApplySnapshot(const FMovementVelocityAndModeSnapshot& Snapshot,
                                     bool bApplyTimeDilationToVelocity)
{
	if (OwnerMovementComponent)
	{
		// 目的：让角色的“视觉移动速度”与时间操控的速度相匹配, 比如 2 倍速回溯时，角色也应该以 2 倍速（反向）移动。
		OwnerMovementComponent->Velocity = bApplyTimeDilationToVelocity ? Snapshot.MovementVelocity * GameMode->GetGlobalRewindSpeed(): Snapshot.MovementVelocity;
		OwnerMovementComponent->SetMovementMode(Snapshot.MovementMode);
	}
}

void URewindComponent::OnGlobalFastForwardCompleted()
{
	if (TryStopTimeManipulation(bIsFastForwarding, !bIsTimeScrubbing, false))
	{
		bLastTimeManipulationWasRewind = false; // 用于方向控制，后续维护lastIndex和插值需要用到

		OnFastForwardCompleted.Broadcast();
		if (!IsTimeBeingManipulated()) OnTimeManipulationCompleted.Broadcast();
	}
}

void URewindComponent::OnGlobalTimeScrubCompleted()
{
	if(TryStopTimeManipulation(bIsTimeScrubbing, false, true))
	{
		OnFastForwardCompleted.Broadcast();
		if (!IsTimeBeingManipulated()) OnTimeManipulationCompleted.Broadcast();
	}
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

void URewindComponent::RecordSnapshot(float DeltaTime)
{
	/* 记录snapshot的函数，每帧会检测情况调用 */
	TRACE_CPUPROFILER_EVENT_SCOPE(URewindComponent::RecordSnapshot);
	TimeSinceSnapshotsChanged += DeltaTime;

	// 未达到频率，不记录。 但第一帧总是记录
	if (TimeSinceSnapshotsChanged < SnapshotFrequencySeconds && TransformAndVelocitySnapshots.Num() != 0) return;

	// 缓冲区爆满的情况: 丢弃最老的snapshot
	if (TransformAndVelocitySnapshots.Num() == MaxSnapshots) TransformAndVelocitySnapshots.PopFront();

	// snapshot
	FTransform Transform = GetOwner()->GetActorTransform();
	FVector LinearVelocity = OwnerRootComponent->GetPhysicsLinearVelocity();
	FVector AngularVelocityInRadians = OwnerRootComponent->GetPhysicsAngularVelocityInRadians();

	// 存储snapshot
	LatestSnapshotIndex = TransformAndVelocitySnapshots.Emplace(TimeSinceSnapshotsChanged, Transform, LinearVelocity, AngularVelocityInRadians);

	if (bSnapshotMovementVelocityAndMode) // 角色运动可选项的记录
	{
		if (MovementVelocityAndModeSnapshots.Num() == MaxSnapshots) MovementVelocityAndModeSnapshots.PopFront();
		FVector MovementVelocity = OwnerMovementComponent->Velocity;
		TEnumAsByte<EMovementMode> MovementMode = OwnerMovementComponent->MovementMode;
		const int32 LastMovementSnapshotIndex = MovementVelocityAndModeSnapshots.Emplace(TimeSinceSnapshotsChanged, MovementVelocity, MovementMode);

		check(LastMovementSnapshotIndex == LatestSnapshotIndex); //检查两组snap的数据是否同步
	}
	
	TimeSinceSnapshotsChanged = 0.0f; //重置计时器，开始累计下一个snapshot的计时器
}

void URewindComponent::EraseFutureSnapshots()
{
	/* 删除最新index之后的snapshot */
	while (LatestSnapshotIndex < TransformAndVelocitySnapshots.Num() - 1)
	{
		TransformAndVelocitySnapshots.Pop();
	}
	if (bSnapshotMovementVelocityAndMode)
	{
		while (LatestSnapshotIndex < MovementVelocityAndModeSnapshots.Num() - 1)
		{
			MovementVelocityAndModeSnapshots.Pop();
		}
	}
}

void URewindComponent::PlaySnapshots(float DeltaTime, bool bRewinding)
{
	/* 根据本帧的时间（DeltaTime）和全局速度，计算出角色应该在时间轴上移动到哪个新位置 */
	TRACE_CPUPROFILER_EVENT_SCOPE(URewindComponent::PlaySnapshots);

	UnpauseAnimation();

	if (HandleInsufficientSnapshots()) { return; }

	DeltaTime *= GameMode->GetGlobalRewindSpeed(); // 匹配设置的速度
	TimeSinceSnapshotsChanged += DeltaTime;

	bool bReachedEndOfTrack = false;
	float LastSnapshotTime = TransformAndVelocitySnapshots[LatestSnapshotIndex].TimeSinceLastSnapshot;
	if (bRewinding) // 回溯方向
	{
		// 跳过小间隔粒度的snapshot
		while (LatestSnapshotIndex > 0 && TimeSinceSnapshotsChanged > LastSnapshotTime)
		{
			TimeSinceSnapshotsChanged -= LastSnapshotTime;
			LastSnapshotTime = TransformAndVelocitySnapshots[LatestSnapshotIndex].TimeSinceLastSnapshot;
			--LatestSnapshotIndex;
		}

		if (LatestSnapshotIndex == TransformAndVelocitySnapshots.Num() - 1) // 只剩一个snapshot,直接适配
		{
			ApplySnapshot(TransformAndVelocitySnapshots[LatestSnapshotIndex], false);
			if (bSnapshotMovementVelocityAndMode)
			{
				ApplySnapshot(MovementVelocityAndModeSnapshots[LatestSnapshotIndex], true);
			}
			return;
		}
		bReachedEndOfTrack = LatestSnapshotIndex == 0;
	}
	else // 快进方向
	{
		while (LatestSnapshotIndex < TransformAndVelocitySnapshots.Num() - 1 && TimeSinceSnapshotsChanged > LastSnapshotTime)
		{
			TimeSinceSnapshotsChanged -= LastSnapshotTime;
			LastSnapshotTime = TransformAndVelocitySnapshots[LatestSnapshotIndex].TimeSinceLastSnapshot;
			++LatestSnapshotIndex;
		}
		bReachedEndOfTrack = LatestSnapshotIndex ==  TransformAndVelocitySnapshots.Num() - 1;
	}

	// 到达左右端点的情况
	if (bReachedEndOfTrack)
	{
		TimeSinceSnapshotsChanged = FMath::Min(TimeSinceSnapshotsChanged, LastSnapshotTime);
		if (bAnimationsPausedAtStartOfTimeManipulation) PauseAnimation();
	}

	InterpolateAndApplySnapshots(bRewinding);
}

void URewindComponent::PauseTime(float DeltaTime, bool bRewinding)
{
	/* 核心逻辑不是“立即暂停”，而是平滑地完成当前的插值，然后才真正暂停。这确保了 Actor 总是“冻结”在两个快照之间的插值终点，而不是尴尬的中间位置。 */
	TRACE_CPUPROFILER_EVENT_SCOPE(URewindComponent::PauseTime);

	if (HandleInsufficientSnapshots()) return;  // 不够snapshot插值，不进行下面的插值流程

	if (bRewinding) // 上一次操作的方向是rewind
	{
		if (LatestSnapshotIndex == TransformAndVelocitySnapshots.Num() - 1)
		{
			ApplySnapshot(TransformAndVelocitySnapshots[LatestSnapshotIndex], false);
			if (bSnapshotMovementVelocityAndMode)
			{
				ApplySnapshot(MovementVelocityAndModeSnapshots[LatestSnapshotIndex], true);
			}
			PauseAnimation();
			return;
		}
	}

	float LastedSnapshotTime = TransformAndVelocitySnapshots[LatestSnapshotIndex].TimeSinceLastSnapshot;
	// 检查插值进度 (TimeSinceSnapshotsChanged) 是否还未完成
	if (TimeSinceSnapshotsChanged < LastedSnapshotTime)
	{
		DeltaTime *= GameMode->GetGlobalRewindSpeed(); // 获取本帧的时间，并乘以“回溯速度”。
		TimeSinceSnapshotsChanged = FMath::Min(TimeSinceSnapshotsChanged + DeltaTime, LastedSnapshotTime); //推进进度，并使用 FMath::Min 确保进度“不会超过”总时长。
	}

	
	InterpolateAndApplySnapshots(bRewinding);

	// 检查插值进度是否“已经”到达 (或非常接近) 总时长
	if (FMath::IsNearlyEqual(TimeSinceSnapshotsChanged, LastedSnapshotTime))
	{
		// / 只有当插值 100% 完成时，才调用 PauseAnimation() 来冻结动画。
		PauseAnimation();
	}
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

bool URewindComponent::TryStopTimeManipulation(bool& bStateToSet, bool bResetTimeSinceSnapshotsChanged,
	bool bResetMovementVelocity)
{
	if (!bStateToSet) return false;
	bStateToSet = false;

	if (!bIsTimeScrubbing)
	{
		/* 没有时停，需要回复到正常游戏模式状态的代码逻辑 */
		if (bResetTimeSinceSnapshotsChanged) TimeSinceSnapshotsChanged = 0.0f;

		// 恢复物理模拟和动画播放
		UnpausePhysics();
		UnpauseAnimation();

		// 应用最终快照状态
		if (LatestSnapshotIndex >= 0)
		{
			ApplySnapshot(TransformAndVelocitySnapshots[LatestSnapshotIndex], true);
			if (bSnapshotMovementVelocityAndMode)
			{
				// 速度清零，防止倒带后残留速度
				ApplySnapshot(MovementVelocityAndModeSnapshots[LatestSnapshotIndex], false);
			}
		}
		// 删除未来的快照，防止与新操作冲突
		EraseFutureSnapshots();
	}

	return true;
}

void URewindComponent::PausePhysics()
{
	if (OwnerRootComponent && OwnerRootComponent->BodyInstance.bSimulatePhysics)
	{
		bPausedPhysics = true;
		OwnerRootComponent->SetSimulatePhysics(false);
	}
}

void URewindComponent::UnpausePhysics()
{
	if (!bPausedPhysics) return;

	check(OwnerRootComponent);
	bPausedPhysics = true;
	OwnerRootComponent->SetSimulatePhysics(true);
	OwnerRootComponent->RecreatePhysicsState(); // 强制物理引擎重新生成组件的最新运动状态
}

void URewindComponent::PauseAnimation()
{
	// 检查预设值
	if (!bPauseAnimationDuringTimeScrubbing) return;

	check(OwnerSkeletalMesh);
	bPausedAnimation = true;
	OwnerSkeletalMesh->bPauseAnims = true; // 骨骼网格体暂停动画
}

void URewindComponent::UnpauseAnimation()
{
	if (!bPausedAnimation) return;

	check(OwnerSkeletalMesh);
	bPausedAnimation = false;
	OwnerSkeletalMesh->bPauseAnims = false;
}

