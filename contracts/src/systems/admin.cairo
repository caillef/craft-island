#[starknet::interface]
trait IAdmin<T> {
    fn give_self(ref self: T, item: u16, qty: u32);
}

// dojo decorator
#[dojo::contract]
mod admin {
    use super::IAdmin;
    use starknet::{get_caller_address};
    use craft_island_pocket::models::inventory::InventoryTrait;

    fn get_world(ref self: ContractState) -> dojo::world::storage::WorldStorage {
        self.world(@"craft_island_pocket")
    }

    #[abi(embed_v0)]
    impl AdminImpl of IAdmin<ContractState> {
        fn give_self(ref self: ContractState, item: u16, qty: u32) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            InventoryTrait::add_to_player_inventories(ref world, player.into(), item, qty);
        }
    }
}
