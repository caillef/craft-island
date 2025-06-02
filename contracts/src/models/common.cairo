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
