#!/bin/bash
jq -r 'to_entries
    | map({
        index: .key,
        name: .value.name,
        type: .value.type,
        max: (
        if .value.maxStackSize then .value.maxStackSize
        elif .value.type == "tool" then 1
        else 64
        end
        ),
        icon: "/Script/Paper2D.PaperSprite'\''/Game/CraftIsland/UI/ItemsIconsSprite/items_icons_\(.key).items_icons_\(.key)'\''",
        craft: (
        if .value.craft and (.value.craft | length > 0) then
            .value.craft[0].ingredients
        else
            ""
        end
        ),
        actorClass: (
        if .value.actorClass then .value.actorClass
        else
            ""
        end
        )
    })
    | sort_by(.index | tonumber)
    | (["Row Name,Index,Enum,Type,Max Stack Size,Icon,Craft,ActorClass"]
        + map("\(.index),\(.index),\(.name),\(.type),\(.max),\(.icon),\(.craft),\(.actorClass)"))[]
' data/items.json > data/items.csv
