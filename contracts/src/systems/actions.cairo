#[starknet::interface]
trait IActions<T> {
    fn spawn(ref self: T);
    fn buy(ref self: T, item_ids: Array<u16>, quantities: Array<u32>);
    fn sell(ref self: T);
    fn hit_block(ref self: T, x: u64, y: u64, z: u64, hp: u32);
    fn use_item(ref self: T, x: u64, y: u64, z: u64);
    fn select_hotbar_slot(ref self: T, slot: u8);
    fn craft(ref self: T, item: u32, x: u64, y:u64, z: u64);
    fn inventory_move_item(ref self: T, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8);
    fn generate_island_part(ref self: T, x: u64, y: u64, z: u64, island_id: u16);
    fn visit(ref self: T, space_id: u16);
    fn visit_new_island(ref self: T);
    fn start_processing(ref self: T, process_type: u8, input_amount: u32);
    fn cancel_processing(ref self: T);
}

// dojo decorator
#[dojo::contract]
mod actions {
    use super::{IActions};
    use starknet::{get_caller_address};
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

    use dojo::model::{ModelStorage};

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
            if hotbar.get_hotbar_selected_item_type() == 33 {
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

        fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64, hp: u32) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            
            // Check if player is processing
            ensure_not_locked(@world, player);

            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let itemType: u16 = inventory.get_hotbar_selected_item_type();

            if (itemType == 18) {
                // hoe, transform grass to dirt
                update_block(ref world, x, y, z - 1, itemType);
                return;
            }

            // Check if trying to break blocks 1, 2, or 3 (dirt, grass, stone)
            let player_data: PlayerData = world.read_model((player));
            let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
            let chunk: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id));

            // Get block ID at position
            let x_local = x % 4;
            let y_local = y % 4;
            let z_local = z % 2;
            let lower_layer = z % 4 < 2;
            let blocks = if lower_layer { chunk.blocks2 } else { chunk.blocks1 };
            let shift: u128 = fast_power_2(((x_local + y_local * 4 + z_local * 16) * 4).into()).into();
            let block_id: u16 = ((blocks / shift) % 16).try_into().unwrap();

            // If block is dirt (1), grass (2), or stone (3), require shovel (39)
            if (block_id == 1 || block_id == 2 || block_id == 3) && itemType != 39 {
                assert!(false, "Error: Need a shovel to break this block");
            }

            // handle hp
            if !remove_block(ref world, x, y, z) {
                harvest(ref self, x, y, z);
            }
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
    }

    fn spawn_random_resources(ref world: dojo::world::storage::WorldStorage, player: felt252, island_id: u16, shift: u128, island_type: u32) {
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
}
