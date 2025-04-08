// Copyright Epic Games, Inc. All Rights Reserved.

#include "CraftIslandGameMode.h"
#include "CraftIslandCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACraftIslandGameMode::ACraftIslandGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
