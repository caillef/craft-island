use starknet::ContractAddress;

#[derive(Drop, Serde)]
#[dojo::model]
pub struct PlayerData {
    #[key]
    pub player: ContractAddress,
    pub last_inventory_created_id: u16,
    pub last_space_created_id: u16,
    pub current_island_owner: felt252,
    pub current_island_id: u16,
}

// Not Used Yet
#[derive(Drop, Serde)]
#[dojo::model]
pub struct PlayerStats {
    #[key]
    pub player: ContractAddress,
    pub miner_level: u8,
    pub lumberjack_level: u8,
    pub farmer_level: u8,
    pub miner_xp: u32,
    pub lumberjack_xp: u32,
    pub farmer_xp: u32,
}

#[derive(Drop, Serde)]
#[dojo::model]
pub struct GatherableResource {
    #[key]
    pub island_owner: felt252,
    #[key]
    pub island_id: u16,
    #[key]
    pub chunk_id: u128,
    #[key]
    pub position: u8,
    pub resource_id: u16,
    pub planted_at: u64, // timestamp
    pub next_harvest_at: u64, // timestamp
    pub harvested_at: u64, // timestamp
    pub max_harvest: u8, // if 0, destroyed when harvested, 255 if unlimited
    pub remained_harvest: u8,
    pub destroyed: bool,
}
