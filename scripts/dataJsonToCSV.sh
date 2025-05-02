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
      icon: "/Script/Paper2D.PaperSprite'\''/Game/CraftIsland/UI/items_icons_ItemIcon_\(.key).items_icons_ItemIcon_\(.key)'\''"
    })
  | sort_by(.index | tonumber)
  | (["Row Name,Index,Enum,Type,Max Stack Size,Icon"]
     + map("\(.index),\(.index),\(.name),\(.type),\(.max),\(.icon)"))[]
' data/items.json > data/items.csv
