use dojo::{world::WorldStorage,model::ModelStorage};
use craft_island_pocket::models::inventory::{Inventory, InventoryTrait};
use craft_island_pocket::models::islandchunk::IslandChunkTrait;
use craft_island_pocket::models::common::{PlayerData};
use craft_island_pocket::models::gatherableresource::GatherableResourceTrait;
use starknet::ContractAddress;

fn init_island(ref world: WorldStorage, player: ContractAddress) {
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080100000007ff0000000800, 0, 76842741656518657));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080100000008000000000800, 0, 4785147619639313));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080100000008010000000800, 0, 281547992268817));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080000000008000000000800, 0, 1229782938247303441));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080000000008010000000800, 0, 1229782938247303441));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080000000008020000000800, 0, 4296085777));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x000000080000000007ff0000000800, 0, 1229782938247303440));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007ff00000008000000000800, 0, 1229782938247303441));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007ff00000008010000000800, 0, 1234568085865828625));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007ff00000008020000000800, 0, 18760703480098));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007ff00000007ff0000000800, 0, 1224997790610882560));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007fe00000008000000000800, 0, 1152939097061326848));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007fe00000008010000000800, 0, 1224997790627664128));
    world.write_model(@IslandChunkTrait::new(player.into(), 1, 0x00000007fe00000008020000000800, 0, 268439552));

    // wooden sticks
    world.write_model(@GatherableResourceTrait::new(player.into(), 1, 0x000000080000000008000000000800, 17, 32));
    // rocks
    world.write_model(@GatherableResourceTrait::new(player.into(), 1, 0x000000080000000008000000000800, 23, 33));
    // boulders
    world.write_model(@GatherableResourceTrait::new(player.into(), 1, 0x000000080000000008010000000800, 16, 49));
    // trees
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x000000080000000007ff0000000800, 23, 46));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x000000080000000008010000000800, 30, 46));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008000000000800, 24, 46));
    // wheat
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008020000000800, 17, 47));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008010000000800, 29, 47));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008010000000800, 25, 47));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008020000000800, 16, 47));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008010000000800, 28, 47));
    world.write_model(@GatherableResourceTrait::new_ready(player.into(), 1, 0x00000007ff00000008010000000800, 24, 47));
}

fn init_player(ref world: WorldStorage, player: ContractAddress) {
    let mut player_data: PlayerData = PlayerData {
        player: player,
        last_inventory_created_id: 2, // hotbar, inventory, craft
        last_space_created_id: 1, // own island
        current_space_owner: player.into(),
        current_space_id: 1,
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
    hotbar.add_items(51, 5);
    hotbar.add_items(53, 5);
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
