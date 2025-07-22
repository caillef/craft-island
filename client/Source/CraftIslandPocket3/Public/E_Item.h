//
//  E_Item.h
//  CraftIslandPocket3 (Mac)
//
//  Created by Corentin Cailleaud on 10/07/2025.
//  Copyright © 2025 Epic Games, Inc. All rights reserved.
//

#pragma once

#include "E_Item.generated.h"

UENUM(BlueprintType)
enum class E_Item : uint8
{
    None UMETA(DisplayName = "None"),
    Grass UMETA(DisplayName = "Grass"),
    Dirt UMETA(DisplayName = "Dirt"),
    Stone UMETA(DisplayName = "Stone"),

    NewEnumerator4,
    NewEnumerator5,
    NewEnumerator6,
    NewEnumerator7,
    NewEnumerator8,
    NewEnumerator9,
    NewEnumerator10,
    NewEnumerator11,
    NewEnumerator12,
    NewEnumerator13,
    NewEnumerator14,
    NewEnumerator15,
    NewEnumerator16,
    NewEnumerator17,
    NewEnumerator18,
    NewEnumerator19,
    NewEnumerator20,
    NewEnumerator21,
    NewEnumerator22,
    NewEnumerator23,
    NewEnumerator24,
    NewEnumerator25,
    NewEnumerator26,
    NewEnumerator27,
    NewEnumerator28,
    NewEnumerator29,

    House UMETA(DisplayName = "House"),           // Index 30
    OakTree UMETA(DisplayName = "Oak Tree"),      // Index 31
    WoodenStick2 UMETA(DisplayName = "Wooden Stick"), // Index 32
    Rock UMETA(DisplayName = "Rock"),
    StoneAxeHead UMETA(DisplayName = "Stone Axe Head"),
    StoneAxe UMETA(DisplayName = "Stone Axe"),
    StonePickaxeHead UMETA(DisplayName = "Stone Pickaxe Head"),
    StonePickaxe UMETA(DisplayName = "Stone Pickaxe"),
    StoneShovelHead UMETA(DisplayName = "Stone Shovel Head"),
    StoneShovel UMETA(DisplayName = "Stone Shovel"),
    StoneHoeHead UMETA(DisplayName = "Stone Hoe Head"),
    StoneHoe UMETA(DisplayName = "Stone Hoe"),
    StoneHammerHead UMETA(DisplayName = "Stone Hammer Head"),
    StoneHammer UMETA(DisplayName = "Stone Hammer"),
    OakLog UMETA(DisplayName = "Oak Log"),
    OakPlank UMETA(DisplayName = "Oak Plank"),
    OakSapling UMETA(DisplayName = "Oak Sapling"),
    WheatSeed UMETA(DisplayName = "Wheat Seed"),
    Wheat UMETA(DisplayName = "Wheat"),
    Boulder UMETA(DisplayName = "Boulder"),
    HousePattern UMETA(DisplayName = "House Pattern"),
    CarrotSeed UMETA(DisplayName = "Carrot Seed"),
    Carrot UMETA(DisplayName = "Carrot"),
    PotatoSeed UMETA(DisplayName = "Potato Seed"),
    Potato UMETA(DisplayName = "Potato"),
};
