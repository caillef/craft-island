//! Position and chunk calculation helpers
//! Provides unified functions for common geometric calculations in the game world

use craft_island_pocket::helpers::utils::{get_position_id};

// Helper functions for common position/chunk calculations
pub fn calculate_chunk_id(x: u64, y: u64, z: u64) -> u128 {
    get_position_id(x / 4, y / 4, z / 4)
}

pub fn calculate_position_in_chunk(x: u64, y: u64, z: u64) -> u8 {
    (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap()
}

pub fn get_chunk_and_position(x: u64, y: u64, z: u64) -> (u128, u8) {
    let chunk_id = calculate_chunk_id(x, y, z);
    let position = calculate_position_in_chunk(x, y, z);
    (chunk_id, position)
}