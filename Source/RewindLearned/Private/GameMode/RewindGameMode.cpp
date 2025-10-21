// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindLearned/Public/GameMode/RewindGameMode.h"

ARewindGameMode::ARewindGameMode()
{
	// 设置默认pawn
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/RewindLearned/Content/BP/BP_RewindCharacter"));
	if (PlayerPawnBPClass.Class != nullptr) {DefaultPawnClass = PlayerPawnBPClass.Class;}
}

/* ---------------------调整时间操纵速度相关函数--------------------- */
void ARewindGameMode::SetRewindSpeedSlowest()
{
	GlobalRewindSpeed = SlowestRewindSpeed;
}

void ARewindGameMode::SetRewindSpeedSlower()
{
	GlobalRewindSpeed = SlowerRewindSpeed;
}

void ARewindGameMode::SetRewindSpeedNormal()
{
	GlobalRewindSpeed = NormalRewindSpeed;
}

void ARewindGameMode::SetRewindSpeedFaster()
{
	GlobalRewindSpeed = FasterRewindSpeed;
}

void ARewindGameMode::SetRewindSpeedFastest()
{
	GlobalRewindSpeed = FastestRewindSpeed;
}

/* ---------------------时间操纵相关函数--------------------- */
void ARewindGameMode::StartGlobalRewind()
{
	TRACE_BOOKMARK(TEXT("ARewindGameMode::StartGlobalRewind")); // 性能检测
	bIsGlobalRewinding = true;
	//GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Blue, FString::Printf(TEXT("ARewindGameMode::StartGlobalRewind()")));
	OnGlobalRewindStarted.Broadcast(); // 广播该事件，通知rewind component工作
}

void ARewindGameMode::StopGlobalRewind()
{
	TRACE_BOOKMARK(TEXT("ARewindGameMode::StartGlobalStop"));
	bIsGlobalRewinding = false;
	OnGlobalRewindCompleted.Broadcast();
}

void ARewindGameMode::StartGlobalFastForward()
{
	TRACE_BOOKMARK(TEXT("ARewindGameMode::StartGlobalFastForward"));
	bIsGlobalFastForwarding = true;
	OnGlobalFastForwardStarted.Broadcast();
}

void ARewindGameMode::StopGlobalFastForward()
{
	TRACE_BOOKMARK(TEXT("ARewindGameMode::StopGlobalFastForward"));
	bIsGlobalFastForwarding = false;
	OnGlobalFastForwardCompleted.Broadcast();
}

void ARewindGameMode::ToggleTimeScrub()
{
	/* 时间暂停开关函数 */
	bIsGlobalTimeScrubbing = !bIsGlobalTimeScrubbing;
	if (bIsGlobalTimeScrubbing)
	{
		TRACE_BOOKMARK(TEXT("ARewindGameMode::ToggleTimeScrub - Start Time Scrubbing"));
		OnGlobalTimeScrubStarted.Broadcast();
	}
	else
	{
		TRACE_BOOKMARK(TEXT("ARewindGameMode::ToggleTimeScrub - Stop Time Scrubbing"));
		OnGlobalTimeScrubCompleted.Broadcast();
	}
}
