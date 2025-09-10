//! Processing system functions
//! Handles material processing, cooking, and crafting machines

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::models::processing::{
    ProcessingLock, ProcessType, get_processing_config, MAX_PROCESSING_TIME
};
use craft_island_pocket::common::errors::{
    GameResult, GameError, GameResultTrait
};
use craft_island_pocket::helpers::inventory::{add_items_with_overflow};

pub fn start_processing_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, process_type: ProcessType, input_amount: u32, strict_validation: bool) -> GameResult<()> {
    // Get processing configuration
    let config = get_processing_config(process_type);
    if config.input_item == 0 || config.output_item == 0 {
        return GameResult::Err(GameError::InvalidInput("Invalid process type"));
    }

    // Check if already processing
    let existing_lock: ProcessingLock = world.read_model(player);
    if existing_lock.unlock_time > starknet::get_block_info().unbox().block_timestamp {
        return GameResult::Err(GameError::ProcessingInProgress);
    }

    // Calculate how many batches we can process
    let max_batches = input_amount / config.input_amount;
    if max_batches == 0 {
        return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), config.input_amount)));
    }

    // Check inventory for input items
    let mut hotbar: Inventory = world.read_model((player, 0));
    let mut inventory: Inventory = world.read_model((player, 1));
    
    let hotbar_amount = hotbar.get_item_amount(config.input_item.try_into().unwrap());
    let inventory_amount = inventory.get_item_amount(config.input_item.try_into().unwrap());
    let total_available = hotbar_amount + inventory_amount;
    
    let actual_batches = if total_available < input_amount {
        if strict_validation {
            return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), input_amount)));
        } else {
            total_available / config.input_amount
        }
    } else {
        max_batches
    };

    if actual_batches == 0 {
        return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), config.input_amount)));
    }

    // Calculate actual input needed
    let total_input_needed = actual_batches * config.input_amount;
    
    // Remove input items (hotbar first, then inventory)
    let mut remaining_to_remove = total_input_needed;
    if hotbar_amount > 0 {
        let to_remove_hotbar = if hotbar_amount >= remaining_to_remove { remaining_to_remove } else { hotbar_amount };
        hotbar.remove_items(config.input_item.try_into().unwrap(), to_remove_hotbar);
        remaining_to_remove -= to_remove_hotbar;
    }
    
    if remaining_to_remove > 0 && inventory_amount > 0 {
        inventory.remove_items(config.input_item.try_into().unwrap(), remaining_to_remove);
    }

    // Create processing lock
    let timestamp = starknet::get_block_info().unbox().block_timestamp;
    let processing_duration = actual_batches.into() * config.time_per_batch;
    let capped_duration = if processing_duration > MAX_PROCESSING_TIME {
        MAX_PROCESSING_TIME
    } else {
        processing_duration
    };

    let new_lock = ProcessingLock {
        player: player,
        process_type: process_type,
        batches_processed: actual_batches,
        unlock_time: timestamp + capped_duration,
    };

    // If processing finishes instantly (duration = 0), give items immediately
    if capped_duration == 0 {
        let remaining = add_items_with_overflow(ref world, player, config.output_item.try_into().unwrap(), actual_batches);
        
        if remaining > 0 {
            // Try to add remaining output items to hotbar, then inventory
            let remaining_in_hotbar = hotbar.add_items(config.output_item.try_into().unwrap(), remaining);
            let remaining_in_inventory = if remaining_in_hotbar > 0 {
                inventory.add_items(config.output_item.try_into().unwrap(), remaining_in_hotbar)
            } else {
                0
            };
            assert!(remaining_in_inventory == 0, "Not enough space for output");
        }

        // Return unprocessed input items
        let remaining_batches = max_batches - actual_batches;
        if remaining_batches > 0 {
            let items_to_return = remaining_batches * config.input_amount;
            // Try to add to hotbar first, then inventory
            let remaining_in_hotbar = hotbar.add_items(config.input_item.try_into().unwrap(), items_to_return);
            let remaining_in_inventory = if remaining_in_hotbar > 0 {
                inventory.add_items(config.input_item.try_into().unwrap(), remaining_in_hotbar)
            } else {
                0
            };
            assert!(remaining_in_inventory == 0, "Not enough space to return items");
        }
    }

    world.write_model(@new_lock);
    world.write_model(@hotbar);
    world.write_model(@inventory);

    GameResult::Ok(())
}

pub fn cancel_processing(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) -> GameResult<()> {
    let mut processing_lock: ProcessingLock = world.read_model(player);
    let timestamp = starknet::get_block_info().unbox().block_timestamp;

    if processing_lock.unlock_time <= timestamp {
        return GameResult::Err(GameError::InvalidInput("No active processing to cancel"));
    }

    // Return the input items based on batches processed
    let config = get_processing_config(processing_lock.process_type);
    let items_to_return = processing_lock.batches_processed * config.input_amount;
    add_items_with_overflow(ref world, player, config.input_item.try_into().unwrap(), items_to_return);

    // Clear the lock
    processing_lock.unlock_time = 0;
    processing_lock.batches_processed = 0;
    world.write_model(@processing_lock);

    GameResult::Ok(())
}

// Safe versions
pub fn try_start_processing(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, process_type: u8, amount: u32) -> bool {
    let process_enum = match process_type {
        0 => ProcessType::None,
        1 => ProcessType::GrindWheat,
        2 => ProcessType::CutLogs,
        3 => ProcessType::SmeltOre,
        4 => ProcessType::CrushStone,
        5 => ProcessType::ProcessClay,
        _ => ProcessType::None,
    };
    GameResultTrait::is_ok(@start_processing_internal(ref world, player, process_enum, amount, false))
}

pub fn try_cancel_processing(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) -> bool {
    GameResultTrait::is_ok(@cancel_processing(ref world, player))
}