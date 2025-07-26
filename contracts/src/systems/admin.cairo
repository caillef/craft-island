#[starknet::interface]
trait IAdmin<T> {
    fn give_self(ref self: T, item: u16, qty: u32);
    fn give_coins(ref self: T, amount: u32);
}

// dojo decorator
#[dojo::contract]
mod admin {
    use super::IAdmin;
    use starknet::{get_caller_address};
    use craft_island_pocket::models::inventory::InventoryTrait;
    use craft_island_pocket::models::common::PlayerData;
    use dojo::{world::WorldStorage, model::ModelStorage};

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
        
        fn give_coins(ref self: ContractState, amount: u32) {
            let mut world = get_world(ref self);
            let player = get_caller_address();
            let mut player_data: PlayerData = world.read_model((player));
            player_data.coins += amount;
            world.write_model(@player_data);
        }
    }
}
