#[starknet::interface]
trait IActions<T> {
    fn spawn(ref self: T);
    fn buy(ref self: T, item_ids: Array<u16>, quantities: Array<u32>);
    fn sell(ref self: T);
    fn hit_block(ref self: T, x: u64, y: u64, z: u64);
    fn use_item(ref self: T, x: u64, y: u64, z: u64);
    fn select_hotbar_slot(ref self: T, slot: u8);
    fn craft(ref self: T, item: u32, x: u64, y:u64, z: u64);
    fn inventory_move_item(ref self: T, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8);
    fn generate_island_part(ref self: T, x: u64, y: u64, z: u64, island_id: u16);
    fn visit(ref self: T, space_id: u16);
    fn visit_new_island(ref self: T);
    fn start_processing(ref self: T, process_type: u8, input_amount: u32);
    fn cancel_processing(ref self: T);
    fn execute_compressed_actions(ref self: T, packed_actions: felt252) -> u8;
    fn execute_packed_actions(ref self: T, packed_data: Array<felt252>) -> Array<bool>;
    fn debug_give_coins(ref self: T); // Debug function to give 50 free coins
    fn set_name(ref self: T, name: ByteArray);
    // Test functions for GameResult system (Ticket #1)
    fn test_gameresult_success(ref self: T) -> u32;
    fn test_gameresult_error_handling(ref self: T) -> bool;
    // TICKET #2: Test functions to demonstrate GameResult migration for hit_block
    fn test_hit_block_migration_demo(ref self: T) -> bool;
}

// dojo decorator
#[dojo::contract]
mod actions {
    use super::{IActions};
    use starknet::{get_caller_address, ContractAddress};
    use craft_island_pocket::models::common::{
        PlayerData
    };
    use craft_island_pocket::models::gatherableresource::{
        GatherableResource, GatherableResourceImpl, GatherableResourceTrait
    };
    use craft_island_pocket::models::inventory::{
        Inventory, InventoryTrait
    };
    use craft_island_pocket::models::islandchunk::{
        IslandChunk, place_block, remove_block, update_block, IslandChunkTrait
    };
    use craft_island_pocket::models::worldstructure::{WorldStructureTrait};
    use craft_island_pocket::models::processing::{
        ProcessingLock, ProcessType, get_processing_config, MAX_PROCESSING_TIME
    };
    use craft_island_pocket::helpers::{
        utils::{get_position_id},
        craft::{craftmatch},
        init::{init},
        math::{fast_power_2},
        processing_guard::{ensure_not_locked},
    };
    use craft_island_pocket::common::errors::{
        GameResult, GameError, GameResultTrait
    };

    use dojo::model::{ModelStorage};

    // Helper function to extract bits from felt252
    fn extract_u8(value: felt252, shift: u32) -> u8 {
        let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
        let masked: u256 = shifted - (shifted / 256) * 256;
        masked.try_into().unwrap()
    }

    // Helper function to extract u16 from felt252
    fn extract_u16(value: felt252, shift: u32) -> u16 {
        let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
        let masked: u256 = shifted - (shifted / 0x10000) * 0x10000;
        masked.try_into().unwrap()
    }

    // Helper function to extract u32 from felt252
    fn extract_u32(value: felt252, shift: u32) -> u32 {
        let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
        let masked: u256 = shifted - (shifted / 0x100000000) * 0x100000000;
        masked.try_into().unwrap()
    }

    // Helper function to extract u64 from felt252
    fn extract_u64(value: felt252, shift: u32, bits: u32) -> u64 {
        let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
        let mask: u256 = fast_power_2(bits.into()).into();
        let masked: u256 = shifted - (shifted / mask) * mask;
        masked.try_into().unwrap()
    }

    fn get_world(ref self: ContractState) -> dojo::world::storage::WorldStorage {
        self.world(@"craft_island_pocket")
    }

    fn plant(ref self: ContractState, x: u64, y: u64, z: u64, item_id: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
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

    fn harvest(ref self: ContractState, x: u64, y: u64, z: u64) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut resource: GatherableResource = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
        assert!(resource.resource_id > 0 && !resource.destroyed, "Error: Resource does not exists");
        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;

        // If crop is not ready yet, return the seed
        if resource.next_harvest_at > timestamp {
            if resource.resource_id == 47 || resource.resource_id == 51 || resource.resource_id == 53 {
                // Return the seed
                InventoryTrait::add_to_player_inventories(ref world, player.into(), resource.resource_id, 1);
                // Destroy the resource
                resource.destroyed = true;
                resource.resource_id = 0;
                world.write_model(@resource);
                return;
            }
            assert!(false, "Error: Can't harvest now");
        }
        let mut inventory: Inventory = world.read_model((player, 0));
        let tool: u16 = inventory.get_hotbar_selected_item_type();
        if resource.resource_id == 46 { // sapling
            assert!(tool == 35, "Error: need an axe");
        }
        if resource.resource_id == 49 { // boulder
            assert!(tool == 37, "Error: need a pickaxe");
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
            // Give 1-2 wheat (golden if tier is 1)
            let seed = timestamp % 2;
            let wheat_id = if resource.tier == 1 { 65 } else { 48 }; // 65 = Golden Wheat, 48 = Wheat
            InventoryTrait::add_to_player_inventories(ref world, player.into(), wheat_id, 1 + seed.try_into().unwrap());
        } else if item_id == 51 { // Carrot seed
            // Give 2-3 carrots (golden if tier is 1)
            let seed = timestamp % 2;
            let carrot_id = if resource.tier == 1 { 66 } else { 52 }; // 66 = Golden Carrot, 52 = Carrot
            InventoryTrait::add_to_player_inventories(ref world, player.into(), carrot_id, 2 + seed.try_into().unwrap());
        } else if item_id == 53 { // Potato seed
            // Give 2-3 potatoes (golden if tier is 1)
            let seed = timestamp % 2;
            let potato_id = if resource.tier == 1 { 67 } else { 54 }; // 67 = Golden Potato, 54 = Potato
            InventoryTrait::add_to_player_inventories(ref world, player.into(), potato_id, 2 + seed.try_into().unwrap());
        } else {
            InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, 1);
        }

        if resource.max_harvest == 255 {
            return;
        }
        if resource.max_harvest > 0 && resource.remained_harvest < resource.max_harvest {
            resource.destroyed = true;
            resource.resource_id = 0;
        } else {
            resource.next_harvest_at = timestamp + 10;
        }
        world.write_model(@resource);
    }

    fn buy(
        ref self: ContractState, item_ids: Array<u16>, quantities: Array<u32>
    ) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let mut player_data: PlayerData = world.read_model((player));

        // Validate input arrays have same length
        assert(item_ids.len() == quantities.len(), 'Arrays must have same length');
        assert(item_ids.len() > 0, 'Must buy at least one item');

        // Calculate total cost and validate items
        let mut total_cost: u32 = 0;
        let mut i: u32 = 0;
        loop {
            if i >= item_ids.len() {
                break;
            }

            let item_id = *item_ids.at(i);
            let quantity = *quantities.at(i);

            // Get price for each item
            let price_per_item = if item_id == 47 {
                10 // Wheat seed - 10 coins
            } else if item_id == 53 {
                5  // Potato seed - 5 coins
            } else if item_id == 51 {
                2  // Carrot seed - 2 coins
            } else if item_id == 60 {
                5  // Workshop Pattern - 5 coins
            } else if item_id == 61 {
                100  // Well Pattern - 100 coins
            } else if item_id == 62 {
                3000  // Kitchen Pattern - 3000 coins
            } else if item_id == 63 {
                500  // Warehouse Pattern - 500 coins
            } else if item_id == 64 {
                10000  // Brewery Pattern - 10000 coins
            } else {
                assert(false, 'Item not available for purchase');
                0
            };

            total_cost += price_per_item * quantity;
            i += 1;
        };

        // Check if player has enough coins
        assert(player_data.coins >= total_cost, 'Not enough coins');

        // Deduct coins
        player_data.coins -= total_cost;

        // Add items to player inventory
        i = 0;
        loop {
            if i >= item_ids.len() {
                break;
            }

            let item_id = *item_ids.at(i);
            let quantity = *quantities.at(i);

            InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, quantity);

            i += 1;
        };

        // Save player data
        world.write_model(@player_data);
    }

    fn sell(ref self: ContractState) {
        let mut world = get_world(ref self);
        let player = get_caller_address();

        // Get sell inventory (id = 3)
        let mut sell_inventory: Inventory = world.read_model((player, 3));
        let mut player_data: PlayerData = world.read_model((player));

        let mut total_coins: u32 = 0;

        // Count coins for each item type
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

        // Remove sold items
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

        // Update player coins
        player_data.coins += total_coins;

        // Save changes
        world.write_model(@player_data);
        world.write_model(@sell_inventory);
    }

    fn try_inventory_craft(ref self: ContractState) {
        let mut world = get_world(ref self);
        let player = get_caller_address();

        let mut craftinventory: Inventory = world.read_model((player, 2));
        // mask to remove quantity
        let craft_matrix: u256 = (craftinventory.slots1.into() & 7229938216006767371223902296383078621116345691456360212366842288707796205568);
        let wanteditem = craftmatch(craft_matrix);

        assert(wanteditem > 0, 'Not a valid recipe');
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

    fn try_craft(ref self: ContractState, item: u16, x: u64, y: u64, z: u64) {
        if item == 0 {
            return try_inventory_craft(ref self);
        }

        let mut world = get_world(ref self);
        let player = get_caller_address();

        // Stone Craft
        if item == 34 || item == 36 || item == 38 || item == 40 || item == 42 {
            let player_data: PlayerData = world.read_model((player));
            let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
            let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
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

    #[abi(embed_v0)]
    impl ActionsImpl of IActions<ContractState> {
        fn spawn(ref self: ContractState) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            init(ref world, player);
        }

        fn sell(ref self: ContractState) {
            let world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);

            sell(ref self);
        }

        fn buy(ref self: ContractState, item_ids: Array<u16>, quantities: Array<u32>) {
            let world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);

            buy(ref self, item_ids, quantities);
        }

        // TICKET #2: New hit_block implementation using GameResult (unsafe wrapper)
        fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            println!("hit_block called - delegating to hit_block_internal");
            GameResultTrait::unwrap(hit_block_internal(ref world, player, x, y, z));
        }

        fn use_item(ref self: ContractState, x: u64, y: u64, z: u64) {
            let mut world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));

            let item_type: u16 = inventory.get_hotbar_selected_item_type();
            if item_type == 41 { // hoe
                update_block(ref world, x, y, z, item_type);
            } else if item_type == 43 { // hammer
                WorldStructureTrait::upgrade_structure(ref world, x, y, z);
            } else {
                assert(item_type > 0, 'Error: item id is zero');

                // Get player data to check current space
                let player_data: PlayerData = world.read_model((player));

                if item_type < 32 {
                    // Blocks can only be placed in main island (space_id == 1)
                    assert(player_data.current_space_id == 1, 'Error: not in main island');
                    // Actual blocks (grass, dirt, stone) can only be placed at ground level
                    if item_type <= 3 {
                        assert(z == 8192, 'Error: blocks only at z=0');
                    }
                    place_block(ref world, x, y, z, item_type);
                } else if item_type == 50 || (item_type >= 60 && item_type <= 64) {
                    // World structures (House Pattern, Workshop, Well, Kitchen, Warehouse, Brewery) only in main island
                    assert(player_data.current_space_id == 1, 'Error: not in main island');
                    WorldStructureTrait::place_structure(ref world, x, y, z, item_type);
                } else if item_type == 35 || item_type == 37 || item_type == 39 {
                    // Tools (Axe, Pickaxe, Shovel) - do nothing when used on empty space
                    // These tools are only used via hit_block function
                    return;
                } else {
                    // Other items (plants, seeds, etc.) can be placed anywhere
                    plant(ref self, x, y, z, item_type);
                }
            }
        }

        fn select_hotbar_slot(ref self: ContractState, slot: u8) {
            let mut world = get_world(ref self);

            let player = get_caller_address();
            let mut inventory: Inventory = world.read_model((player, 0));
            inventory.select_hotbar_slot(slot);
            world.write_model(@inventory);
        }

        fn craft(ref self: ContractState, item: u32, x: u64, y: u64, z: u64) {
            let world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);

            try_craft(ref self, item.try_into().unwrap(), x, y, z);
        }

        fn inventory_move_item(ref self: ContractState, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8) {
            assert(from_inventory != to_inventory_id || from_slot != to_slot, 'Error: same slot');
            let mut world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);
            let mut inventory: Inventory = world.read_model((player, from_inventory));
            let mut to_inventory: Inventory = world.read_model((player, to_inventory_id));

            if from_inventory == to_inventory_id {
                inventory.move_item(from_slot, to_slot);
            } else {
                inventory.move_item_between_inventories(from_slot, ref to_inventory, to_slot);
                world.write_model(@to_inventory);
            }
            world.write_model(@inventory);
        }

        fn generate_island_part(ref self: ContractState, x: u64, y: u64, z: u64, island_id: u16) {
            let mut world = get_world(ref self);
            let player = get_caller_address();

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

        fn visit(ref self: ContractState, space_id: u16) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            let mut player_data: PlayerData = world.read_model((player));
            player_data.current_space_id  = space_id;
            world.write_model(@player_data);
        }

        fn visit_new_island(ref self: ContractState) {
            let mut world = get_world(ref self);
            let player = get_caller_address();

            let mut player_data: PlayerData = world.read_model((player));
            let island_id = player_data.last_space_created_id + 1;
            player_data.last_space_created_id = island_id;
            world.write_model(@player_data);

            let seed: u64 = starknet::get_block_info().unbox().block_timestamp;

            let island_compositions: Array<u128> = array![1229782938247303441, 2459565876494606882, 3689348814741910323];

            let island_type = ((seed * 7) % island_compositions.len().into()).try_into().unwrap();
            let island_composition: u128 = *island_compositions.at(island_type);

            // Generate 4x4 grid of chunks for new island
            let player_felt = player.into();
            let base_shift: u128 = 0x000000080000000008000000000800;
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
                    world.write_model(@IslandChunkTrait::new(player_felt, island_id, base_shift + chunk_offset, 0, island_composition));
                    y += 1;
                };
                x += 1;
            };

            // Generate 10 random gatherable resources
            spawn_random_resources(ref world, player.into(), island_id, 0x000000080000000008000000000800, 0);

            self.visit(island_id);
        }

        fn start_processing(ref self: ContractState, process_type: u8, input_amount: u32) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            let current_time = starknet::get_block_timestamp();

            // Check if player is already locked
            let processing_lock: ProcessingLock = world.read_model(player);
            assert(processing_lock.unlock_time <= current_time, 'Already processing');

            // Convert u8 to ProcessType
            let process_type_enum = match process_type {
                0 => ProcessType::None,
                1 => ProcessType::GrindWheat,
                2 => ProcessType::CutLogs,
                3 => ProcessType::SmeltOre,
                4 => ProcessType::CrushStone,
                5 => ProcessType::ProcessClay,
                _ => panic!("Invalid process type")
            };

            assert(process_type_enum != ProcessType::None, 'Invalid process type');
            assert(input_amount > 0, 'Input amount must be positive');

            // Get processing config
            let config = get_processing_config(process_type_enum);

            // Get player inventories (hotbar and main inventory)
            let mut hotbar: Inventory = world.read_model((player, 0));
            let mut inventory: Inventory = world.read_model((player, 1));

            // Count available input items in both hotbar and inventory
            let hotbar_items = hotbar.get_item_amount(config.input_item.try_into().unwrap());
            let inventory_items = inventory.get_item_amount(config.input_item.try_into().unwrap());
            let available_items = hotbar_items + inventory_items;
            assert(available_items >= input_amount, 'Not enough input items');

            // Calculate batches to process based on input amount
            let batches_to_process = input_amount / config.input_amount;
            assert(batches_to_process > 0, 'Not enough for one batch');

            // Calculate processing time
            let total_time = batches_to_process.into() * config.time_per_batch;

            // Cap at max processing time
            let capped_time = if total_time > MAX_PROCESSING_TIME {
                MAX_PROCESSING_TIME
            } else {
                total_time
            };
            let actual_batches = (capped_time / config.time_per_batch).try_into().unwrap();

            // Use the exact input amount requested (we already verified it's available)
            let items_to_remove = input_amount;

            // Remove input items from hotbar first, then inventory
            let mut items_left_to_remove = items_to_remove;
            if hotbar_items > 0 {
                let remove_from_hotbar = if hotbar_items >= items_left_to_remove {
                    items_left_to_remove
                } else {
                    hotbar_items
                };
                hotbar.remove_items(config.input_item.try_into().unwrap(), remove_from_hotbar);
                items_left_to_remove -= remove_from_hotbar;
            }

            if items_left_to_remove > 0 {
                inventory.remove_items(config.input_item.try_into().unwrap(), items_left_to_remove);
            }

            // Write updated inventories (only removed input items, no output added yet)
            world.write_model(@hotbar);
            world.write_model(@inventory);

            // Set processing lock
            let new_lock = ProcessingLock {
                player,
                unlock_time: current_time + capped_time,
                process_type: process_type_enum,
                batches_processed: actual_batches
            };
            world.write_model(@new_lock);
        }

        fn cancel_processing(ref self: ContractState) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            let current_time = starknet::get_block_timestamp();

            // Get current lock
            let processing_lock: ProcessingLock = world.read_model(player);
            assert(processing_lock.unlock_time > 0, 'Not processing');

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
                assert(remaining_in_inventory == 0, 'Not enough space for output');
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
                assert(remaining_in_inventory == 0, 'Cannot return all input items');
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
        }

        fn execute_compressed_actions(ref self: ContractState, packed_actions: felt252) -> u8 {
            let mut world = get_world(ref self);
            let player = get_caller_address();

            // Check if player is processing
            ensure_not_locked(@world, player);

            // Debug print removed for Sepolia deployment

            // Unpack and execute up to 8 actions
            // With 30 bits per action, we can fit 8 actions in a felt252 (8 * 30 = 240 bits < 251 bits)
            let mut current_packed: u256 = packed_actions.into();
            let mut action_count: u8 = 0;
            let mut successful_count: u8 = 0;

            let action_size: u256 = 0x40000000; // 2^30 = 1073741824
            let coord_mask: u256 = 0x3FFF; // 14 bits = 16383
            let coord_divisor: u256 = 0x4000; // 2^14 = 16384

            loop {
                if action_count >= 8_u8 {
                    break;
                }

                // Check if there are more actions (if current_packed is 0, we're done)
                if current_packed == 0 {
                    break;
                }

                // Extract one action (30 bits)
                // Bit layout: [1 bit: action_type][1 bit: z][14 bits: x][14 bits: y]
                let action_data: u256 = current_packed & 0x3FFFFFFF; // 30 bits mask

                // Extract fields from the 30-bit action
                let y: u64 = (action_data & coord_mask).try_into().unwrap();
                let remaining = action_data / coord_divisor;

                let x: u64 = (remaining & coord_mask).try_into().unwrap();
                let remaining = remaining / coord_divisor;

                let z_bit: u64 = (remaining & 1).try_into().unwrap();
                let action_type: u64 = ((remaining / 2) & 1).try_into().unwrap();

                // Z is still offset (0 means 8192, 1 means 8193)
                // X and Y are already offset by client (no additional offset needed)
                let world_z = if z_bit == 0 { 8192 } else { 8193 };

                // Debug logging removed for Sepolia deployment

                // Try to execute the action (x and y already have proper offset)
                let success = if action_type == 0 {
                    // Use item (place)
                    try_use_item(ref world, player, x, y, world_z)
                } else {
                    // Hit block
                    try_hit_block(ref world, player, x, y, world_z)
                };

                if success {
                    successful_count += 1;
                }

                // Shift to next action
                current_packed = current_packed / action_size;
                action_count += 1;
            };

            // Return number of successful actions
            successful_count
        }

        fn execute_packed_actions(ref self: ContractState, packed_data: Array<felt252>) -> Array<bool> {

            let mut world = get_world(ref self);
            let player = get_caller_address();

            // Limit to prevent gas issues
            assert(packed_data.len() <= 10, 'Too many packed felts');

            let mut results: Array<bool> = array![];
            let mut felt_idx = 0;

            while felt_idx < packed_data.len() {
                let current_felt = *packed_data.at(felt_idx);
                let pack_type = extract_u8(current_felt, 0);

                if pack_type == 0 {
                    // Type 0: Packed PlaceUse/Hit actions (up to 8 per felt)
                    // [8 bits: pack_type=0][4 bits: count][30 bits: action1]...[30 bits: action8]
                    // Each action: [1 bit: action_type][14 bits: y][14 bits: x][1 bit: z]
                    let count_raw = extract_u8(current_felt, 8);
                    let count = count_raw - (count_raw / 16) * 16; // 4 bits

                    let mut data: u256 = current_felt.into() / 4096; // Skip header (12 bits)
                    let mut i: u8 = 0;

                    while i < count {
                        let action_raw = extract_u32(data.try_into().unwrap(), 0);
                        let action = action_raw - (action_raw / 0x40000000) * 0x40000000; // 30 bits
                        let action_type = action & 0x1; // 1 bit
                        let y: u64 = extract_u64(action.into(), 1, 14); // 14 bits
                        let x: u64 = extract_u64(action.into(), 15, 14); // 14 bits
                        let z: u64 = if extract_u8(action.into(), 29) == 0 { 8192 } else { 8193 }; // 1 bit at position 29

                        let success = if action_type == 0 {
                            try_use_item(ref world, player, x, y, z)
                        } else {
                            try_hit_block(ref world, player, x, y, z)
                        };

                        results.append(success);
                        data = data / 0x40000000; // Shift 30 bits
                        i += 1;
                    }
                } else if pack_type == 1 {
                    // Type 1: Packed MoveItem actions (up to 6 per felt)
                    // [8 bits: pack_type=1][4 bits: count][40 bits: move1]...[40 bits: move6]
                    let count_raw = extract_u8(current_felt, 8);
                    let count = count_raw - (count_raw / 16) * 16; // 4 bits
                    let mut data: u256 = current_felt.into() / 4096;
                    let mut i: u8 = 0;

                    while i < count {
                        let from_inv: u16 = extract_u8(data.try_into().unwrap(), 0).into(); // 8 bits
                        let from_slot = extract_u8(data.try_into().unwrap(), 8); // 8 bits
                        let to_inv: u16 = extract_u8(data.try_into().unwrap(), 16).into(); // 8 bits
                        let to_slot = extract_u8(data.try_into().unwrap(), 24); // 8 bits

                        let success = try_move_item(ref world, player, from_inv, from_slot, to_inv, to_slot);
                        results.append(success);

                        data = data / 0x10000000000; // Shift 40 bits
                        i += 1;
                    }
                } else if pack_type == 2 {
                    // Type 2: Mixed simple actions
                    // [8 bits: pack_type=2][4 bits: count][variable data per action]
                    let count_raw = extract_u8(current_felt, 8);
                    let count = count_raw - (count_raw / 16) * 16; // 4 bits
                    let mut data: u256 = current_felt.into() / 4096;
                    let mut i: u8 = 0;

                    while i < count {
                        let action_type = extract_u8(data.try_into().unwrap(), 0);
                        data = data / 256;

                        let success = if action_type == 0 {
                            // PlaceUse: [32 bits: position]
                            // Position: [14 bits: y][14 bits: x][1 bit: z][3 bits: padding]
                            let position_raw = extract_u32(data.try_into().unwrap(), 0);
                            let position_data = position_raw; // Use full 32 bits, no mask needed
                            data = data / 0x100000000; // Skip 32 bits total

                            let z_bit = (position_data / 0x10000000) & 0x1; // Bit 28 in 30-bit data
                            let x = (position_data / 0x4000) & 0x3FFF; // Bits 14-27
                            let y = position_data & 0x3FFF; // Bits 0-13
                            let world_z = if z_bit == 0 { 8192 } else { 8193 };

                            try_use_item(ref world, player, x.into(), y.into(), world_z)
                        } else if action_type == 1 {
                            // Hit: [32 bits: position]
                            let position_raw = extract_u32(data.try_into().unwrap(), 0);
                            let position_data = position_raw; // Use full 32 bits, no mask needed
                            data = data / 0x100000000; // Skip 32 bits total

                            let z_bit = (position_data / 0x10000000) & 0x1; // Bit 28 in 30-bit data
                            let x = (position_data / 0x4000) & 0x3FFF; // Bits 14-27
                            let y = position_data & 0x3FFF; // Bits 0-13
                            let world_z = if z_bit == 0 { 8192 } else { 8193 };

                            try_hit_block(ref world, player, x.into(), y.into(), world_z)
                        } else if action_type == 2 {
                            // SelectHotbar: [8 bits: slot]
                            let slot = extract_u8(data.try_into().unwrap(), 0);
                            data = data / 256;
                            try_select_hotbar(ref world, player, slot)
                        } else if action_type == 6 {
                            // Sell: no additional data
                            try_sell(ref world, player)
                        } else if action_type == 8 {
                            // CancelProcessing: no additional data
                            try_cancel_processing(ref world, player)
                        } else if action_type == 9 {
                            // Visit: [16 bits: space_id]
                            let space_id = extract_u16(data.try_into().unwrap(), 0);
                            data = data / 0x10000;
                            try_visit(ref world, player, space_id)
                        } else if action_type == 10 {
                            // VisitNewIsland: no additional data
                            try_visit_new_island(ref world, player)
                        } else {
                            false
                        };

                        results.append(success);
                        i += 1;
                    }
                } else if pack_type == 3 {
                    // Type 3: Single large action
                    // [8 bits: pack_type=3][8 bits: action_type][remaining bits: action data]
                    let action_type = extract_u8(current_felt, 8);
                    let data: u256 = current_felt.into() / 0x10000;

                    let success = if action_type == 3 {
                        // Craft: [32 bits: item_id][30 bits: position]
                        // Position: [14 bits: y][14 bits: x][2 bits: z and reserved]
                        let item_id = extract_u32(data.try_into().unwrap(), 0);
                        let y: u64 = extract_u64(data.try_into().unwrap(), 32, 14);
                        let x: u64 = extract_u64(data.try_into().unwrap(), 46, 14);
                        let z: u64 = if extract_u8(data.try_into().unwrap(), 60) == 0 { 8192 } else { 8193 };
                        try_craft_helper(ref world, player, item_id, x, y, z)
                    } else if action_type == 5 {
                        // Buy: [16 bits: item_id][32 bits: quantity]
                        let item_id = extract_u16(data.try_into().unwrap(), 0);
                        let quantity = extract_u32(data.try_into().unwrap(), 16);
                        try_buy_single(ref world, player, item_id, quantity)
                    } else if action_type == 7 {
                        // StartProcessing: [8 bits: process_type][32 bits: amount]
                        let process_type: u8 = (data & 0xFF).try_into().unwrap();
                        let amount: u32 = ((data / 0x100) & 0xFFFFFFFF).try_into().unwrap();
                        try_start_processing(ref world, player, process_type, amount)
                    } else if action_type == 11 {
                        // GenerateIsland: [30 bits: position][16 bits: island_id]
                        // Position: [14 bits: y][14 bits: x][2 bits: z and reserved]
                        let coords_raw = extract_u32(data.try_into().unwrap(), 0);
                        let coords = coords_raw - (coords_raw / 0x40000000) * 0x40000000; // 30 bits
                        let y: u64 = extract_u64(coords.into(), 0, 14);
                        let x: u64 = extract_u64(coords.into(), 14, 14);
                        let z: u64 = if extract_u8(coords.into(), 28) == 0 { 8192 } else { 8193 };
                        let island_id = extract_u16(data.try_into().unwrap(), 30);
                        try_generate_island(ref world, player, x, y, z, island_id)
                    } else {
                        false
                    };

                    results.append(success);
                } else {
                    // Unknown pack type, skip
                    results.append(false);
                }

                felt_idx += 1;
            };


            results
        }

        // Debug function to give 50 free coins
        fn debug_give_coins(ref self: ContractState) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            
            // Get current player data
            let mut player_data: PlayerData = world.read_model((player));
            
            // Add 50 coins
            player_data.coins += 50;
            
            // Save updated player data
            world.write_model(@player_data);
        }

        // Set player name
        fn set_name(ref self: ContractState, name: ByteArray) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            
            // Log the set_name call
            println!("set_name called by player: {:?}, name: {:?}", player, name);
            
            // Get current player data
            let mut player_data: PlayerData = world.read_model((player));
            
            // Set the name
            player_data.name = name;
            
            // Save updated player data
            world.write_model(@player_data);
        }

        // TICKET #2: Demo function showing GameResult pattern for hit_block migration
        fn test_hit_block_migration_demo(ref self: ContractState) -> bool {
            println!("TICKET #2 DEMO: Testing hit_block migration pattern");
            
            // Simulate the internal logic returning GameResult
            let success_case: GameResult<()> = GameResult::Ok(());
            let player_locked_case: GameResult<()> = GameResult::Err(GameError::PlayerLocked(123456));
            let need_shovel_case: GameResult<()> = GameResult::Err(GameError::InsufficientItems((39, 1)));
            
            println!("Testing success case:");
            let success_result = GameResultTrait::is_ok(@success_case);
            println!("  Success case result: {}", success_result);
            
            println!("Testing player locked error:");
            let locked_result = GameResultTrait::is_err(@player_locked_case);
            println!("  Player locked error result: {}", locked_result);
            
            println!("Testing need shovel error:");
            let shovel_result = GameResultTrait::is_err(@need_shovel_case);
            println!("  Need shovel error result: {}", shovel_result);
            
            // Test unwrap (unsafe wrapper pattern)
            println!("Testing unwrap pattern (unsafe wrapper):");
            GameResultTrait::unwrap(GameResult::Ok(())); // Should succeed
            println!("  Unwrap succeeded for Ok case");
            
            // Test safe wrapper pattern (return bool)
            let safe_wrapper_result = success_result && locked_result && shovel_result;
            println!("TICKET #2 DEMO: All tests passed = {}", safe_wrapper_result);
            
            safe_wrapper_result
        }
    }
