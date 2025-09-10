//! Resource gathering and interaction functions
//! Handles planting, harvesting, block hitting, and item usage

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::models::gatherableresource::{
    GatherableResource, GatherableResourceImpl, GatherableResourceTrait
};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::models::islandchunk::{
    IslandChunk, remove_block, update_block
};
use craft_island_pocket::helpers::{
    utils::{get_position_id},
    math::{fast_power_2},
    processing_guard::{ensure_not_locked},
    position::{calculate_chunk_id, get_chunk_and_position},
};
use craft_island_pocket::common::errors::{
    GameResult, GameError
};

pub fn plant(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, item_id: u16) {
    let player_data: PlayerData = world.read_model((player));
    let (chunk_id, position) = get_chunk_and_position(x, y, z);
    let mut resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
    assert!(resource.resource_id == 0, "Error: Resource exists");

    // Check if the block below is suitable for planting
    assert!(z > 0, "Error: Cannot plant at z=0");
    let below_z = z - 1;
    let below_chunk_id: u128 = get_position_id(x / 4, y / 4, below_z / 4);
    let below_chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, below_chunk_id));

    // Calculate position of block below in its chunk
    let below_x_local = x % 4;
    let below_y_local = y % 4;
    let below_z_local = below_z % 2;

    // Determine which blocks storage to use based on z position (chunks store blocks in two 128-bit values)
    let blocks = if (below_z % 4) < 2 { below_chunk.blocks2 } else { below_chunk.blocks1 };

    // Extract block ID at position from packed storage (each block uses 4 bits)
    let shift: u128 = fast_power_2(((below_x_local + below_y_local * 4 + below_z_local * 16) * 4).into()).into();
    let block_below: u64 = ((blocks / shift) % 8).try_into().unwrap();

    // For seeds (wheat seed id 47, carrot seed id 51, potato id 53), check if there's farmland (tilled dirt) below
    if item_id == 47 || item_id == 51 || item_id == 53 {
        assert!(block_below == 2, "Error: Seeds and potatoes need farmland (tilled dirt) below");
    }

    // For saplings (oak sapling id 46), check if there's dirt or grass below
    if item_id == 46 {
        assert!(block_below == 1 || block_below == 2, "Error: Saplings need dirt or grass below");
    }

    resource.resource_id = item_id;

    let mut inventory: Inventory = world.read_model((player, 0));
    inventory.remove_items(item_id, 1);
    world.write_model(@inventory);

    let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
    resource.planted_at = timestamp;
    resource.next_harvest_at = GatherableResourceImpl::calculate_next_harvest_time(item_id, timestamp);
    resource.harvested_at = 0;
    resource.max_harvest = GatherableResourceImpl::get_max_harvest(item_id);
    resource.remained_harvest = resource.max_harvest;
    resource.destroyed = false;

    // Simple 10% chance for golden tier (wheat, carrot, potato)
    if item_id == 47 || item_id == 51 || item_id == 53 {
        let mut player_data_mut: PlayerData = world.read_model((player));
        // Simple random using timestamp, position, and nonce with 6-digit primes
        let simple_random = (timestamp + x * 100003 + y * 100019 + z * 100043 + player_data_mut.random_nonce.into()) % 100;
        if simple_random < 10 {
            resource.tier = 1; // Golden (10% chance)
        } else {
            resource.tier = 0; // Normal
        }
        // Simple nonce increment
        player_data_mut.random_nonce = (player_data_mut.random_nonce + 1) % 1000000;
        world.write_model(@player_data_mut);
    } else {
        resource.tier = 0;
    }

    world.write_model(@resource);
}

pub fn harvest_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64) -> GameResult<()> {
    let player_data: PlayerData = world.read_model(player);
    let (chunk_id, position) = get_chunk_and_position(x, y, z);
    let mut resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
    
    if resource.resource_id == 0 || resource.destroyed {
        return GameResult::Err(GameError::ResourceNotFound);
    }
    
    let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;

    // If crop is not ready yet, return the seed
    if resource.next_harvest_at > timestamp {
        if resource.resource_id == 47 || resource.resource_id == 51 || resource.resource_id == 53 {
            // Try to return the seed
            let mut hotbar: Inventory = world.read_model((player, 0));
            let mut inventory: Inventory = world.read_model((player, 1));

            let mut qty_left = hotbar.add_items(resource.resource_id, 1);
            if qty_left == 0 {
                resource.destroyed = true;
                resource.resource_id = 0;
                world.write_model(@resource);
                world.write_model(@hotbar);
                return GameResult::Ok(());
            } else {
                qty_left = inventory.add_items(resource.resource_id, qty_left);
                if qty_left == 0 {
                    resource.destroyed = true;
                    resource.resource_id = 0;
                    world.write_model(@resource);
                    world.write_model(@hotbar);
                    world.write_model(@inventory);
                    return GameResult::Ok(());
                } else {
                    return GameResult::Err(GameError::InsufficientSpace);
                }
            }
        }
        return GameResult::Err(GameError::CooldownActive(resource.next_harvest_at));
    }
    
    let mut inventory: Inventory = world.read_model((player, 0));
    let tool: u16 = inventory.get_hotbar_selected_item_type();
    if resource.resource_id == 46 && tool != 35 { // sapling needs axe
        return GameResult::Err(GameError::InvalidItem(35));
    }
    if resource.resource_id == 49 && tool != 37 { // boulder needs pickaxe
        return GameResult::Err(GameError::InvalidItem(37));
    }
    
    resource.harvested_at = timestamp;
    resource.remained_harvest -= 1;

    let mut item_id = resource.resource_id;
    if item_id == 49 { // Boulder
        item_id = 33; // Rock
        InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, 1);
    } else if item_id == 46 { // Oak Tree
        InventoryTrait::add_to_player_inventories(ref world, player.into(), 46, 1);
        InventoryTrait::add_to_player_inventories(ref world, player.into(), 44, 1);
    } else if item_id == 47 { // Wheat seed
        let seed = timestamp % 2;
        let wheat_id = if resource.tier == 1 { 65 } else { 48 };
        InventoryTrait::add_to_player_inventories(ref world, player.into(), wheat_id, 1 + seed.try_into().unwrap());
    } else if item_id == 51 { // Carrot seed
        let seed = timestamp % 2;
        let carrot_id = if resource.tier == 1 { 66 } else { 52 };
        InventoryTrait::add_to_player_inventories(ref world, player.into(), carrot_id, 2 + seed.try_into().unwrap());
    } else if item_id == 53 { // Potato seed
        let seed = timestamp % 2;
        let potato_id = if resource.tier == 1 { 67 } else { 54 };
        InventoryTrait::add_to_player_inventories(ref world, player.into(), potato_id, 2 + seed.try_into().unwrap());
    } else {
        InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, 1);
    }

    if resource.max_harvest == 255 {
        return GameResult::Ok(());
    }
    if resource.max_harvest > 0 && resource.remained_harvest < resource.max_harvest {
        resource.destroyed = true;
        resource.resource_id = 0;
    } else {
        resource.next_harvest_at = timestamp + 10;
    }
    world.write_model(@resource);
    GameResult::Ok(())
}

pub fn hit_block_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, check_lock: bool) -> GameResult<()> {
    // Check if player is processing (only for unsafe version)
    if check_lock {
        ensure_not_locked(@world, player);
    }

    // get inventory and get current slot item id
    let mut inventory: Inventory = world.read_model((player, 0));

    if inventory.get_hotbar_selected_item_type() == 39 {
        return GameResult::Ok(());
    }

    // Check if trying to break blocks 1, 2, or 3 (dirt, grass, stone)
    let player_data: PlayerData = world.read_model((player));
    let chunk_id = calculate_chunk_id(x, y, z);
    let chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));

    // Get block ID at position
    let x_local = x % 4;
    let y_local = y % 4;
    let z_local = z % 2;
    let lower_layer = z % 4 < 2;
    let blocks = if lower_layer { chunk.blocks2 } else { chunk.blocks1 };

    let shift: u128 = fast_power_2(((x_local + y_local * 4 + z_local * 16) * 4).into()).into();
    let block_id: u64 = ((blocks / shift) % 16).try_into().unwrap();

    if block_id == 1 || block_id == 2 || block_id == 3 {
        return GameResult::Err(GameError::InvalidTarget);
    }

    remove_block(ref world, x, y, z);
    GameResult::Ok(())
}

pub fn use_item_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, check_lock: bool) -> GameResult<()> {
    if check_lock {
        ensure_not_locked(@world, player);
    }
    
    let mut inventory: Inventory = world.read_model((player, 0));
    let item_id: u16 = inventory.get_hotbar_selected_item_type();
    
    if item_id == 39 { // Hoe - convert grass to farmland
        let player_data: PlayerData = world.read_model((player));
        let chunk_id = calculate_chunk_id(x, y, z);
        let chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));

        // Get block ID at position
        let x_local = x % 4;
        let y_local = y % 4;
        let z_local = z % 2;
        let lower_layer = z % 4 < 2;
        let blocks = if lower_layer { chunk.blocks2 } else { chunk.blocks1 };

        let shift: u128 = fast_power_2(((x_local + y_local * 4 + z_local * 16) * 4).into()).into();
        let block_id: u64 = ((blocks / shift) % 16).try_into().unwrap();

        if block_id == 2 { // grass -> farmland (tilled dirt)
            update_block(ref world, x, y, z, 2);
            return GameResult::Ok(());
        }
        return GameResult::Err(GameError::InvalidTarget);
    }

    // Check if it's a gatherable resource that can be placed
    if item_id == 32 || item_id == 33 || item_id == 46 || item_id == 47 || item_id == 49 || item_id == 51 || item_id == 53 {
        return place_gatherable_resource_internal(ref world, player, x, y, z, item_id);
    }

    GameResult::Ok(())
}

pub fn place_gatherable_resource_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, resource_id: u16) -> GameResult<()> {
    let player_data: PlayerData = world.read_model(player);
    let (chunk_id, position) = get_chunk_and_position(x, y, z);

    // Check if there's already a resource at this position
    let existing_resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
    if existing_resource.resource_id != 0 && !existing_resource.destroyed {
        return GameResult::Err(GameError::InvalidPosition((x, y, z)));
    }

    // Check if player has the item
    let mut inventory: Inventory = world.read_model((player, 0));
    if inventory.get_item_amount(resource_id) == 0 {
        let mut main_inventory: Inventory = world.read_model((player, 1));
        if main_inventory.get_item_amount(resource_id) == 0 {
            return GameResult::Err(GameError::ItemNotFound);
        }
        main_inventory.remove_items(resource_id, 1);
        world.write_model(@main_inventory);
    } else {
        inventory.remove_items(resource_id, 1);
        world.write_model(@inventory);
    }

    // Create the resource
    let new_resource = if resource_id == 32 || resource_id == 33 || resource_id == 49 {
        GatherableResourceTrait::new_ready(player_data.current_space_owner, player_data.current_space_id, chunk_id, position, resource_id)
    } else {
        GatherableResourceTrait::new(player_data.current_space_owner, player_data.current_space_id, chunk_id, position, resource_id)
    };

    world.write_model(@new_resource);
    GameResult::Ok(())
}