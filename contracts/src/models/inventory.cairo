use starknet::ContractAddress;
use craft_island_pocket::helpers::math::{fast_power_2};
use core::traits::BitAnd;
use core::traits::BitOr;
use core::traits::Div;

impl BitAndImpl of BitAnd<felt252> {
    fn bitand(lhs: felt252, rhs: felt252) -> felt252 {
        let (lhs, rhs): (u256, u256) = (lhs.into(), rhs.into());
        (lhs & rhs).try_into().unwrap()
    }
}

impl BitOrImpl of BitOr<felt252> {
    fn bitor(lhs: felt252, rhs: felt252) -> felt252 {
        let (lhs, rhs): (u256, u256) = (lhs.into(), rhs.into());
        (lhs | rhs).try_into().unwrap()
    }
}

impl DivImpl of Div<felt252> {
    fn div(lhs: felt252, rhs: felt252) -> felt252 {
        let (lhs, rhs): (u256, u256) = (lhs.into(), rhs.into());
        (lhs / rhs).try_into().unwrap()
    }
}

#[derive(Copy, Drop, Serde)]
#[dojo::model]
pub struct Inventory {
    #[key]
    pub owner: ContractAddress,
    #[key]
    pub id: u16,
    pub inventory_type: u8,
    pub inventory_size: u8,
    pub slots1: felt252,
    pub slots2: felt252,
    pub slots3: felt252,
    pub slots4: felt252,
}

#[derive(Copy, Drop)]
struct SlotData {
    resource_type: u16,
    quantity: u8,
    extra: u16
}

#[generate_trait]
impl SlotDataImpl of SlotDataTrait {
    fn is_empty(self: SlotData) -> bool {
        self.resource_type == 0 && self.quantity == 0
    }
}

#[generate_trait]
impl InventoryImpl of InventoryTrait {
    fn add_resource(ref self: Inventory, slot: u8, resource_type: u16, quantity: u8) -> bool {
        let slot_data = self.get_slot_data(slot);
        if slot_data.is_empty() {
            self.set_slot_data(slot, resource_type, quantity, 0);
            return true;
        }
        false
    }

    fn remove_resource(ref self: Inventory, slot: u8, quantity: u8) -> bool {
        let slot_data = self.get_slot_data(slot);
        if !slot_data.is_empty() && slot_data.quantity >= quantity {
            let new_quantity = slot_data.quantity - quantity;
            if new_quantity == 0 {
                self.clear_slot(slot);
            } else {
                self.set_slot_data(slot, slot_data.resource_type, new_quantity, slot_data.extra);
            }
            return true;
        }
        false
    }

    fn move_resource(ref self: Inventory, from_slot: u8, to_slot: u8) -> bool {
        let from_data = self.get_slot_data(from_slot);
        let to_data = self.get_slot_data(to_slot);

        if !from_data.is_empty() && to_data.is_empty() {
            self.set_slot_data(to_slot, from_data.resource_type, from_data.quantity, from_data.extra);
            self.clear_slot(from_slot);
            return true;
        }
        false
    }

    fn get_slot_data(self: @Inventory, slot: u8) -> SlotData {
        let felt_index = slot / 9;
        let slot_in_felt = slot % 9;
        let felt_value = match felt_index {
            0 => *self.slots1,
            1 => *self.slots2,
            2 => *self.slots3,
            3 => *self.slots4,
            _ => 0,
        };

        let shift: u128 = (slot_in_felt * 28).into();
        let power_val = fast_power_2(shift);
        let slot_data: felt252 = (felt_value / power_val.try_into().unwrap()) & 0xFFFFFFF;

        let resource_type = (slot_data / fast_power_2(18).try_into().unwrap()) & 0x3FF;
        let quantity = (slot_data / fast_power_2(10).try_into().unwrap()) & 0xFF;
        let extra = slot_data & 0x3FF;

        SlotData {
            resource_type: resource_type.try_into().unwrap(),
            quantity: quantity.try_into().unwrap(),
            extra: extra.try_into().unwrap()
        }
    }

    fn set_slot_data(ref self: Inventory, slot: u8, resource_type: u16, quantity: u8, extra: u16) {
        let felt_index = slot / 9;
        let slot_in_felt = slot % 9;
        let shift: u128 = (slot_in_felt * 28).into();

        // Create mask for the slot we want to clear
        let slot_mask: felt252 = 0xFFFFFFF.into() * fast_power_2(shift).try_into().unwrap();
        // First clear the slot by subtracting existing data
        match felt_index {
            0 => { self.slots1 = self.slots1 - (self.slots1 & slot_mask); },
            1 => { self.slots2 = self.slots2 - (self.slots2 & slot_mask); },
            2 => { self.slots3 = self.slots3 - (self.slots3 & slot_mask); },
            3 => { self.slots4 = self.slots4 - (self.slots4 & slot_mask); },
            _ => {},
        };

        // Pack the new slot data
        let slot_data: felt252 = (
            ((resource_type.into() & 0x3FF) * fast_power_2(18).try_into().unwrap()) |
            ((quantity.into() & 0xFF) * fast_power_2(10).try_into().unwrap()) |
            (extra.into() & 0x3FF)
        ).into();

        // Shift into position
        let slot_data = slot_data * fast_power_2(shift).try_into().unwrap();

        match felt_index {
            0 => { self.slots1 = self.slots1 | slot_data; },
            1 => { self.slots2 = self.slots2 | slot_data; },
            2 => { self.slots3 = self.slots3 | slot_data; },
            3 => { self.slots4 = self.slots4 | slot_data; },
            _ => {},
        };
    }

    fn clear_slot(ref self: Inventory, slot: u8) {
        self.set_slot_data(slot, 0, 0, 0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_resource_to_empty_slot() {
        let mut inventory = Inventory::default();
        assert!(inventory.add_resource(0, 1, 5));
        let data = inventory.get_slot_data(0);
        assert_eq!(data.resource_type, 1);
        assert_eq!(data.quantity, 5);
    }

    #[test]
    fn test_add_resource_to_occupied_slot() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        assert!(!inventory.add_resource(0, 2, 3));
    }

    #[test]
    fn test_remove_resource_partial() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        assert!(inventory.remove_resource(0, 3));
        let data = inventory.get_slot_data(0);
        assert_eq!(data.quantity, 2);
    }

    #[test]
    fn test_remove_resource_complete() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        assert!(inventory.remove_resource(0, 5));
        let data = inventory.get_slot_data(0);
        assert!(data.is_empty());
    }

    #[test]
    fn test_remove_resource_insufficient() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        assert!(!inventory.remove_resource(0, 6));
    }

    #[test]
    fn test_move_resource_success() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        assert!(inventory.move_resource(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert!(from_data.is_empty());
        assert_eq!(to_data.resource_type, 1);
        assert_eq!(to_data.quantity, 5);
    }

    #[test]
    fn test_move_resource_to_occupied() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        inventory.add_resource(1, 2, 3);
        assert!(!inventory.move_resource(0, 1));
    }

    #[test]
    fn test_slot_data_empty() {
        let data = SlotData { resource_type: 0, quantity: 0, extra: 0 };
        assert!(data.is_empty());
    }

    #[test]
    fn test_slot_data_not_empty() {
        let data = SlotData { resource_type: 1, quantity: 5, extra: 0 };
        assert!(!data.is_empty());
    }

    #[test]
    fn test_clear_slot() {
        let mut inventory = Inventory::default();
        inventory.add_resource(0, 1, 5);
        inventory.clear_slot(0);
        let data = inventory.get_slot_data(0);
        assert!(data.is_empty());
    }
}
