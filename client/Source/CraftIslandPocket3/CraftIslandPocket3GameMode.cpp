// Copyright Epic Games, Inc. All Rights Reserved.

#include "CraftIslandPocket3GameMode.h"
#include "CraftIslandPocket3Character.h"
#include "UObject/ConstructorHelpers.h"

ACraftIslandPocket3GameMode::ACraftIslandPocket3GameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
