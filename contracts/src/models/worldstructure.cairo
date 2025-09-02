use dojo::{world::WorldStorage,model::ModelStorage};

use starknet::{get_caller_address};
use craft_island_pocket::helpers::utils::{
    get_position_id
};
use craft_island_pocket::models::common::{
    PlayerData
};
use craft_island_pocket::models::inventory::{
    Inventory, InventoryTrait
};
use craft_island_pocket::models::islandchunk::{
    IslandChunk, get_block_at_pos
};

#[derive(Drop, Serde)]
#[dojo::model]
pub struct WorldStructure {
    #[key]
    pub island_owner: felt252,
    #[key]
    pub island_id: u16,
    #[key]
    pub chunk_id: u128,
    #[key]
    pub position: u8,
    pub structure_type: u16,
    pub build_inventory_id: u16,
    pub completed: bool,
    pub linked_space_owner: felt252,
    pub linked_space_id: u16,
    pub destroyed: bool,
}

#[generate_trait]
pub impl WorldStructureImpl of WorldStructureTrait {
    fn place_structure(ref world: WorldStorage, x: u64, y: u64, z: u64, item_id: u16) {
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        
        // Check that we're placing at z > 0 (structures must be above ground)
        assert!(z > 8192, "Error: Structures must be above ground");
        
        // Check if there's a block underneath
        let chunk_id_below: u128 = get_position_id(x / 4, y / 4, (z - 1) / 4);
        let chunk_below: IslandChunk = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id_below));
        
        let z_below = z - 1;
        let block_below = if z_below % 4 < 2 {
            get_block_at_pos(chunk_below.blocks2, x % 4, y % 4, z_below % 2)
        } else {
            get_block_at_pos(chunk_below.blocks1, x % 4, y % 4, z_below % 2)
        };
        
        assert!(block_below > 0, "Error: Need block underneath");
        
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut structure: WorldStructure = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
        assert!(structure.structure_type == 0, "Error: World Structure exists");
        structure.structure_type = item_id;

        let mut inventory: Inventory = world.read_model((player, 0));
        inventory.remove_items(item_id, 1);
        world.write_model(@inventory);

        let mut player_data: PlayerData = world.read_model((player));
        assert!(player_data.last_inventory_created_id > 0, "Player not init");
        player_data.last_inventory_created_id += 1;
        player_data.last_space_created_id += 1;
        structure.build_inventory_id = player_data.last_inventory_created_id;

        let mut building_inventory: Inventory = InventoryTrait::new(player_data.last_inventory_created_id, 4, 2, player);
        building_inventory.slots1 = 2322443444159488; // (temporary: 1 stick 1 stone 00001000010000000100000000000000100000000000010000000000) First slot is 3 stick, second slot 3 stone 00001000010000001100000000000000100000000000110000000000
        building_inventory.readonly = true;
        world.write_model(@building_inventory);

        structure.completed = false;
        structure.linked_space_owner = player.into();
        structure.linked_space_id = player_data.last_space_created_id;
        structure.destroyed = false;
        world.write_model(@player_data);
        world.write_model(@structure);
    }

    fn upgrade_structure(ref world: WorldStorage, x: u64, y: u64, z: u64) {
        let player = get_caller_address();
        let player_data: PlayerData = world.read_model((player));
        let chunk_id: u128 = get_position_id(x / 4, y / 4, z / 4);
        // check block under
        let position: u8 = (x % 4 + (y % 4) * 4 + (z % 4) * 16).try_into().unwrap();
        let mut structure: WorldStructure = world.read_model((player_data.current_space_owner, player_data.current_space_id, chunk_id, position));
        assert!(structure.structure_type > 0, "Error: World Structure does not exist");

        let mut build_inventory: Inventory = world.read_model((player, structure.build_inventory_id));
        let mut hotbar: Inventory = world.read_model((player, 0));
        if !hotbar.world_structure_next_step(ref build_inventory) {
            let mut inventory: Inventory = world.read_model((player, 1));
            assert(inventory.world_structure_next_step(ref build_inventory), 'Not enough resource');
            world.write_model(@inventory);
        }
        if build_inventory.slots1 == 0 {
            structure.completed = true;
        }
        world.write_model(@structure);
        world.write_model(@build_inventory);
        world.write_model(@hotbar);
    }
}
