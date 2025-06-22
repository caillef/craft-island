#[starknet::interface]
trait IActions<T> {
    fn spawn(ref self: T);
    //fn buy(ref self: T, item_id: u32, quantity: u32);
    //fn sell(ref self: T, item_id: u32, quantity: u32);
    fn hit_block(ref self: T, x: u64, y: u64, z: u64, hp: u32);
    fn use_item(ref self: T, x: u64, y: u64, z: u64);
    fn select_hotbar_slot(ref self: T, slot: u8);
    fn craft(ref self: T, item: u32, x: u64, y:u64, z: u64);
    fn inventory_move_item(ref self: T, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8);
    fn generate_island_part(ref self: T, x: u64, y: u64, z: u64, island_id: u16);
    fn visit(ref self: T, space_id: u16);
    fn visit_new_island(ref self: T);
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
        GatherableResource, GatherableResourceImpl
    };
    use craft_island_pocket::models::inventory::{
        Inventory, InventoryTrait
    };
    use craft_island_pocket::models::islandchunk::{
        place_block, remove_block, update_block, IslandChunkTrait
    };
    use craft_island_pocket::models::worldstructure::{WorldStructureTrait};
    use craft_island_pocket::helpers::{
        utils::{get_position_id},
        craft::{craftmatch},
        init::{init},
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
        println!("next harvest {}, timestamp {}", resource.next_harvest_at, timestamp);
        assert!(resource.next_harvest_at <= timestamp, "Error: Can't harvest now");
        let mut inventory: Inventory = world.read_model((player, 0));
        let tool: u16 = inventory.get_hotbar_selected_item_type();
        if resource.resource_id == 46 { // sapling
            assert!(tool == 35, "Error: Need an axe");
        }
        println!("Harvested {}", resource.resource_id);
        resource.harvested_at = timestamp;
        resource.remained_harvest -= 1;

        let mut item_id = resource.resource_id;
        if item_id == 49 { // Boulder
            item_id = 33; // Rock
        }

        InventoryTrait::add_to_player_inventories(ref world, player.into(), item_id, 1);
        if item_id == 47 { // Wheat seed
            InventoryTrait::add_to_player_inventories(ref world, player.into(), 48, 1);
        }
        if item_id == 46 { // Oak Tree
            InventoryTrait::add_to_player_inventories(ref world, player.into(), 44, 1);
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
        ref self: ContractState, item_id: u32, quantity: u32
    ) { //let player = get_caller_address();
    }

    fn sell(
        ref self: ContractState, item_id: u32, quantity: u32
    ) { //let player = get_caller_address();
    }

    fn try_inventory_craft(ref self: ContractState) {
        let mut world = get_world(ref self);
        let player = get_caller_address();

        let mut craftinventory: Inventory = world.read_model((player, 2));
        // mask to remove quantity
        let craft_matrix: u256 = (craftinventory.slots1.into() & 7229938216006767371223902296383078621116345691456360212366842288707796205568);
        println!("Craft matrix {}", craft_matrix);
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
        println!("Crafting {}", item);
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

        fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64, hp: u32) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let itemType: u16 = inventory.get_hotbar_selected_item_type();

            if (itemType == 18) {
                // hoe, transform grass to dirt
                update_block(ref world, x, y, z - 1, itemType);
                return;
            }
            // handle hp
            if !remove_block(ref world, x, y, z) {
                harvest(ref self, x, y, z);
            }
        }

        fn use_item(ref self: ContractState, x: u64, y: u64, z: u64) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let item_type: u16 = inventory.get_hotbar_selected_item_type();

            println!("Hotbar selected item {}", item_type);
            if item_type == 41 { // hoe
                update_block(ref world, x, y, z, item_type);
            } else if item_type == 43 { // hammer
                WorldStructureTrait::upgrade_structure(ref world, x, y, z);
            } else {
                assert(item_type > 0, 'Error: item id is zero');
                if item_type < 32 {
                    place_block(ref world, x, y, z, item_type);
                } else if item_type == 50 {
                    WorldStructureTrait::place_structure(ref world, x, y, z, item_type);
                } else {
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
            try_craft(ref self, item.try_into().unwrap(), x, y, z);
        }

        fn inventory_move_item(ref self: ContractState, from_inventory: u16, from_slot: u8, to_inventory_id: u16, to_slot: u8) {
            assert(from_inventory != to_inventory_id || from_slot != to_slot, 'Error: same slot');
            let mut world = get_world(ref self);

            let player = get_caller_address();
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

            let shift: u128 = get_position_id(x, y, z);

            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000100000008000000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000200000008000000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000300000008000000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000000000008010000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000100000008010000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000200000008010000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000300000008010000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000000000008020000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000100000008020000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000200000008020000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000300000008020000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000000000008030000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000100000008030000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000200000008030000000000, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, shift + 0x000000000300000008030000000000, 0, 1229782938247303441));
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

            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080000000008000000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080100000008000000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080200000008000000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080300000008000000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080000000008010000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080100000008010000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080200000008010000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080300000008010000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080000000008020000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080100000008020000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080200000008020000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080300000008020000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080000000008030000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080100000008030000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080200000008030000000800, 0, 1229782938247303441));
            world.write_model(@IslandChunkTrait::new(player.into(), island_id, 0x000000080300000008030000000800, 0, 1229782938247303441));

            self.visit(island_id);
        }
    }
}
