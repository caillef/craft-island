use starknet::ContractAddress;

#[derive(Drop, Serde)]
#[dojo::model]
pub struct PlayerData {
    #[key]
    pub player: ContractAddress,
    pub onboarding_step: u8,
    pub coins: u32,
    pub current_island: felt252
}

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
    pub blocks1: u128,
    pub blocks2: u128,
}

#[derive(Drop, Serde)]
#[dojo::model]
pub struct Inventory {
    #[key]
    pub owner: ContractAddress,
    #[key]
    pub id: u16,
    pub inventory_type: u8, // 0 for main inventory, 1 for storage
    pub slots1: felt252, // 9 * 28 bits, 10 for resource type, 8 for qty, 10 rest
    pub slots2: felt252, // 9 * 28 bits, 10 for resource type, 8 for qty, 10 rest
    pub slots3: felt252, // 9 * 28 bits, 10 for resource type, 8 for qty, 10 rest
    pub slots4: felt252, // 9 * 28 bits, 10 for resource type, 8 for qty, 10 rest
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
    pub resource_id: u32,
    pub planted_at: u64, // timestamp
    pub next_harvest_at: u64, // timestamp
    pub harvested_at: u64, // timestamp
    pub max_harvest: u8, // if 0, destroyed when harvested
    pub remained_harvest: u8, // if 0, destroyed when gathered
    pub destroyed: bool,
}
