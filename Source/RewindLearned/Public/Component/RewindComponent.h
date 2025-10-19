// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/RingBuffer.h"
#include "RewindComponent.generated.h"


class ARewindGameMode;
class UCharacterMovementComponent;

// 声明一些时间类型
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeManipulationStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeManipulationCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRewindStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRewindCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastForwardStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastForwardCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeScrubStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeScrubCompleted);

USTRUCT()
struct FTransformAndVelocitySnapshot
{
	/* 该结构体存储snapshot时物体的Transform、速度的数据 */
	GENERATED_BODY();
	
	UPROPERTY(Transient)
	float TimeSinceLastSnapshot = 0.0f;  // 记录当前与上一次快照的时间间隔，用于之后的插值计算

	UPROPERTY(Transient)
	FTransform Transform{FVector::ZeroVector}; //// 记录Transform（位置，旋转，缩放）

	UPROPERTY(Transient)
	FVector LinearVelocity = FVector::ZeroVector; // 记录线性速度

	UPROPERTY(Transient)
	FVector AngularVelocityInRadians = FVector::ZeroVector; // 记录角速度
};

USTRUCT()
struct FMovementVelocityAndModeSnapshot
{
	/* 该结构体存储snapshot时角色移动组件状态的数据 */
	GENERATED_BODY();

	// 记录当前快照与上一帧之间的时间间隔，用于插值计算
	UPROPERTY(Transient)
	float TimeSinceLastSnapshot = 0.0f;

	// 移动组件的速度记录
	UPROPERTY(Transient)
	FVector MovementVelocity = FVector::ZeroVector;

	// 记录角色当前的移动模式
	UPROPERTY(Transient)
	TEnumAsByte<enum EMovementMode> MovementMode = EMovementMode::MOVE_None;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REWINDLEARNED_API URewindComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	URewindComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/* ----------------------------- 委托事件 ----------------------------- */
	/* 时间操作通用事件、时间回溯、时间快进、时间暂停 的动态多播处理 */
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnTimeManipulationStarted OnTimeManipulationStarted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnTimeManipulationCompleted OnTimeManipulationCompleted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnRewindStarted OnRewindStarted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnRewindCompleted OnRewindCompleted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnRewindStarted OnFastForwardStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnRewindCompleted OnFastForwardCompleted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnTimeScrubStarted OnTimeScrubStarted;
	
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnTimeScrubCompleted OnTimeScrubCompleted;

protected:
	/* ----------------------------- 状态变量 ----------------------------- */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Rewind")
	bool bIsRewinding = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Rewind")
	bool bIsFastForwarding = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Rewind")
	bool bIsTimeScrubbing = false;

public:
	/* ----------------------------- 查看状态接口 ----------------------------- */
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsRewinding() const { return bIsRewinding; };

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsFastForwarding() const { return bIsFastForwarding; };
	
	UFUNCTION(BlueprintCallable, Category = "Rewind")
    bool IsTimeScrubbing() const { return bIsTimeScrubbing; };

	// 检查当前是否正在执行三种操作中的任意一种
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsTimeBeingManipulated() const { return bIsRewinding || bIsFastForwarding || bIsTimeScrubbing; };

private:
	/* ----------------------------- 功能性开关变量 ----------------------------- */
	// Whether rewinding is currently enabled（这是个功能开启变量，而之前的bIsRewinding是状态变量）
	UPROPERTY(VisibleAnywhere, Category = "Rewind")
	bool bIsRewindingEnabled = true;

public:
	/* ----------------------------- 功能性开关函数 ----------------------------- */
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsRewindingEnabled() const { return bIsRewindingEnabled; }
	
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void SetIsRewindingEnabled(bool bEnabled);
	
public:
	/* ----------------------------- 一些预设值 ----------------------------- */
	UPROPERTY(EditDefaultsOnly, Category = "Rewind")
	float SnapshotFrequencySeconds = 1.0f / 30.f;  // snapshot的频率设置，这里设置30hz

	UPROPERTY(EditDefaultsOnly, Category = "Rewind")
	bool bSnapshotMovementVelocityAndMode = false; // 是否snapshot运动组件的数据，回溯角色时需要用到

	UPROPERTY(EditDefaultsOnly, Category = "Rewind")
	bool bPauseAnimationDuringTimeScrubbing = false; // 时间暂停时是否暂停动画, 回溯角色时需要用到

private:
	/* ----------------------------- 实现时间操作功能所需要的一些结构和变量 ----------------------------- */
	// 存储snapshots中Transform等数据的环形缓冲区
	TRingBuffer<FTransformAndVelocitySnapshot> TransformAndVelocitySnapshots;

	// 存储snapshots中角色的运动数据的环形缓冲区
	TRingBuffer<FMovementVelocityAndModeSnapshot> MovementVelocityAndModeSnapshots;

	UPROPERTY(Transient, VisibleAnywhere, Category="Rewind|Debug")
	uint32 MaxSnapshots = 1; // Snapshots的最大存储数目数量，在BeginPlay中的环形缓冲区初始化时计算该变量

	UPROPERTY(Transient, VisibleAnywhere, Category="Rewind|Debug")
	float TimeSinceSnapshotsChanged = 0.0f; // 跟踪快照记录的时间间隔，用于判断是否需要生成新快照（达到阈值后触发 RecordSnapshot）
	
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	int32 LatestSnapshotIndex = -1;  // 最新快照的索引，快速定位环形缓冲区中的最新数据
	
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	UPrimitiveComponent* OwnerRootComponent;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	UCharacterMovementComponent* OwnerMovementComponent;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	USkeletalMeshComponent* OwnerSkeletalMesh;
	
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	bool bPausedPhysics = false; // 标记是否暂停物理模拟，时间暂停时可能用到
	
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	bool bPausedAnimation = false; // 标记是否暂停动画播放，时间暂停时可能用到
	
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	bool bAnimationsPausedAtStartOfTimeManipulation = false; // 记录在时间操作开始时（如倒带、快进）动画是否已被暂停。

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	bool bLastTimeManipulationWasRewind = true;  //记录上一次时间操作的类型（回溯或快进），用于时间暂停（Time Scrubbing）时的插值方向控制。

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	ARewindGameMode* GameMode;  // GameMode全局控制时需要用到

private:
	/* ----------------------------- 功能函数 ----------------------------- */
	UFUNCTION()
	void OnGlobalRewindStarted();
	
	UFUNCTION()
	void OnGlobalFastForwardStarted();
	
	UFUNCTION()
	void OnGlobalTimeScrubStarted();
	
	UFUNCTION()
	void OnGlobalRewindCompleted();
	
	UFUNCTION()
	void OnGlobalFastForwardCompleted();
	
	UFUNCTION()
	void OnGlobalTimeScrubCompleted();

private:
	/* ----------------------------- 辅助函数 ----------------------------- */
	// 初始化缓冲区大小
	void InitializeRingBuffers(float MaxRewindSeconds);

	// 记录snapshot并将snapshot存入缓冲区
	void RecordSnapshot(float DeltaTime);

	// 删除最新snapshot之后的所有snapshots
	void EraseFutureSnapshots();

	// 播放snapshots，第二个变量用于控制向前还是向后播放
	void PlaySnapshots(float DeltaTime, bool bRewinding);

	// Advances to the next snapshot if rewinding or fast forwarding, then freezes time
	void PauseTime(float DeltaTime, bool bRewinding);

	// 开始时间操作时要用到的辅助函数
	bool TryStartTimeManipulation(bool& bStateToSet, bool bResetTimeSinceSnapshotsChanged);

	// 结束时间操作时要用到的辅助函数
	bool TryStopTimeManipulation(bool& bStateToSet, bool bResetTimeSinceSnapshotsChanged, bool bResetMovementVelocity);

	// 暂停物理模拟特性
	void PausePhysics();

	// 恢复物理模拟特性和运动状态
	void UnpausePhysics();

	// 暂停动画播放
	void PauseAnimation();

	// 恢复动画播放
	void UnpauseAnimation();

	// 检查snapshot是否有2个可以进行插值处理; 检查快照的有效性
	bool HandleInsufficientSnapshots();

	// 对snapshot进行线性插值处理
	void InterpolateAndApplySnapshots(bool bRewinding);

	// 线性插值混合Transform等信息
	FTransformAndVelocitySnapshot BlendSnapshots(
		const FTransformAndVelocitySnapshot& A,
		const FTransformAndVelocitySnapshot& B,
		float Alpha);

	// 线性插值混合运动组件信息
	FMovementVelocityAndModeSnapshot BlendSnapshots(
		const FMovementVelocityAndModeSnapshot& A,
		const FMovementVelocityAndModeSnapshot& B,
		float Alpha);

	// 将snapshot的Transform等信息信息应用到owner上
	void ApplySnapshot(const FTransformAndVelocitySnapshot& Snapshot, bool bApplyPhysics);

	// 将snapshot的角色运动信息应用到owner上
	void ApplySnapshot(const FMovementVelocityAndModeSnapshot& Snapshot, bool bApplyTimeDilationToVelocity);
};
