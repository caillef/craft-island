//! Bitwise extraction helpers  
//! Provides unified functions for extracting different sized integers from felt252

use craft_island_pocket::helpers::math::{fast_power_2};

/// Helper function to extract u8 from felt252
pub fn extract_u8(value: felt252, shift: u32) -> u8 {
    let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
    let masked: u256 = shifted - (shifted / 256) * 256;
    masked.try_into().unwrap()
}

/// Helper function to extract u16 from felt252
pub fn extract_u16(value: felt252, shift: u32) -> u16 {
    let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
    let masked: u256 = shifted - (shifted / 0x10000) * 0x10000;
    masked.try_into().unwrap()
}

/// Helper function to extract u32 from felt252
pub fn extract_u32(value: felt252, shift: u32) -> u32 {
    let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
    let masked: u256 = shifted - (shifted / 0x100000000) * 0x100000000;
    masked.try_into().unwrap()
}

/// Helper function to extract u64 from felt252
pub fn extract_u64(value: felt252, shift: u32, bits: u32) -> u64 {
    let shifted: u256 = value.into() / fast_power_2(shift.into()).into();
    let mask: u256 = fast_power_2(bits.into()).into();
    let masked: u256 = shifted - (shifted / mask) * mask;
    masked.try_into().unwrap()
}