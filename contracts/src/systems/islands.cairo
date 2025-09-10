//! Island generation and navigation functions
//! Handles island creation, visiting, and resource spawning

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::models::gatherableresource::{GatherableResource, GatherableResourceTrait};
use craft_island_pocket::models::islandchunk::{IslandChunkTrait};
use craft_island_pocket::helpers::{
    utils::{get_position_id},
};

pub fn generate_island_part(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, island_id: u16) {
    assert!(island_id > 1, "Can't explore main island");

    let shift: u128 = get_position_id(x, y, z);

    let seed: u64 = starknet::get_block_info().unbox().block_timestamp;

    let island_compositions: Array<u128> = array![1229782938247303441, 2459565876494606882, 3689348814741910323];

    let island_type: u32 = ((seed * 7) % island_compositions.len().into()).try_into().unwrap();
    let island_composition: u128 = *island_compositions.at(island_type);

    // Generate 4x4 grid of chunks
    let player_felt = player.into();
    let mut x = 0;
    loop {
        if x >= 4 {
            break;
        }
        let mut y = 0;
        loop {
            if y >= 4 {
                break;
            }
            let chunk_offset = x * 0x000000000100000000000000000000 + y * 0x000000000000000000010000000000;
            world.write_model(@IslandChunkTrait::new(player_felt, island_id, shift + chunk_offset, 0, island_composition));
            y += 1;
        };
        x += 1;
    };

    spawn_random_resources(ref world, player.into(), island_id, shift, island_type);
}

pub fn visit(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, space_id: u16) {
    let mut player_data: PlayerData = world.read_model((player));
    player_data.current_space_id = space_id;
    world.write_model(@player_data);
}

pub fn visit_new_island(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) {
    let mut player_data: PlayerData = world.read_model((player));
    let island_id = player_data.last_space_created_id + 1;
    player_data.last_space_created_id = island_id;
    world.write_model(@player_data);

    let seed: u64 = starknet::get_block_info().unbox().block_timestamp;

    let island_compositions: Array<u128> = array![1229782938247303441, 2459565876494606882, 3689348814741910323];

    let island_type = ((seed * 7) % island_compositions.len().into()).try_into().unwrap();
    let island_composition: u128 = *island_compositions.at(island_type);

    // Generate 4x4 grid of chunks at origin (0, 0, 0)
    let shift: u128 = get_position_id(0, 0, 0);
    let player_felt = player.into();
    let mut x = 0;
    loop {
        if x >= 4 {
            break;
        }
        let mut y = 0;
        loop {
            if y >= 4 {
                break;
            }
            let chunk_offset = x * 0x000000000100000000000000000000 + y * 0x000000000000000000010000000000;
            world.write_model(@IslandChunkTrait::new(player_felt, island_id, shift + chunk_offset, 0, island_composition));
            y += 1;
        };
        x += 1;
    };

    spawn_random_resources(ref world, player.into(), island_id, shift, island_type);
}

pub fn spawn_random_resources(ref world: dojo::world::storage::WorldStorage, player: felt252, island_id: u16, shift: u128, island_type: u32) {
    let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;

    // Available resource types: wooden sticks (32), rocks (33), trees (46), wheat (47), boulders (49)
    let mut resource_types: Array<u16> = array![];

    if island_type == 0 {
        resource_types = array![32, 33, 46, 47, 49];
    } else if island_type == 1 {
        resource_types = array![32, 33, 47];
    } else if island_id == 2 {
        resource_types = array![33, 49];
    }

    // Generate chunk IDs dynamically to save contract size
    let mut i: u32 = 0;
    loop {
        if i >= 10 {
            break;
        }

        // Simple pseudo-random generation using timestamp + iteration
        let seed = timestamp + i.into();
        let resource_index = (seed % resource_types.len().into()).try_into().unwrap();
        let position = ((seed * 13) % 16 + 16).try_into().unwrap(); // 16 positions in base layer (16-31)

        let resource_id = *resource_types.at(resource_index);

        // Calculate chunk_id dynamically instead of using array
        let chunk_index: u32 = ((seed * 7) % 16).try_into().unwrap();
        let x_part: u32 = chunk_index % 4;
        let y_part: u32 = (chunk_index / 4) % 4;
        let x_offset: u128 = x_part.into() * 0x000000000100000000000000000000;
        let y_offset: u128 = y_part.into() * 0x000000000000000000010000000000;
        let chunk_id = shift + x_offset + y_offset;

        // Check if position is already occupied
        let existing_resource: GatherableResource = world.read_model((player, island_id, chunk_id, position));
        if existing_resource.resource_id == 0 {
            // Spawn ready resources for trees and wheat, normal for others
            if resource_id == 46 || resource_id == 47 {
                world.write_model(@GatherableResourceTrait::new_ready(player, island_id, chunk_id, position, resource_id));
            } else {
                world.write_model(@GatherableResourceTrait::new(player, island_id, chunk_id, position, resource_id));
            }
        }

        i += 1;
    };
}

// Safe versions
pub fn try_visit(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, space_id: u16) -> bool {
    // Simple validation - could be enhanced
    true
}

pub fn try_visit_new_island(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) -> bool {
    // Simple validation - could be enhanced  
    true
}

pub fn try_generate_island(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, x: u64, y: u64, z: u64, island_id: u16) -> bool {
    // Basic validation
    if island_id == 0 {
        return false;
    }
    // Would normally generate terrain here
    true
}