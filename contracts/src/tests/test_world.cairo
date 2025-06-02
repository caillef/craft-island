#[cfg(test)]
mod tests {
    use craft_island_pocket::models::common::PlayerData;

    #[test]
    fn test_player_data_creation() {
        let player = starknet::contract_address_const::<0x123>();
        let player_data = PlayerData {
            player,
            last_inventory_created_id: 0,
            last_space_created_id: 0,
            current_island_owner: 'test',
            current_island_id: 1,
        };

        assert(player_data.player == player, 'player address wrong');
        assert(player_data.last_inventory_created_id == 0, 'inventory id wrong');
        assert(player_data.last_space_created_id == 0, 'space id wrong');
        assert(player_data.current_island_owner == 'test', 'island owner wrong');
        assert(player_data.current_island_id == 1, 'island id wrong');
    }
}
