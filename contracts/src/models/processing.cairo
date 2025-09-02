use starknet::ContractAddress;

#[derive(Copy, Drop, Serde, PartialEq, Introspect, Default, DojoStore)]
pub enum ProcessType {
    #[default]
    None,
    GrindWheat,     // Wheat -> Flour
    CutLogs,        // Wood Log -> Planks
    SmeltOre,       // Iron Ore -> Iron Ingot
    CrushStone,     // Stone -> Gravel
    ProcessClay,    // Clay -> Brick
}

#[derive(Copy, Drop, Serde)]
#[dojo::model]
pub struct ProcessingLock {
    #[key]
    pub player: ContractAddress,
    pub unlock_time: u64,
    pub process_type: ProcessType,
    pub batches_processed: u32,
}

#[derive(Copy, Drop, Serde)]
pub struct ProcessingConfig {
    pub input_item: u32,      // Item ID
    pub output_item: u32,     // Item ID
    pub input_amount: u32,
    pub output_amount: u32,
    pub time_per_batch: u64,  // seconds
}

pub fn get_processing_config(process_type: ProcessType) -> ProcessingConfig {
    match process_type {
        ProcessType::None => ProcessingConfig {
            input_item: 0,
            output_item: 0,
            input_amount: 0,
            output_amount: 0,
            time_per_batch: 0,
        },
        ProcessType::GrindWheat => ProcessingConfig {
            input_item: 48, // Wheat
            output_item: 999, // Flour (TODO: needs to be added to items.json)
            input_amount: 1,
            output_amount: 1,
            time_per_batch: 5,
        },
        ProcessType::CutLogs => ProcessingConfig {
            input_item: 44, // Oak Log
            output_item: 45, // Oak Plank
            input_amount: 1,
            output_amount: 2,
            time_per_batch: 4,
        },
        ProcessType::SmeltOre => ProcessingConfig {
            input_item: 999, // Iron Ore (TODO: needs to be added to items.json)
            output_item: 999, // Iron Ingot (TODO: needs to be added to items.json)
            input_amount: 1,
            output_amount: 1,
            time_per_batch: 15,
        },
        ProcessType::CrushStone => ProcessingConfig {
            input_item: 3, // Stone (block ID)
            output_item: 33, // Rock (closest existing item)
            input_amount: 1,
            output_amount: 4,
            time_per_batch: 8,
        },
        ProcessType::ProcessClay => ProcessingConfig {
            input_item: 999, // Clay (TODO: needs to be added to items.json)
            output_item: 999, // Brick (TODO: needs to be added to items.json)
            input_amount: 1,
            output_amount: 1,
            time_per_batch: 20,
        },
    }
}

// Maximum processing time in seconds (1 hour)
pub const MAX_PROCESSING_TIME: u64 = 3600;
