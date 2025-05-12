use craft_island_pocket::helpers::math::{fast_power_2};

#[starknet::interface]
trait IActions<T> {
    fn spawn(ref self: T);
    //fn buy(ref self: T, item_id: u32, quantity: u32);
    //fn sell(ref self: T, item_id: u32, quantity: u32);
    fn hit_block(ref self: T, x: u64, y: u64, z: u64, hp: u32);
    fn use_item(ref self: T, x: u64, y: u64, z: u64);
    fn select_hotbar_slot(ref self: T, slot: u8);
    fn craft(ref self: T, item: u32);
}

// dojo decorator
#[dojo::contract]
mod actions {
    use super::{IActions, get_position_id, get_block_at_pos};
    use starknet::{get_caller_address, ContractAddress};
    use craft_island_pocket::models::common::{
        IslandChunk, GatherableResource
    };
    use craft_island_pocket::models::inventory::{
        Inventory, InventoryTrait
    };
    use craft_island_pocket::helpers::math::{fast_power_2};

    use dojo::model::{ModelStorage};

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
        assert!(resource.next_harvest_at <= timestamp, "Error: Can\'t harvest now");
        println!("Harvested {}", resource.resource_id);
        resource.harvested_at = timestamp;
        resource.remained_harvest -= 1;

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.add_items(resource.resource_id.try_into().unwrap(), 1);
        world.write_model(@inventory);

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
        let island_id: felt252 = player.into();
        println!("Raw Position {} {} {}", x, y, z);
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((island_id, chunk_id));

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.remove_items(block_id.try_into().unwrap(), 1);
        world.write_model(@inventory);

        //println!("Adding block: {} {}", chunk.blocks1, chunk.blocks2);
        if z % 4 < 2 {
            let block_info = get_block_at_pos(chunk.blocks2, x % 4, y % 4, z % 2);
            assert(block_info == 0, 'Error: block exists');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            chunk.blocks2 += block_id.into() * shift.into();

            // If grass under, replace with dirt
            if z % 2 == 1 {
                let block_under_info = get_block_at_pos(chunk.blocks2, x % 4, y % 4, z % 2 - 1);
                if block_under_info == 1 {
                    let shift: u128 = fast_power_2(
                        (((x % 4) + (y % 4) * 4 + (z % 2 - 1) * 16) * 4).into()
                    )
                        .into();
                    chunk.blocks2 -= block_under_info.into() * shift.into();
                    chunk.blocks2 += 2 * shift.into();
                }
            } else {}
        } else {
            let block_info = get_block_at_pos(chunk.blocks1, x % 4, y % 4, z % 2);
            assert(block_info == 0, 'Error: block exists');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            chunk.blocks1 += block_id.into() * shift.into();

            // If grass under, replace with dirt
            if z % 2 == 1 {
                let block_under_info = get_block_at_pos(chunk.blocks1, x % 4, y % 4, z % 2 - 1);
                if block_under_info == 1 {
                    let shift: u128 = fast_power_2(
                        (((x % 4) + (y % 4) * 4 + (z % 2 - 1) * 16) * 4).into()
                    )
                        .into();
                    chunk.blocks1 -= block_under_info.into() * shift.into();
                    chunk.blocks1 += 2 * shift.into();
                }
            } else {}
        }
        //println!("Added block: {} {}", chunk.blocks1, chunk.blocks2);
        world.write_model(@chunk);
        // Remove block from inventory
    }

    fn remove_block(ref self: ContractState, x: u64, y: u64, z: u64) -> bool {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((island_id, chunk_id));
        let mut block_id = 0;
        //println!("Removing block: {} {}", chunk.blocks1, chunk.blocks2);
        if z % 4 < 2 {
            let block_info = get_block_at_pos(chunk.blocks2, x % 4, y % 4, z % 2);
            if block_info == 0 {
                return false;
            }
            let index: u128 = (((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into();
            let mut shift: u128 = fast_power_2(index).into();
            chunk.blocks2 -= (block_info.into() * shift.into());
            block_id = block_info;
        } else {
            let block_info = get_block_at_pos(chunk.blocks1, x % 4, y % 4, z % 2);
            if block_info == 0 {
                return false;
            }
            let index: u128 = (((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into();
            let mut shift: u128 = fast_power_2(index).into();
            chunk.blocks1 -= (block_info.into() * shift.into());
            block_id = block_info;
        }
        //println!("Removed block: {} {}", chunk.blocks1, chunk.blocks2);

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.add_items(block_id.try_into().unwrap(), 1);
        world.write_model(@inventory);

        world.write_model(@chunk);
        return true;
        // Add block to inventory
    }

    fn update_block(ref self: ContractState, x: u64, y: u64, z: u64, tool: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        println!("Raw Position {} {} {}", x, y, z);
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((island_id, chunk_id));

        //println!("Adding block: {} {}", chunk.blocks1, chunk.blocks2);
        if z % 4 < 2 {
            let block_info = get_block_at_pos(chunk.blocks2, x % 4, y % 4, z % 2);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 41 && block_info == 1 {
                chunk.blocks2 -= block_info.into() * shift.into();
                chunk.blocks2 += 2 * shift.into();
            }
        } else {
            let block_info = get_block_at_pos(chunk.blocks1, x % 4, y % 4, z % 2);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 18 && block_info == 1 {
                chunk.blocks1 -= block_info.into() * shift.into();
                chunk.blocks1 += 2 * shift.into();
            }
        }
        world.write_model(@chunk);
        // Remove block from inventory
    }

    fn try_craft(ref self: ContractState, item: u16) {
        let mut world = get_world(ref self);
        let player = get_caller_address();

        let mut inventory: Inventory = world.read_model((player, 0));

        if item == 34 || item == 36 || item == 38 || item == 40 || item == 42 {
            if inventory.get_item_amount(33) >= 2 {
                inventory.remove_items(33, 2);
                inventory.add_items(item, 1);
            }
        }
        world.write_model(@inventory);
    }

    fn init_island(ref self: ContractState, player: ContractAddress) {
        let mut world = get_world(ref self);

    let starter_chunk0 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080100000007ff0000000800,
          version: 0,
          blocks1: 0,
          blocks2: 76842741656518657,
      };
      world.write_model(@starter_chunk0);

    let starter_chunk1 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080100000008000000000800,
          version: 0,
          blocks1: 0,
          blocks2: 4785147619639313,
      };
      world.write_model(@starter_chunk1);

    let starter_chunk2 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080100000008010000000800,
          version: 0,
          blocks1: 0,
          blocks2: 281547992268817,
      };
      world.write_model(@starter_chunk2);

    let starter_chunk3 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080000000008000000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1229782938247303441,
      };
      world.write_model(@starter_chunk3);

    let starter_chunk4 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080000000008010000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1229782938247303441,
      };
      world.write_model(@starter_chunk4);

    let starter_chunk5 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080000000008020000000800,
          version: 0,
          blocks1: 0,
          blocks2: 4296085777,
      };
      world.write_model(@starter_chunk5);

    let starter_chunk6 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x000000080000000007ff0000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1229782938247303440,
      };
      world.write_model(@starter_chunk6);

    let starter_chunk7 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007ff00000008000000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1229782938247303441,
      };
      world.write_model(@starter_chunk7);

    let starter_chunk8 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007ff00000008010000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1234568085865828625,
      };
      world.write_model(@starter_chunk8);

    let starter_chunk9 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007ff00000008020000000800,
          version: 0,
          blocks1: 0,
          blocks2: 18760703480098,
      };
      world.write_model(@starter_chunk9);

    let starter_chunk10 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007ff00000007ff0000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1224997790610882560,
      };
      world.write_model(@starter_chunk10);

    let starter_chunk11 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007fe00000008000000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1152939097061326848,
      };
      world.write_model(@starter_chunk11);

    let starter_chunk12 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007fe00000008010000000800,
          version: 0,
          blocks1: 0,
          blocks2: 1224997790627664128,
      };
      world.write_model(@starter_chunk12);

    let starter_chunk13 = IslandChunk {
          island_id: player.into(),
          chunk_id: 0x00000007fe00000008020000000800,
          version: 0,
          blocks1: 0,
          blocks2: 268439552,
      };
      world.write_model(@starter_chunk13);

    let resource0 = GatherableResource {
        island_id: player.into(),
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

    let resource0 = GatherableResource {
        island_id: player.into(),
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
    world.write_model(@resource0);
    }

    fn get_world(ref self: ContractState) -> dojo::world::storage::WorldStorage {
        self.world(@"craft_island_pocket")
    }

    fn init_inventory(ref self: ContractState, player: ContractAddress) {
        let mut world = get_world(ref self);

        let mut inventory: Inventory = InventoryTrait::new(0, 9, player);
        inventory.add_items(1, 19);
        //inventory.add_items(2, 23);
        //inventory.add_items(46, 8);
        //inventory.add_items(47, 12);
        //inventory.add_items(41, 1);
        inventory.add_items(32, 4);
        inventory.add_items(33, 7);
        world.write_model(@inventory);
    }

    #[abi(embed_v0)]
    impl ActionsImpl of IActions<ContractState> {
        fn spawn(ref self: ContractState) {
            let player = get_caller_address();
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
                update_block(ref self, x, y, z, itemType);
                return;
            }
            // handle hp
            if !remove_block(ref self, x, y, z + 1) {
                harvest(ref self, x, y, z + 1);
            }
        }

        fn use_item(ref self: ContractState, x: u64, y: u64, z: u64) {
            println!("Raw Position {} {} {}", x, y, z);

            let world = get_world(ref self);
            let player = get_caller_address();
            // get inventory and get current slot item id
            let mut inventory: Inventory = world.read_model((player, 0));
            let itemType: u16 = inventory.get_hotbar_selected_item_type();

            if (itemType == 41) {
                // hoe, transform grass to dirt
                update_block(ref self, x, y, z, itemType);
            } else {
                assert(itemType > 0, 'Error: item id is zero');
                if itemType < 32 {
                    place_block(ref self, x, y, z, itemType);
                } else {
                    plant(ref self, x, y, z + 1, itemType);
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

        fn craft(ref self: ContractState, item: u32) {
            println!("Crafting {}", item);
            try_craft(ref self, item.try_into().unwrap());
        }
    }
}

fn get_position_id(x: u64, y: u64, z: u64) -> u128 {
    x.into() * fast_power_2(80) + y.into() * fast_power_2(40) + z.into()
}

fn get_block_at_pos(blocks: u128, x: u64, y: u64, z: u64) -> u64 {
    let shift: u128 = fast_power_2(((x + y * 4 + z * 16) * 4).into()).into();
    return ((blocks / shift) % 8).try_into().unwrap();
}
