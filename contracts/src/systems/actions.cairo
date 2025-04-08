use craft_island_pocket::helpers::math::{fast_power_2};

#[starknet::interface]
trait IActions<T> {
    fn spawn(ref self: T);
    //fn buy(ref self: T, resource_id: u32, quantity: u32);
    //fn sell(ref self: T, resource_id: u32, quantity: u32);
    //fn plant(ref self: T, x: u64, y: u64, z: u64, resource_id: u32);
    //fn harvest(ref self: T, x: u64, y: u64, z: u64);
    //fn place_block(ref self: T, x: u64, y: u64, z: u64, block_id: u32);
    //fn remove_block(ref self: T, x: u64, y: u64, z: u64);
    fn place(ref self: T, x: u64, y: u64, z: u64, resource_id: u32);
    fn hit_block(ref self: T, x: u64, y: u64, z: u64, hp: u32);
    fn add_chunk(ref self: T, x: u64, y: u64, z: u64);
    //fn craft(ref self: T);
}

// dojo decorator
#[dojo::contract]
mod actions {
    use super::{IActions, get_position_id, get_block_at_pos};
    use starknet::{get_caller_address};
    use craft_island_pocket::models::{
        IslandChunk, GatherableResource
    };
    use craft_island_pocket::helpers::math::{fast_power_2};

    use dojo::model::{ModelStorage};

    fn plant(ref self: ContractState, x: u64, y: u64, z: u64, resource_id: u32) {
        let mut world = self.world(@"craft_island_pocket");
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut resource: GatherableResource = world.read_model((island_id, chunk_id, position));
        assert!(resource.resource_id == 0, "Error: Resource exists");
        resource.resource_id = resource_id;

        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        resource.planted_at = timestamp;
        resource.next_harvest_at = timestamp + 10;
        resource.harvested_at = 0;
        resource.max_harvest = 1;
        resource.remained_harvest = resource.max_harvest;
        resource.destroyed = false;
        world.write_model(@resource);
    }

    fn harvest(ref self: ContractState, x: u64, y: u64, z: u64) {
        let mut world = self.world(@"craft_island_pocket");
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
        if resource.max_harvest > 0 && resource.remained_harvest < resource.max_harvest {
            resource.destroyed = true;
            resource.resource_id = 0;
        } else {
            resource.next_harvest_at = timestamp + 10;
        }
        world.write_model(@resource);
    }

    fn buy(
        ref self: ContractState, resource_id: u32, quantity: u32
    ) { //let player = get_caller_address();
    }

    fn sell(
        ref self: ContractState, resource_id: u32, quantity: u32
    ) { //let player = get_caller_address();
    }

    fn place_block(ref self: ContractState, x: u64, y: u64, z: u64, block_id: u32) {
        let mut world = self.world(@"craft_island_pocket");
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        println!("Raw Position {} {} {}", x, y, z);
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((island_id, chunk_id));
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
        let mut world = self.world(@"craft_island_pocket");
        let player = get_caller_address();
        let island_id: felt252 = player.into();
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let mut chunk: IslandChunk = world.read_model((island_id, chunk_id));
        //println!("Removing block: {} {}", chunk.blocks1, chunk.blocks2);
        if z % 4 < 2 {
            let block_info = get_block_at_pos(chunk.blocks2, x % 4, y % 4, z % 2);
            if block_info == 0 {
                return false;
            }
            let index: u128 = (((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into();
            let mut shift: u128 = fast_power_2(index).into();
            chunk.blocks2 -= (block_info.into() * shift.into());
        } else {
            let block_info = get_block_at_pos(chunk.blocks1, x % 4, y % 4, z % 2);
            if block_info == 0 {
                return false;
            }
            let index: u128 = (((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into();
            let mut shift: u128 = fast_power_2(index).into();
            chunk.blocks1 -= (block_info.into() * shift.into());
        }
        //println!("Removed block: {} {}", chunk.blocks1, chunk.blocks2);
        world.write_model(@chunk);
        return true;
        // Add block to inventory
    }

    #[abi(embed_v0)]
    impl ActionsImpl of IActions<ContractState> {
        fn spawn(ref self: ContractState) {
            let mut world = self.world(@"craft_island_pocket");

            let player = get_caller_address();

            let starter_chunk = IslandChunk {
                island_id: player.into(),
                chunk_id: 0x000000080000000008000000000800, // offset 2048,2048,2048
                blocks1: 0,
                blocks2: 1229782938247303441,
            };

            world.write_model(@starter_chunk);
        }

        fn place(ref self: ContractState, x: u64, y: u64, z: u64, resource_id: u32) {
            assert(resource_id > 0, 'Error: resource id is zero');
            if resource_id <= 3 {
                place_block(ref self, x, y, z, resource_id);
            } else {
                plant(ref self, x, y, z, resource_id);
            }
        }

        // x,y,z must be offsetted with 2048. 0,0,0 in the world is 2048,2048,2048 here
        fn add_chunk(ref self: ContractState, x: u64, y: u64, z: u64) {
            let mut world = self.world(@"craft_island_pocket");

            let player = get_caller_address();

            let chunk_id = x.into() * fast_power_2(80) + y.into() * fast_power_2(40) + z.into();
            let existing_chunk: IslandChunk = world.read_model((player, chunk_id));

            assert(existing_chunk.blocks1 + existing_chunk.blocks2 == 0, 'Error: chunk already exists');

            let new_chunk = IslandChunk {
                island_id: player.into(),
                chunk_id: chunk_id,
                blocks1: 0,
                blocks2: 1229782938247303441,
            };
            world.write_model(@new_chunk);
        }

        fn hit_block(ref self: ContractState, x: u64, y: u64, z: u64, hp: u32) {
            // handle hp
            if !remove_block(ref self, x, y, z) {
                harvest(ref self, x, y, z);
            }
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
