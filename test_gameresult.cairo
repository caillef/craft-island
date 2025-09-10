//! Test script for GameResult system
//! Run with: sozo execute actions test_gameresult_success
//! Run with: sozo execute actions test_gameresult_error

use craft_island_pocket::common::errors::{GameResult, GameError, GameResultTrait};

// Test function that returns success
fn test_gameresult_success() -> GameResult<u32> {
    GameResult::Ok(42)
}

// Test function that returns error
fn test_gameresult_error() -> GameResult<u32> {
    GameResult::Err(GameError::ResourceNotFound)
}

// Test unwrap success
fn test_unwrap_success() -> u32 {
    let result = test_gameresult_success();
    result.unwrap() // Should return 42
}

// Test is_ok/is_err
fn test_result_checks() -> (bool, bool) {
    let success = test_gameresult_success();
    let error = test_gameresult_error();
    
    (success.is_ok(), error.is_err()) // Should return (true, true)
}