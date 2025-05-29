
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
    pub structure_type: u8,
    pub build_inventory_id: u16,
    pub completed: bool,
    pub linked_space_owner: felt252,
    pub linked_space_id: u16,
    pub destroyed: bool,
}
