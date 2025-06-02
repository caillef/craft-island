
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
        let mut max_harvest: u8 = 1;
        if resource_id == 49 {
            max_harvest = 255;
        }
        let remained_harvest: u8 = max_harvest;

        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        let planted_at = timestamp;
        let mut next_harvest_at = timestamp + 10;
        if resource_id == 32 || resource_id == 33 { // wooden sticks or rock
            next_harvest_at = 0; // instant
        }

        GatherableResource {
            island_owner: owner,
            island_id: island_id,
            chunk_id: chunk_id,
            position: position,
            resource_id: resource_id,
            planted_at: planted_at,
            next_harvest_at: next_harvest_at,
            harvested_at: 0,
            max_harvest: max_harvest,
            remained_harvest: remained_harvest,
            destroyed: false,
        }
    }

    fn new_ready(owner: felt252, island_id: u16, chunk_id: u128, position: u8, resource_id: u16) -> GatherableResource {
        let mut max_harvest: u8 = 1;
        if resource_id == 49 {
            max_harvest = 255;
        }
        let remained_harvest: u8 = max_harvest;
        let timestamp: u64 = starknet::get_block_info().unbox().block_timestamp;
        let planted_at = timestamp;
        let mut next_harvest_at = timestamp + 10;
        if resource_id == 32 || resource_id == 33 { // wooden sticks or rock
            next_harvest_at = 0; // instant
        }

        GatherableResource {
            island_owner: owner,
            island_id: island_id,
            chunk_id: chunk_id,
            position: position,
            resource_id: resource_id,
            planted_at: planted_at,
            next_harvest_at: planted_at + 1,
            harvested_at: 0,
            max_harvest: max_harvest,
            remained_harvest: remained_harvest,
            destroyed: false,
        }
    }
}
