//! Commerce and trading functions
//! Handles buying, selling, and pricing of items

use starknet::ContractAddress;
use dojo::model::{ModelStorage};

use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::common::errors::{
    GameResult, GameError, GameResultTrait
};

pub fn get_item_price_internal(item_id: u16) -> u32 {
    if item_id == 48 { // wheat
        10
    } else if item_id == 52 { // carrot
        15
    } else if item_id == 54 { // potato
        20
    } else if item_id == 65 { // golden wheat
        100
    } else if item_id == 66 { // golden carrot
        150
    } else if item_id == 67 { // golden potato
        200
    } else if item_id == 33 { // rock
        5
    } else if item_id == 44 { // wood
        8
    } else {
        0
    }
}

pub fn get_item_price(item_id: u16) -> u32 {
    get_item_price_internal(item_id)
}

pub fn sell_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) -> GameResult<u32> {
    // Get sell inventory (id = 3)
    let mut sell_inventory: Inventory = world.read_model((player, 3));
    let mut player_data: PlayerData = world.read_model(player);

    let mut total_coins: u32 = 0;

    // Count coins for each item type (using same prices as original)
    let carrot_amount = sell_inventory.get_item_amount(52);  // Carrot
    let potato_amount = sell_inventory.get_item_amount(54);  // Potato  
    let wheat_amount = sell_inventory.get_item_amount(48);   // Wheat
    let golden_carrot_amount = sell_inventory.get_item_amount(66);  // Golden Carrot
    let golden_potato_amount = sell_inventory.get_item_amount(67);  // Golden Potato
    let golden_wheat_amount = sell_inventory.get_item_amount(65);   // Golden Wheat
    let bowl_amount = sell_inventory.get_item_amount(55);   // Bowl
    let bowl_soup_amount = sell_inventory.get_item_amount(71);   // Bowl of Soup

    total_coins += carrot_amount * 2;   // 2 coins per carrot
    total_coins += potato_amount * 8;   // 8 coins per potato
    total_coins += wheat_amount * 10;   // 10 coins per wheat
    total_coins += golden_carrot_amount * 20;   // 20 coins per golden carrot (10x)
    total_coins += golden_potato_amount * 80;   // 80 coins per golden potato (10x)
    total_coins += golden_wheat_amount * 100;   // 100 coins per golden wheat (10x)
    total_coins += bowl_amount * 5;   // 5 coins per bowl
    total_coins += bowl_soup_amount * 50;   // 50 coins per bowl of soup

    if total_coins == 0 {
        return GameResult::Err(GameError::InvalidInput("Nothing to sell"));
    }

    // Remove sold items (preserve original item-by-item logic)
    if carrot_amount > 0 {
        sell_inventory.remove_items(52, carrot_amount);
    }
    if potato_amount > 0 {
        sell_inventory.remove_items(54, potato_amount);
    }
    if wheat_amount > 0 {
        sell_inventory.remove_items(48, wheat_amount);
    }
    if golden_carrot_amount > 0 {
        sell_inventory.remove_items(66, golden_carrot_amount);
    }
    if golden_potato_amount > 0 {
        sell_inventory.remove_items(67, golden_potato_amount);
    }
    if golden_wheat_amount > 0 {
        sell_inventory.remove_items(65, golden_wheat_amount);
    }
    if bowl_amount > 0 {
        sell_inventory.remove_items(55, bowl_amount);
    }
    if bowl_soup_amount > 0 {
        sell_inventory.remove_items(71, bowl_soup_amount);
    }

    // Update inventories and player data
    world.write_model(@sell_inventory);
    
    player_data.coins += total_coins;
    world.write_model(@player_data);
    
    GameResult::Ok(total_coins)
}

pub fn try_sell(ref world: dojo::world::storage::WorldStorage, player: ContractAddress) -> bool {
    GameResultTrait::is_ok(@sell_internal(ref world, player))
}

pub fn buy_internal(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, item_ids: Span<u16>, quantities: Span<u32>, check_inventory_space: bool) -> GameResult<u32> {
    if item_ids.len() != quantities.len() {
        return GameResult::Err(GameError::InvalidInput("Mismatched array lengths"));
    }

    let mut player_data: PlayerData = world.read_model((player));
    let mut total_cost: u32 = 0;

    // Calculate total cost first
    let mut i = 0;
    loop {
        if i >= item_ids.len() {
            break;
        }
        
        let item_id = *item_ids.at(i);
        let quantity = *quantities.at(i);
        let price_per_item = get_item_price_internal(item_id);
        
        if price_per_item == 0 {
            return GameResult::Err(GameError::InvalidItem(item_id));
        }
        
        total_cost += price_per_item * quantity;
        i += 1;
    };

    // Check if player has enough coins
    if player_data.coins < total_cost {
        return GameResult::Err(GameError::InsufficientFunds(total_cost));
    }

    // Process purchases
    let mut actual_cost: u32 = 0;
    let mut i = 0;
    loop {
        if i >= item_ids.len() {
            break;
        }
        
        let item_id = *item_ids.at(i);
        let quantity = *quantities.at(i);
        let price_per_item = get_item_price_internal(item_id);
        
        if check_inventory_space {
            // Try to add items to inventories
            let mut hotbar: Inventory = world.read_model((player, 0));
            let mut inventory: Inventory = world.read_model((player, 1));
            
            let overflow = hotbar.add_items(item_id, quantity);
            let remaining = if overflow > 0 {
                inventory.add_items(item_id, overflow)
            } else {
                0
            };

            if remaining > 0 {
                // Refund for items that couldn't fit
                let refund = price_per_item * remaining;
                player_data.coins += refund;
                actual_cost -= refund;
            }

            world.write_model(@hotbar);
            world.write_model(@inventory);
        } else {
            InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, quantity);
        }
        
        actual_cost += price_per_item * (quantity - if check_inventory_space { 0 } else { 0 });
        i += 1;
    };

    // Deduct coins
    player_data.coins -= actual_cost;
    world.write_model(@player_data);
    
    GameResult::Ok(actual_cost)
}

pub fn try_buy_single(ref world: dojo::world::storage::WorldStorage, player: ContractAddress, item_id: u16, quantity: u32) -> bool {
    let item_ids: Array<u16> = array![item_id];
    let quantities: Array<u32> = array![quantity];
    GameResultTrait::is_ok(@buy_internal(ref world, player, item_ids.span(), quantities.span(), false))
}