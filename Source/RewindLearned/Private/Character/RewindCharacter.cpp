// Fill out your copyright notice in the Description page of Project Settings.


#include "RewindLearned/Public/Character//RewindCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "GameMode/RewindGameMode.h"
#include "EnhancedInputComponent.h"
#include "RewindLearned/Public/Component/RewindComponent.h"

// Sets default values
ARewindCharacter::ARewindCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 创建Rewind Component
	RewindComponent = CreateDefaultSubobject<URewindComponent>(TEXT("RewindComponent"));
	RewindComponent->SnapshotFrequencySeconds = 1.0f / 30.0f;
	RewindComponent->bSnapshotMovementVelocityAndMode = true;
	RewindComponent->bPauseAnimationDuringTimeScrubbing = true;
}

// Called when the game starts or when spawned
void ARewindCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 配置mapping context
	if (APlayerController *PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// 获取GameMode
	GameMode = Cast<ARewindGameMode>(GetWorld()->GetAuthGameMode());
}

void ARewindCharacter::ToggleTimeScrub(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->ToggleTimeScrub();
}

void ARewindCharacter::Rewind(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->StartGlobalRewind();
}

void ARewindCharacter::StopRewinding(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->StopGlobalRewind();
}

void ARewindCharacter::FastForward(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->StartGlobalFastForward();
}

void ARewindCharacter::StopFastForwarding(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->StopGlobalFastForward();
}

void ARewindCharacter::SetRewindSpeedSlowest(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->SetRewindSpeedSlowest();
}

void ARewindCharacter::SetRewindSpeedSlower(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->SetRewindSpeedSlower();
}

void ARewindCharacter::SetRewindSpeedNormal(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->SetRewindSpeedNormal();
}

void ARewindCharacter::SetRewindSpeedFaster(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->SetRewindSpeedFaster();
}

void ARewindCharacter::SetRewindSpeedFastest(const FInputActionValue& Value)
{
	check(GameMode);
	if (GameMode) GameMode->SetRewindSpeedFastest();
}

void ARewindCharacter::ToggleRewindParticipation(const FInputActionValue& Value)
{
	// 自身是否参与时间回溯可以通过开关自身的component实现
	RewindComponent->SetIsRewindingEnabled(!RewindComponent->IsRewindingEnabled());
}

// Called every frame
void ARewindCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ARewindCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 绑定时间控制相关操作
	if (UEnhancedInputComponent *EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 时间暂停开关
		EnhancedInputComponent->BindAction(ToggleTimeScrubAction, ETriggerEvent::Started, this, &ARewindCharacter::ToggleTimeScrub);
		// 角色是否参与时间事件开关
		EnhancedInputComponent->BindAction(ToggleRewindParticipationAction, ETriggerEvent::Started, this, &ARewindCharacter::ToggleRewindParticipation);
		// Rewind
		EnhancedInputComponent->BindAction(RewindAction, ETriggerEvent::Started, this, &ARewindCharacter::Rewind);
		EnhancedInputComponent->BindAction(RewindAction, ETriggerEvent::Completed, this, &ARewindCharacter::StopRewinding);
		// FastForward
		EnhancedInputComponent->BindAction(FastForwardAction, ETriggerEvent::Started, this, &ARewindCharacter::FastForward);
		EnhancedInputComponent->BindAction(FastForwardAction, ETriggerEvent::Completed, this, &ARewindCharacter::StopFastForwarding);
		// 速度设置
		EnhancedInputComponent->BindAction(
			SetRewindSpeedSlowestAction, ETriggerEvent::Started, this, &ARewindCharacter::SetRewindSpeedSlowest);
		EnhancedInputComponent->BindAction(
			SetRewindSpeedSlowerAction, ETriggerEvent::Started, this, &ARewindCharacter::SetRewindSpeedSlower);
		EnhancedInputComponent->BindAction(
			SetRewindSpeedNormalAction, ETriggerEvent::Started, this, &ARewindCharacter::SetRewindSpeedNormal);
		EnhancedInputComponent->BindAction(
			SetRewindSpeedFasterAction, ETriggerEvent::Started, this, &ARewindCharacter::SetRewindSpeedFaster);
		EnhancedInputComponent->BindAction(
			SetRewindSpeedFastestAction, ETriggerEvent::Started, this, &ARewindCharacter::SetRewindSpeedFastest);
	}
}

