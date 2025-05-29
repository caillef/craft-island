use craft_island_pocket::helpers::math::{fast_power_2};

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
}

// dojo decorator
#[dojo::contract]
mod actions {
    use super::{IActions, get_position_id};
    use starknet::{get_caller_address, ContractAddress};
    use craft_island_pocket::models::common::{
        GatherableResource, PlayerData
    };
    use craft_island_pocket::models::inventory::{
        Inventory, InventoryTrait
    };
    use craft_island_pocket::models::islandchunk::{
        IslandChunk, IslandChunkTrait
    };
    use craft_island_pocket::models::worldstructure::{
        WorldStructure
    };
    use craft_island_pocket::helpers::{
        craft::{craftmatch}
    };

    use dojo::model::{ModelStorage};

    fn get_world(ref self: ContractState) -> dojo::world::storage::WorldStorage {
        self.world(@"craft_island_pocket")
    }

    fn place_structure(ref self: ContractState, x: u64, y: u64, z: u64, item_id: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut structure: WorldStructure = world.read_model((island_id, chunk_id, position));
        assert!(structure.structure_type == 0, "Error: World Structure exists");
        structure.structure_type = 30; // House

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.remove_items(item_id, 1);
        world.write_model(@inventory);

        let mut player_data: PlayerData = world.read_model((player));
        assert!(player_data.last_inventory_created_id > 0, "Player not init");
        player_data.last_inventory_created_id += 1;
        player_data.last_space_created_id += 1;
        structure.build_inventory_id = player_data.last_inventory_created_id;

        let mut building_inventory: Inventory = InventoryTrait::new(player_data.last_inventory_created_id, 4, 9, player);
        world.write_model(@building_inventory);

        structure.completed = false;
        structure.linked_space_owner = player.into();
        structure.linked_space_id = player_data.last_space_created_id;
        structure.destroyed = false;
        world.write_model(@player_data);
        world.write_model(@structure);
    }

    fn plant(ref self: ContractState, x: u64, y: u64, z: u64, item_id: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut resource: GatherableResource = world.read_model((island_id, chunk_id, position));
        assert!(resource.resource_id == 0, "Error: Resource exists");
        resource.resource_id = item_id;

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.remove_items(item_id, 1);
        world.write_model(@inventory);

        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        resource.planted_at = timestamp;
        if item_id == 32 || item_id == 33 {
            resource.next_harvest_at = 0; // instant
        } else {
            resource.next_harvest_at = timestamp + 10;
        }
        resource.harvested_at = 0;
        resource.max_harvest = 1;
        resource.remained_harvest = resource.max_harvest;
        resource.destroyed = false;
        world.write_model(@resource);
    }

    fn harvest(ref self: ContractState, x: u64, y: u64, z: u64) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut resource: GatherableResource = world.read_model((island_id, chunk_id, position));
        assert!(resource.resource_id > 0 && !resource.destroyed, "Error: Resource does not exists");
        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        println!("next harvest {}, timestamp {}", resource.next_harvest_at, timestamp);
        assert!(resource.next_harvest_at <= timestamp, "Error: Can\'t harvest now");
        println!("Harvested {}", resource.resource_id);
        resource.harvested_at = timestamp;
        resource.remained_harvest -= 1;

        let mut item_id = resource.resource_id;
        if item_id == 49 { // Boulder
            item_id = 33; // Rock
        }
        if item_id == 46 { // Oak Tree
            item_id = 44; // Oak log
        }
        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.add_items(item_id, 1);
        world.write_model(@inventory);

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

    fn place_block(ref self: ContractState, x: u64, y: u64, z: u64, block_id: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        println!("Raw Position {} {} {}", x, y, z);
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((player_data.current_island_owner, player_data.current_island_id, chunk_id));

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.remove_items(block_id.try_into().unwrap(), 1);
        chunk.place_block(x, y, z, block_id);
        world.write_model(@inventory);
        world.write_model(@chunk);
    }

    fn remove_block(ref self: ContractState, x: u64, y: u64, z: u64) -> bool {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((player_data.current_island_owner, player_data.current_island_id, chunk_id));
        let mut block_id = chunk.remove_block(x, y, z);
        if block_id == 0 {
            return false;
        }

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.add_items(block_id.try_into().unwrap(), 1);
        world.write_model(@inventory);
        world.write_model(@chunk);
        return true;
    }

    fn update_block(ref self: ContractState, x: u64, y: u64, z: u64, tool: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        println!("Raw Position {} {} {}", x, y, z);
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((player_data.current_island_owner, player_data.current_island_id, chunk_id));
        chunk.update_block(x, y, z, tool);
        world.write_model(@chunk);
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

        let mut hotbar: Inventory = world.read_model((player, 0));
        hotbar.add_items(wanteditem, 1);

        world.write_model(@hotbar);
        world.write_model(@craftinventory);
    }

    fn try_craft(ref self: ContractState, item: u16, x: u64, y: u64, z: u64) {
        println!("Crafting {}", item);
        if item == 0 {
            return try_inventory_craft(ref self);
        }

        let mut world = get_world(ref self);
        let player = get_caller_address();

        let mut inventory: Inventory = world.read_model((player, 0));
        // Stone Craft
        if item == 34 || item == 36 || item == 38 || item == 40 || item == 42 {
            let island_id: felt252 = player.into();
            let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
            let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
            let mut resource: GatherableResource = world.read_model((island_id, chunk_id, position));
            assert!(resource.resource_id == 33 && !resource.destroyed, "Error: No rock");
            resource.destroyed = true;
            resource.resource_id = 0;
            world.write_model(@resource);

            if inventory.get_hotbar_selected_item_type() == 33 {
                inventory.remove_items(33, 1);
                inventory.add_items(item, 1);
            }
        }
        world.write_model(@inventory);
    }

    fn init_island(ref self: ContractState, player: ContractAddress) {
        let mut world = get_world(ref self);

        let starter_chunk0 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080100000007ff0000000800,
            version: 0,
            blocks1: 0,
            blocks2: 76842741656518657,
        };
        world.write_model(@starter_chunk0);

        let starter_chunk1 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080100000008000000000800,
            version: 0,
            blocks1: 0,
            blocks2: 4785147619639313,
        };
        world.write_model(@starter_chunk1);

        let starter_chunk2 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080100000008010000000800,
            version: 0,
            blocks1: 0,
            blocks2: 281547992268817,
        };
        world.write_model(@starter_chunk2);

        let starter_chunk3 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008000000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1229782938247303441,
        };
        world.write_model(@starter_chunk3);

        let starter_chunk4 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008010000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1229782938247303441,
        };
        world.write_model(@starter_chunk4);

        let starter_chunk5 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008020000000800,
            version: 0,
            blocks1: 0,
            blocks2: 4296085777,
        };
        world.write_model(@starter_chunk5);

        let starter_chunk6 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000007ff0000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1229782938247303440,
        };
        world.write_model(@starter_chunk6);

        let starter_chunk7 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007ff00000008000000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1229782938247303441,
        };
        world.write_model(@starter_chunk7);

        let starter_chunk8 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007ff00000008010000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1234568085865828625,
        };
        world.write_model(@starter_chunk8);

        let starter_chunk9 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007ff00000008020000000800,
            version: 0,
            blocks1: 0,
            blocks2: 18760703480098,
        };
        world.write_model(@starter_chunk9);

        let starter_chunk10 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007ff00000007ff0000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1224997790610882560,
        };
        world.write_model(@starter_chunk10);

        let starter_chunk11 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007fe00000008000000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1152939097061326848,
        };
        world.write_model(@starter_chunk11);

        let starter_chunk12 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007fe00000008010000000800,
            version: 0,
            blocks1: 0,
            blocks2: 1224997790627664128,
        };
        world.write_model(@starter_chunk12);

        let starter_chunk13 = IslandChunk {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x00000007fe00000008020000000800,
            version: 0,
            blocks1: 0,
            blocks2: 268439552,
        };
        world.write_model(@starter_chunk13);

        let resource0 = GatherableResource {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008000000000800,
            position: 17,
            resource_id: 32,
            planted_at: 0,
            next_harvest_at: 0,
            harvested_at: 0,
            max_harvest: 1,
            remained_harvest: 1,
            destroyed: false,
        };
        world.write_model(@resource0);

        let resource1 = GatherableResource {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008000000000800,
            position: 23,
            resource_id: 33,
            planted_at: 0,
            next_harvest_at: 0,
            harvested_at: 0,
            max_harvest: 1,
            remained_harvest: 1,
            destroyed: false,
        };
        world.write_model(@resource1);

        let resource2 = GatherableResource {
            island_owner: player.into(),
            island_id: 1,
            chunk_id: 0x000000080000000008010000000800,
            position: 16,
            resource_id: 49,
            planted_at: 0,
            next_harvest_at: 0,
            harvested_at: 0,
            max_harvest: 255,
            remained_harvest: 255,
            destroyed: false,
        };
        world.write_model(@resource2);
    }

    fn init_player(ref self: ContractState, player: ContractAddress) {
        let mut world = get_world(ref self);

        let mut player_data: PlayerData = PlayerData {
            player: player,
            last_inventory_created_id: 2, // hotbar, inventory, craft
            last_space_created_id: 1, // own island
            current_island_owner: player.into(),
            current_island_id: 1,
        };
        world.write_model(@player_data);
    }

    fn init_inventory(ref self: ContractState, player: ContractAddress) {
        let mut world = get_world(ref self);

        let mut hotbar: Inventory = InventoryTrait::new(0, 0, 9, player);
        hotbar.add_items(1, 19);
        hotbar.add_items(2, 23);
        hotbar.add_items(46, 8);
        hotbar.add_items(47, 12);
        hotbar.add_items(41, 1);
        hotbar.add_items(32, 4);
        hotbar.add_items(33, 7);
        world.write_model(@hotbar);

        let mut inventory: Inventory = InventoryTrait::new(1, 1, 27, player);
        inventory.add_items(1, 19);
        inventory.add_items(2, 23);
        inventory.add_item(10, 32, 4);
        inventory.add_items(46, 8);
        inventory.add_items(47, 12);
        inventory.add_items(41, 1);
        inventory.add_items(32, 4);
        inventory.add_items(33, 7);
        inventory.add_items(36, 1);
        inventory.add_items(38, 1);
        world.write_model(@inventory);

        let mut craft: Inventory = InventoryTrait::new(2, 2, 9, player);
        world.write_model(@craft);
    }

    #[abi(embed_v0)]
    impl ActionsImpl of IActions<ContractState> {
        fn spawn(ref self: ContractState) {
            let player = get_caller_address();
            init_player(ref self, player);
            init_island(ref self, player);
            init_inventory(ref self, player);
        }

        fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64, hp: u32) {
            let world = get_world(ref self);
            let player = get_caller_address();
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let itemType: u16 = inventory.get_hotbar_selected_item_type();

            if (itemType == 18) {
                // hoe, transform grass to dirt
                update_block(ref self, x, y, z - 1, itemType);
                return;
            }
            // handle hp
            if !remove_block(ref self, x, y, z) {
                harvest(ref self, x, y, z);
            }
        }

        fn use_item(ref self: ContractState, x: u64, y: u64, z: u64) {
            let world = get_world(ref self);
            let player = get_caller_address();
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let item_type: u16 = inventory.get_hotbar_selected_item_type();

            println!("Hotbar selected item {}", item_type);
            if (item_type == 41) {
                // hoe, transform grass to dirt
                update_block(ref self, x, y, z, item_type);
            } else {
                assert(item_type > 0, 'Error: item id is zero');
                if item_type < 32 {
                    place_block(ref self, x, y, z, item_type);
                } else if item_type == 50 {
                    place_structure(ref self, x, y, z, item_type);
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
    }
}

fn get_position_id(x: u64, y: u64, z: u64) -> u128 {
    x.into() * fast_power_2(80) + y.into() * fast_power_2(40) + z.into()
}
