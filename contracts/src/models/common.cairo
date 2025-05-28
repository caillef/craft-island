use starknet::ContractAddress;

// Not Used Yet
#[derive(Drop, Serde)]
#[dojo::model]
pub struct PlayerData {
    #[key]
    pub player: ContractAddress,
    pub onboarding_step: u8,
    pub coins: u32,
    pub current_island: felt252
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
pub struct IslandChunk {
    #[key]
    pub island_id: felt252,
    #[key]
    pub chunk_id: u128, // pos x y z & 42 mask bits
    pub version: u8,
    pub blocks1: u128,
    pub blocks2: u128,
}

#[derive(Drop, Serde)]
#[dojo::model]
pub struct GatherableResource {
    #[key]
    pub island_id: felt252,
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

#[derive(Drop, Serde)]
#[dojo::model]
pub struct WorldStructure {
    #[key]
    pub island_id: felt252,
    #[key]
    pub chunk_id: u128,
    #[key]
    pub position: u8,
    pub structure_type: u8,
    pub build_inventory_id: u16,
    pub completed: bool,
    pub linked_space_id: felt252,
}
