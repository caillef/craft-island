// Test Ticket #4: World/Block Actions Migration Demo
use craft_island_pocket::systems::actions::IActionsDispatcherTrait;
use craft_island_pocket::systems::actions::IActionsDispatcher;

use starknet::ContractAddress;
use dojo::model::{ModelStorage};
use dojo::world::{IWorldDispatcher, IWorldDispatcherTrait};

fn main() {
    println!("🌍 Testing Ticket #4: World/Block Actions Migration Demo");
    
    // Connect to world
    let world = IWorldDispatcher { 
        contract_address: 0x067126801bb71a1a236d29bd789b93daa65d7f503f9d786235e9d5e4867022a4.try_into().unwrap() 
    };
    
    // Get actions contract
    let actions_address = world.contract_address;
    let actions = IActionsDispatcher { contract_address: actions_address };
    
    // Test the world actions migration demo function
    let result = actions.test_world_actions_migration_demo();
    
    println!("🎯 TICKET #4: World/Block Actions Migration Test Result: {}", result);
}