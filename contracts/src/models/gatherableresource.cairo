
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


#[generate_trait]
pub impl GatherableResourceImpl of GatherableResourceTrait {
    fn new(owner: felt252, island_id: u16, chunk_id: u128, position: u8, resource_id: u16) -> GatherableResource {
        Self::create_resource(owner, island_id, chunk_id, position, resource_id, false)
    }

    fn new_ready(owner: felt252, island_id: u16, chunk_id: u128, position: u8, resource_id: u16) -> GatherableResource {
        Self::create_resource(owner, island_id, chunk_id, position, resource_id, true)
    }

    fn create_resource(owner: felt252, island_id: u16, chunk_id: u128, position: u8, resource_id: u16, ready: bool) -> GatherableResource {
        let max_harvest = Self::get_max_harvest(resource_id);
        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        let next_harvest_at = if ready {
            timestamp + 1
        } else {
            Self::calculate_next_harvest_time(resource_id, timestamp)
        };

        GatherableResource {
            island_owner: owner,
            island_id: island_id,
            chunk_id: chunk_id,
            position: position,
            resource_id: resource_id,
            planted_at: timestamp,
            next_harvest_at: next_harvest_at,
            harvested_at: 0,
            max_harvest: max_harvest,
            remained_harvest: max_harvest,
            destroyed: false,
        }
    }

    fn get_max_harvest(resource_id: u16) -> u8 {
        if resource_id == 49 {
            255
        } else {
            1
        }
    }

    fn calculate_next_harvest_time(resource_id: u16, timestamp: u64) -> u64 {
        if resource_id == 32 || resource_id == 33 { // wooden sticks or rock
            0 // instant
        } else if resource_id == 51 { // carrot seed
            timestamp + 30 // 30 seconds
        } else if resource_id == 53 { // potato
            timestamp + 180 // 3 minutes
        } else if resource_id == 47 { // wheat seed
            timestamp + 1500 // 25 minutes
        } else {
            timestamp + 10
        }
    }
}
