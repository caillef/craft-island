//! Error handling system for Craft Island game actions
//! Provides unified GameResult type to eliminate code duplication

#[derive(Drop, Serde)]
pub enum GameResult<T> {
    Ok: T,
    Err: GameError,
}

#[derive(Drop, Serde)]
pub enum GameError {
    PlayerLocked: u64,                    // unlock_time
    InsufficientItems: (u16, u32),        // (item_id, needed_quantity)
    InvalidPosition: (u64, u64, u64),     // (x, y, z)
    ResourceNotFound,
    InsufficientSpace,
    InvalidInput: ByteArray,
    ChunkNotFound,
    StructureNotFound,
    ProcessingInProgress,
    InsufficientFunds: u32,               // needed_amount
    ItemNotFound,
    InventoryFull,
    InvalidItem: u16,                     // item_id
    InvalidQuantity: u32,                 // quantity
    CraftingFailed: ByteArray,            // reason
    InvalidSlot: u8,                      // slot_number
    PermissionDenied,
    InvalidTarget,
    CooldownActive: u64,                  // remaining_time
}

pub trait GameResultTrait<T> {
    fn is_ok(self: @GameResult<T>) -> bool;
    fn is_err(self: @GameResult<T>) -> bool;
    fn unwrap(self: GameResult<T>) -> T;
    fn unwrap_or(self: GameResult<T>, default: T) -> T;
}

impl GameResultImpl<T, +Drop<T>, +Copy<T>> of GameResultTrait<T> {
    fn is_ok(self: @GameResult<T>) -> bool {
        match self {
            GameResult::Ok(_) => true,
            GameResult::Err(_) => false,
        }
    }
    
    fn is_err(self: @GameResult<T>) -> bool {
        match self {
            GameResult::Ok(_) => false,
            GameResult::Err(_) => true,
        }
    }
    
    fn unwrap(self: GameResult<T>) -> T {
        match self {
            GameResult::Ok(value) => value,
            GameResult::Err(error) => {
                let error_msg = format_error(error);
                panic!("{}", error_msg)
            },
        }
    }

    fn unwrap_or(self: GameResult<T>, default: T) -> T {
        match self {
            GameResult::Ok(value) => value,
            GameResult::Err(_) => default,
        }
    }
}

pub fn format_error(error: GameError) -> ByteArray {
    match error {
        GameError::PlayerLocked(unlock_time) => {
            "Player locked until timestamp"
        },
        GameError::InsufficientItems((item_id, needed)) => {
            "Insufficient items for crafting"
        },
        GameError::InvalidPosition((x, y, z)) => {
            "Invalid position provided"
        },
        GameError::ResourceNotFound => "Resource not found",
        GameError::InsufficientSpace => "Insufficient space in inventory",
        GameError::InvalidInput(reason) => {
            let mut msg: ByteArray = "Invalid input: ";
            msg.append(@reason);
            msg
        },
        GameError::ChunkNotFound => "Chunk not found",
        GameError::StructureNotFound => "Structure not found",
        GameError::ProcessingInProgress => "Processing already in progress",
        GameError::InsufficientFunds(needed) => {
            "Insufficient funds"
        },
        GameError::ItemNotFound => "Item not found",
        GameError::InventoryFull => "Inventory is full",
        GameError::InvalidItem(item_id) => {
            "Invalid item ID"
        },
        GameError::InvalidQuantity(qty) => {
            "Invalid quantity"
        },
        GameError::CraftingFailed(reason) => {
            "Crafting failed"
        },
        GameError::InvalidSlot(slot) => {
            "Invalid slot"
        },
        GameError::PermissionDenied => "Permission denied",
        GameError::InvalidTarget => "Invalid target",
        GameError::CooldownActive(remaining) => {
            "Cooldown active"
        },
    }
}

// Helper functions for common error cases
pub fn player_locked_error(unlock_time: u64) -> GameResult<()> {
    GameResult::Err(GameError::PlayerLocked(unlock_time))
}

pub fn insufficient_items_error(item_id: u16, needed: u32) -> GameResult<()> {
    GameResult::Err(GameError::InsufficientItems((item_id, needed)))
}

pub fn invalid_position_error(x: u64, y: u64, z: u64) -> GameResult<()> {
    GameResult::Err(GameError::InvalidPosition((x, y, z)))
}