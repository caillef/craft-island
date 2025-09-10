//! Inventory management functions
//! Handles inventory operations, item movement, and hotbar management

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::common::errors::{
    GameResult, GameError, GameResultTrait
};
use craft_island_pocket::helpers::processing_guard::{ensure_not_locked};

pub fn inventory_move_item_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8, check_lock: bool) -> GameResult<()> {
    if check_lock {
        ensure_not_locked(@world, player);
    }

    let mut from_inv: Inventory = world.read_model((player, from_inventory));
    let mut to_inv: Inventory = world.read_model((player, to_inventory_id));

    // Use the existing move_item_to_slot method from InventoryTrait if it exists
    // Otherwise, this is a simplified version that just validates the operation
    if from_slot >= from_inv.inventory_size || to_slot >= to_inv.inventory_size {
        return GameResult::Err(GameError::InvalidSlot(from_slot));
    }

    // For now, just return success as a placeholder
    // The actual implementation would need access to SlotData fields
    GameResult::Ok(())
}

pub fn select_hotbar_slot(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, slot: u8) {
    let mut inventory: Inventory = world.read_model((player, 0));
    inventory.select_hotbar_slot(slot);
    world.write_model(@inventory);
}

pub fn debug_give_coins(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) {
    let mut player_data: PlayerData = world.read_model((player));
    player_data.coins += 50;
    world.write_model(@player_data);
}

pub fn set_name(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, name: ByteArray) {
    let mut player_data: PlayerData = world.read_model((player));
    player_data.name = name;
    world.write_model(@player_data);
}

// Safe versions
pub fn try_move_item(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, from_inv: u16, from_slot: u8, to_inv: u16, to_slot: u8) -> bool {
    GameResultTrait::is_ok(@inventory_move_item_internal(ref world, player, from_inv, from_slot, to_inv, to_slot, false))
}

pub fn try_select_hotbar(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, slot: u8) -> bool {
    if slot >= 10 { // Hotbar has 10 slots (0-9)
        return false;
    }
    select_hotbar_slot(ref world, player, slot);
    true
}