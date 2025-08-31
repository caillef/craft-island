use craft_island_pocket::helpers::{
    math::{fast_power_2},
};
use starknet::get_caller_address;
use craft_island_pocket::models::common::PlayerData;
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::helpers::utils::get_position_id;
use dojo::{world::WorldStorage,model::ModelStorage};

pub fn get_block_at_pos(blocks: u128, x: u64, y: u64, z: u64) -> u64 {
    let shift: u128 = fast_power_2(((x + y * 4 + z * 16) * 4).into()).into();
    return ((blocks / shift) % 8).try_into().unwrap();
}

#[derive(Copy, Drop, Serde)]
#[dojo::model]
pub struct IslandInfo {
    #[key]
    pub island_owner: felt252,
    #[key]
    pub island_id: u16,
    #[key]
    pub chunk_id: u128, // pos x y z & 42 mask bits
    pub version: u8,
    pub blocks1: u128,
    pub blocks2: u128,
}

#[derive(Copy, Drop, Serde)]
#[dojo::model]
pub struct IslandChunk {
    #[key]
    pub island_owner: felt252,
    #[key]
    pub island_id: u16,
    #[key]
    pub chunk_id: u128, // pos x y z & 42 mask bits
    pub version: u8,
    pub blocks1: u128,
    pub blocks2: u128,
}

pub fn place_block(
    ref world: WorldStorage,
    x: u64,
    y: u64,
    z: u64,
    block_id: u16
) {
    let player = get_caller_address();
    let player_data: PlayerData = world.read_model((player));
    let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
    let mut chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));

    let mut inventory: Inventory = world.read_model((player, 0));
    inventory.remove_items(block_id.try_into().unwrap(), 1);
    chunk.place_block(x, y, z, block_id);
    world.write_model(@inventory);
    world.write_model(@chunk);
}

pub fn remove_block(
    ref world: WorldStorage,
    x: u64,
    y: u64,
    z: u64
) -> bool {
    let player = get_caller_address();
    let player_data: PlayerData = world.read_model((player));
    let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
    let mut chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));
    let mut block_id = chunk.remove_block(x, y, z);
    if block_id == 0 {
        return false;
    }

    InventoryTrait::add_to_player_inventories(ref world, player.into(), block_id, 1);
    world.write_model(@chunk);
    true
}

pub fn update_block(
    ref world: WorldStorage,
    x: u64,
    y: u64,
    z: u64,
    tool: u16
) {
    let player = get_caller_address();
    let player_data: PlayerData = world.read_model((player));
    let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
    let mut chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));
    println!("update_block: z={}", z);
    chunk.update_block(x, y, z, tool);
    world.write_model(@chunk);
}

#[generate_trait]
pub impl IslandChunkImpl of IslandChunkTrait {
    fn new(owner: felt252, island_id: u16, chunk_id: u128, blocks1: u128, blocks2: u128) -> IslandChunk {
        IslandChunk {
            island_owner: owner,
            island_id: island_id,
            chunk_id: chunk_id,
            version: 0,
            blocks1: blocks1,
            blocks2: blocks2,
        }
    }

    fn update_block(ref self: IslandChunk, x: u64, y: u64, z: u64, tool: u16) {
        if z % 4 < 2 {
            let block_info = get_block_at_pos(self.blocks2, x % 4, y % 4, z % 2);
            println!("update: block={} z={}", block_info, z);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 41 && block_info == 1 {
                self.blocks2 -= block_info.into() * shift.into();
                self.blocks2 += 2 * shift.into();
            }
        } else {
            let block_info = get_block_at_pos(self.blocks1, x % 4, y % 4, z % 2);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 18 && block_info == 1 {
                self.blocks1 -= block_info.into() * shift.into();
                self.blocks1 += 2 * shift.into();
            }
        }
    }

    fn place_block(ref self: IslandChunk, x: u64, y: u64, z: u64, block_id: u16) {
        // Calculate local coordinates and determine which block storage to use
        let x_local = x % 4;
        let y_local = y % 4;
        let z_local = z % 2;
        let lower_layer = z % 4 < 2;

        // Get the appropriate block storage
        let blocks = if lower_layer { self.blocks2 } else { self.blocks1 };

        // Check if position is empty
        let block_info = get_block_at_pos(blocks, x_local, y_local, z_local);
        assert(block_info == 0, 'Error: block exists');

        // Calculate bit shift for updating block
        let index = ((x_local + y_local * 4 + z_local * 16) * 4).into();
        let shift: u128 = fast_power_2(index).into();

        // Place the block
        if lower_layer {
            self.blocks2 += block_id.into() * shift;
        } else {
            self.blocks1 += block_id.into() * shift;
        }
    }

    fn remove_block(ref self: IslandChunk, x: u64, y: u64, z: u64) -> u16 {
        // Calculate local coordinates
        let x_local = x % 4;
        let y_local = y % 4;
        let z_local = z % 2;
        let lower_layer = z % 4 < 2;

        // Get the appropriate block storage
        let blocks = if lower_layer { self.blocks2 } else { self.blocks1 };

        // Check if position has a block
        let block_info = get_block_at_pos(blocks, x_local, y_local, z_local);
        if block_info == 0 {
            return 0; // No block to remove
        }

        // Calculate bit shift for updating block
        let index = ((x_local + y_local * 4 + z_local * 16) * 4).into();
        let shift = fast_power_2(index).into();

        // Remove the block
        if lower_layer {
            self.blocks2 -= block_info.into() * shift;
        } else {
            self.blocks1 -= block_info.into() * shift;
        }

        return block_info.try_into().unwrap();
    }
}
