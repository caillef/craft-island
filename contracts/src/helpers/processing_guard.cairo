use starknet::ContractAddress;
use dojo::world::storage::WorldStorage;
use dojo::model::ModelStorage;
use craft_island_pocket::models::processing::{ProcessingLock};

pub fn ensure_not_locked(world: @WorldStorage, player: ContractAddress) {
    let current_time = starknet::get_block_timestamp();
    let processing_lock: ProcessingLock = world.read_model(player);
    assert(processing_lock.unlock_time <= current_time, 'Player is processing');
}