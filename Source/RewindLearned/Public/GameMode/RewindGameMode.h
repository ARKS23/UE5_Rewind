// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RewindGameMode.generated.h"


/* 定义一些事件回溯事件类型 */ 
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalRewindStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalRewindCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalFastForwardStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalFastForwardCompleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalTimeScrubStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGlobalTimeScrubCompleted);


UCLASS()
class REWINDLEARNED_API ARewindGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/* --------------------- 预设置 ---------------------*/
	UPROPERTY(EditDefaultsOnly, Category = "Rewind")
	float MaxRewindSeconds = 120.0f;  // 最长回溯长度，用于初始化回溯缓存区大小
	
	ARewindGameMode();

public:
	/* --------------------- 时间回溯的速度相关参数 ---------------------*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewind")
	float SlowestRewindSpeed = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewind")
	float SlowerRewindSpeed = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewind")
	float NormalRewindSpeed = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewind")
	float FasterRewindSpeed = 2.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewind")
	float FastestRewindSpeed = 4.0f;

private:
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind") // Transient表示不需要被序列化
	float GlobalRewindSpeed = 1.0f;

public:
	/* --------------------- 时间回溯的速度设置函数 --------------------- */
	void SetRewindSpeedSlowest();
	
	void SetRewindSpeedSlower();
	
	void SetRewindSpeedNormal();
	
	void SetRewindSpeedFaster();
	
	void SetRewindSpeedFastest();

public:
	/* --------------------- 时间操作函数 --------------------- */
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void StartGlobalRewind();

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void StopGlobalRewind();

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void StartGlobalFastForward();

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void StopGlobalFastForward();

	// 事件暂停开关函数
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	void ToggleTimeScrub();
	
public:
	/* --------------------- 事件 --------------------- */
	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalRewindStarted OnGlobalRewindStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalRewindCompleted OnGlobalRewindCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalFastForwardStarted OnGlobalFastForwardStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalFastForwardCompleted OnGlobalFastForwardCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalTimeScrubStarted OnGlobalTimeScrubStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rewind")
	FOnGlobalTimeScrubCompleted OnGlobalTimeScrubCompleted;

private:
	/* --------------------- 状态相关参数 --------------------- */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind")
	bool bIsGlobalTimeScrubbing = false;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind")
	bool bIsGlobalRewinding = false;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind")
	bool bIsGlobalFastForwarding = false;

public:
	/* --------------------- 获取状态相关函数 --------------------- */
	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsGlobalTimeScrubbing() const { return bIsGlobalTimeScrubbing; };

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsGlobalRewinding() const { return bIsGlobalRewinding; };

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	bool IsGlobalFastForwarding() const { return bIsGlobalFastForwarding; };

	UFUNCTION(BlueprintCallable, Category = "Rewind")
	float GetGlobalRewindSpeed() const { return GlobalRewindSpeed; }
};
