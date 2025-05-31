use dojo::{world::WorldStorage,model::ModelStorage};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::models::islandchunk::IslandChunk;
use craft_island_pocket::models::common::{PlayerData, GatherableResource};
use starknet::ContractAddress;

fn init_island(ref world: WorldStorage, player: ContractAddress) {
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

fn init_player(ref world: WorldStorage, player: ContractAddress) {
    let mut player_data: PlayerData = PlayerData {
        player: player,
        last_inventory_created_id: 2, // hotbar, inventory, craft
        last_space_created_id: 1, // own island
        current_island_owner: player.into(),
        current_island_id: 1,
    };
    world.write_model(@player_data);
}

fn init_inventory(ref world: WorldStorage, player: ContractAddress) {
    let mut hotbar: Inventory = InventoryTrait::new(0, 0, 9, player);
    hotbar.add_items(1, 19);
    hotbar.add_items(2, 23);
    hotbar.add_items(41, 1);
    hotbar.add_items(32, 4);
    hotbar.add_items(43, 1);
    hotbar.add_items(50, 1);
    world.write_model(@hotbar);

    let mut inventory: Inventory = InventoryTrait::new(1, 1, 27, player);
    inventory.add_items(46, 8);
    inventory.add_items(47, 12);
    inventory.add_items(41, 1);
    inventory.add_items(32, 4);
    inventory.add_items(33, 7);
    inventory.add_items(36, 1);
    inventory.add_items(38, 1);
    world.write_model(@inventory);

    let craft: Inventory = InventoryTrait::new(2, 2, 9, player);
    world.write_model(@craft);
}

pub fn init(ref world: WorldStorage, player: ContractAddress) {
    init_player(ref world, player);
    init_island(ref world, player);
    init_inventory(ref world, player);
}
