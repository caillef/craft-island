//! Inventory management helpers
//! Provides unified functions for common inventory operations

use starknet::ContractAddress;
use dojo::model::{ModelStorage};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};

/// Helper function for inventory overflow handling
/// Tries to add items to hotbar first, then to inventory if there's overflow
/// Returns the number of items that couldn't fit in either inventory
pub fn add_items_with_overflow(
    ref world: dojo::world::storage::WorldStorage, 
    player: ContractAddress, 
    item_id: u16, 
    quantity: u32
) -> u32 {
    let mut hotbar: Inventory = world.read_model((player, 0));
    let overflow = hotbar.add_items(item_id, quantity);
    world.write_model(@hotbar);
    
    if overflow > 0 {
        let mut inventory: Inventory = world.read_model((player, 1));
        let remaining = inventory.add_items(item_id, overflow);
        world.write_model(@inventory);
        remaining
    } else {
        0
    }
}