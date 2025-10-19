// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindLearnedGameMode.h"
#include "RewindLearnedCharacter.h"
#include "UObject/ConstructorHelpers.h"

ARewindLearnedGameMode::ARewindLearnedGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
