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
    
    if total_available < input_amount {
        if strict_validation {
            return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), input_amount)));
        }
    }
    
    // Calculate batches to process based on input amount (like original)
    let batches_to_process = input_amount / config.input_amount;
    if batches_to_process == 0 {
        return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), config.input_amount)));
    }

    // Calculate processing time
    let total_time = batches_to_process.into() * config.time_per_batch;

    // Cap at max processing time
    let capped_time = if total_time > MAX_PROCESSING_TIME {
        MAX_PROCESSING_TIME
    } else {
        total_time
    };
    let actual_batches = (capped_time / config.time_per_batch).try_into().unwrap();

    if actual_batches == 0 {
        return GameResult::Err(GameError::InsufficientItems((config.input_item.try_into().unwrap(), config.input_amount)));
    }

    // Use the exact input amount requested (we already verified it's available)
    let items_to_remove = input_amount;
    
    // Remove input items (hotbar first, then inventory) 
    let mut remaining_to_remove = items_to_remove;
    if hotbar_amount > 0 {
        let to_remove_hotbar = if hotbar_amount >= remaining_to_remove { remaining_to_remove } else { hotbar_amount };
        hotbar.remove_items(config.input_item.try_into().unwrap(), to_remove_hotbar);
        remaining_to_remove -= to_remove_hotbar;
    }
    
    if remaining_to_remove > 0 && inventory_amount > 0 {
        inventory.remove_items(config.input_item.try_into().unwrap(), remaining_to_remove);
    }

    // Set processing lock
    let current_time = starknet::get_block_info().unbox().block_timestamp;
    let new_lock = ProcessingLock {
        player,
        unlock_time: current_time + capped_time,
        process_type: process_type,
        batches_processed: actual_batches
    };

    // If processing finishes instantly (duration = 0), give items immediately
    if capped_time == 0 {
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
    let processing_lock: ProcessingLock = world.read_model(player);
    let current_time = starknet::get_block_info().unbox().block_timestamp;

    if processing_lock.unlock_time == 0 {
        return GameResult::Err(GameError::InvalidInput("No active processing to cancel"));
    }

    // Get processing config
    let config = get_processing_config(processing_lock.process_type);

    // Calculate completed and remaining batches
    // Start time = unlock_time - (total_batches * time_per_batch)
    let total_processing_time = config.time_per_batch * processing_lock.batches_processed.into();
    let start_time = processing_lock.unlock_time - total_processing_time;
    let elapsed_time = current_time - start_time;

    // Calculate how many batches have been completed so far
    let completed_batches = (elapsed_time / config.time_per_batch).try_into().unwrap();
    let completed_batches = if completed_batches > processing_lock.batches_processed {
        processing_lock.batches_processed
    } else {
        completed_batches
    };

    let remaining_batches = processing_lock.batches_processed - completed_batches;

    // Get inventories
    let mut hotbar: Inventory = world.read_model((player, 0));
    let mut inventory: Inventory = world.read_model((player, 1));

    // Add output items for completed batches
    if completed_batches > 0 {
        let items_to_add = completed_batches * config.output_amount;
        // Try to add to hotbar first, then inventory
        let remaining_in_hotbar = hotbar.add_items(config.output_item.try_into().unwrap(), items_to_add);
        let remaining_in_inventory = if remaining_in_hotbar > 0 {
            inventory.add_items(config.output_item.try_into().unwrap(), remaining_in_hotbar)
        } else {
            0
        };
        assert!(remaining_in_inventory == 0, "Not enough space for output");
    }

    // Return unprocessed input items
    if remaining_batches > 0 {
        let items_to_return = remaining_batches * config.input_amount;
        // Try to add to hotbar first, then inventory
        let remaining_in_hotbar = hotbar.add_items(config.input_item.try_into().unwrap(), items_to_return);
        let remaining_in_inventory = if remaining_in_hotbar > 0 {
            inventory.add_items(config.input_item.try_into().unwrap(), remaining_in_hotbar)
        } else {
            0
        };
        assert!(remaining_in_inventory == 0, "Cannot return all input items");
    }

    // Write updated inventories
    world.write_model(@hotbar);
    world.write_model(@inventory);

    // Clear lock
    let cleared_lock = ProcessingLock {
        player,
        unlock_time: 0,
        process_type: ProcessType::None,
        batches_processed: 0
    };
    world.write_model(@cleared_lock);

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