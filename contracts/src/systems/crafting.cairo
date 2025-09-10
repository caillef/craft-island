//! Crafting system functions
//! Handles item crafting, recipes, and craft validation

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::models::gatherableresource::{GatherableResource};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::helpers::{
    craft::{craftmatch},
    position::{get_chunk_and_position},
};

pub fn try_inventory_craft(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) {
    let mut craftinventory: Inventory = world.read_model((player, 2));
    // mask to remove quantity
    let craft_matrix: u256 = (craftinventory.slots1.into() & 7229782938247303441);
    let wanteditem = craftmatch(craft_matrix);

    assert!(wanteditem > 0, "Not a valid recipe");
    let mut k = 0;
    loop {
        if k >= 9 {
            break;
        }
        craftinventory.remove_item(k, 1);
        k += 1;
    };

    InventoryTrait::add_to_player_inventories(ref world, player.into(), wanteditem, 1);

    world.write_model(@craftinventory);
}

pub fn try_craft(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, item: u16, x: u64, y: u64, z: u64) {
    if item == 0 {
        return try_inventory_craft(ref world, player);
    }

    // Stone Craft
    if item == 34 || item == 36 || item == 38 || item == 40 || item == 42 {
        let player_data: PlayerData = world.read_model((player));
        let (chunk_id, position) = get_chunk_and_position(x, y, z);
        let mut resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
        assert!(resource.resource_id == 33 && !resource.destroyed, "Error: No rock");
        resource.destroyed = true;
        resource.resource_id = 0;
        world.write_model(@resource);

        let mut hotbar: Inventory = world.read_model((player, 0));
        let selected_item = hotbar.get_hotbar_selected_item_type();
        if selected_item == 33 {
            InventoryTrait::add_to_player_inventories(ref world, player.into(), item, 1);
        }
    }
}

pub fn try_craft_helper(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, item_id: u32, x: u64, y: u64, z: u64) -> bool {
    // Simplified stone craft logic
    let item = item_id.try_into().unwrap();
    
    if item == 34 || item == 36 || item == 38 || item == 40 || item == 42 {
        let player_data: PlayerData = world.read_model(player);
        let (chunk_id, position) = get_chunk_and_position(x, y, z);
        let mut resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));

        if resource.resource_id != 33 || resource.destroyed {
            return false; // No rock
        }

        resource.destroyed = true;
        resource.resource_id = 0;
        world.write_model(@resource);

        let mut hotbar: Inventory = world.read_model((player, 0));
        let selected_item = hotbar.get_hotbar_selected_item_type();

        // Remove rock from inventory  
        if selected_item == 33 {
            hotbar.remove_items(33, 1);
        } else {
            let mut inventory: Inventory = world.read_model((player, 1));
            if hotbar.get_item_amount(33) > 0 {
                hotbar.remove_items(33, 1);
            } else {
                inventory.remove_items(33, 1);
            }
            world.write_model(@inventory);
        }

        let overflow = hotbar.add_items(item, 1);
        if overflow > 0 {
            let mut inventory: Inventory = world.read_model((player, 1));
            inventory.add_items(item, overflow);
            world.write_model(@inventory);
        }

        world.write_model(@hotbar);
        return true;
    }

    false
}