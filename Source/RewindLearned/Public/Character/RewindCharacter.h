// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RewindCharacter.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class URewindComponent;
class UCameraComponent;
class USpringArmComponent;
class ARewindGameMode;

UCLASS()
class REWINDLEARNED_API ARewindCharacter : public ACharacter
{
	GENERATED_BODY()
	
	/* 时间控制组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rewind", meta = (AllowPrivateAccess = "true"))
	URewindComponent *RewindComponent;

	/* ---------------------------------- 输入相关 ---------------------------------- */
	/* mapping context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext *DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta = (AllowPrivateAccess = "true"))
	UInputAction *ToggleTimeScrubAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta = (AllowPrivateAccess = "true"))
	UInputAction *RewindAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta = (AllowPrivateAccess = "true"))
	UInputAction *FastForwardAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetRewindSpeedSlowestAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetRewindSpeedSlowerAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetRewindSpeedNormalAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetRewindSpeedFasterAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetRewindSpeedFastestAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleRewindParticipationAction;

public:
	// Sets default values for this character's properties
	ARewindCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	/* ------------------------------- 输入事件对应的回调函数 ------------------------------- */
	void ToggleTimeScrub(const FInputActionValue& Value);
	
	void Rewind(const FInputActionValue& Value);
	
	void StopRewinding(const FInputActionValue& Value);
	
	void FastForward(const FInputActionValue& Value);
	
	void StopFastForwarding(const FInputActionValue& Value);
	
	void SetRewindSpeedSlowest(const FInputActionValue& Value);
	
	void SetRewindSpeedSlower(const FInputActionValue& Value);
	
	void SetRewindSpeedNormal(const FInputActionValue& Value);
	
	void SetRewindSpeedFaster(const FInputActionValue& Value);
	
	void SetRewindSpeedFastest(const FInputActionValue& Value);
	
	void ToggleRewindParticipation(const FInputActionValue& Value);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	// 回溯的GameMode
	UPROPERTY(Transient, VisibleAnywhere, Category = "Rewind|Debug")
	ARewindGameMode* GameMode;
};
