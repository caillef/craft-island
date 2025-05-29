use starknet::ContractAddress;
use craft_island_pocket::helpers::math::{fast_power_2, fast_power_2_u256};
use core::traits::BitAnd;
use core::traits::BitOr;
use core::traits::Div;

const MAX_SLOTS_SIZE: u8 = 255;

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
    pub inventory_type: u8, // 0 == hotbar, 1 == inventory, 2 == craft, 3 == storage, 4 == structure building, 5 == machine
    pub inventory_size: u8,
    pub slots1: felt252,
    pub slots2: felt252,
    pub slots3: felt252,
    pub slots4: felt252,
    pub hotbar_selected_slot: u8, // used when a slot is selected in the hotbar
}

#[derive(Copy, Drop)]
struct SlotData {
    item_type: u16,
    quantity: u8,
    extra: u16
}

#[generate_trait]
impl SlotDataImpl of SlotDataTrait {
    fn is_empty(self: SlotData) -> bool {
        self.item_type == 0 || self.quantity == 0
    }
}

#[generate_trait]
pub impl InventoryImpl of InventoryTrait {
    fn default(inventory_size: u8) -> Inventory {
        Inventory {
            owner: starknet::contract_address_const::<'PLAYER'>(),
            id: 0,
            inventory_type: 0,
            inventory_size: inventory_size,
            slots1: 0,
            slots2: 0,
            slots3: 0,
            slots4: 0,
            hotbar_selected_slot: 0,
        }
    }

    fn get_hotbar_selected_item_type(self: Inventory) -> u16 {
        let slot = self.hotbar_selected_slot;
        let slot_data = self.get_slot_data(slot);
        if slot_data.quantity == 0 {
            return 0;
        }
        slot_data.item_type
    }

    fn select_hotbar_slot(ref self: Inventory, slot: u8) {
        assert(slot < 9, 'Invalid hotbar slot index');
        let slot_data = self.get_slot_data(slot);
        assert(!slot_data.is_empty(), 'Cannot select empty hotbar slot');
        self.hotbar_selected_slot = slot;
    }

    fn new(id: u16, inventory_type: u8, inventory_size: u8, owner: ContractAddress) -> Inventory {
        assert(inventory_size <= 36, 'Inventory size cannot exceed 36');
        Inventory {
            owner,
            id: id,
            inventory_type,
            inventory_size,
            slots1: 0,
            slots2: 0,
            slots3: 0,
            slots4: 0,
            hotbar_selected_slot: 0,
        }
    }

    fn add_item(ref self: Inventory, slot: u8, item_type: u16, quantity: u8) -> bool {
        assert(slot < self.inventory_size, 'Invalid slot index');
        let slot_data = self.get_slot_data(slot);
        if slot_data.is_empty() || slot_data.item_type == item_type {
            self.set_slot_data(slot, item_type, quantity, 0);
            return true;
        }
        false
    }

    fn remove_item(ref self: Inventory, slot: u8, quantity: u8) -> bool {
        assert(slot < self.inventory_size, 'Invalid slot index');
        let slot_data = self.get_slot_data(slot);
        if !slot_data.is_empty() && slot_data.quantity >= quantity {
            let new_quantity = slot_data.quantity - quantity;
            if new_quantity == 0 {
                self.clear_slot(slot);
            } else {
                self.set_slot_data(slot, slot_data.item_type, new_quantity, slot_data.extra);
            }
            return true;
        }
        false
    }

    fn move_item_between_inventories(ref self: Inventory, from_slot: u8, ref to_inventory: Inventory, to_slot: u8) -> bool {
        assert(from_slot < self.inventory_size && to_slot < to_inventory.inventory_size, 'Invalid slot index');
        let from_data = self.get_slot_data(from_slot);
        let to_data = to_inventory.get_slot_data(to_slot);

        assert(!from_data.is_empty(), 'From slot empty');
        if to_data.is_empty() {
            to_inventory.set_slot_data(to_slot, from_data.item_type, from_data.quantity, from_data.extra);
            self.clear_slot(from_slot);
            return true;
        }
        // check if can merge and not a tool or a tool head
        if from_data.item_type == to_data.item_type && (from_data.item_type < 33 || from_data.item_type >= 44) {
            let sum: u16 = from_data.quantity.into() + to_data.quantity.into();
            if sum > 255 {
                let rest: u8 = (sum - 255).try_into().unwrap();
                to_inventory.set_slot_data(to_slot, from_data.item_type, 255, from_data.extra);
                self.set_slot_data(from_slot, to_data.item_type, rest, to_data.extra);
            } else {
                to_inventory.set_slot_data(to_slot, from_data.item_type, from_data.quantity + to_data.quantity, from_data.extra);
                self.clear_slot(from_slot);
            }
            return true;
        }
        to_inventory.set_slot_data(to_slot, from_data.item_type, from_data.quantity, from_data.extra);
        self.set_slot_data(from_slot, to_data.item_type, to_data.quantity, to_data.extra);
        true
    }

    fn move_item(ref self: Inventory, from_slot: u8, to_slot: u8) -> bool {
        assert(from_slot < self.inventory_size && to_slot < self.inventory_size, 'Invalid slot index');
        let from_data = self.get_slot_data(from_slot);
        let to_data = self.get_slot_data(to_slot);

        assert(!from_data.is_empty(), 'From slot empty');
        if to_data.is_empty() {
            self.set_slot_data(to_slot, from_data.item_type, from_data.quantity, from_data.extra);
            self.clear_slot(from_slot);
            return true;
        }
        // check if can merge and not a tool or a tool head
        if from_data.item_type == to_data.item_type && (from_data.item_type < 33 || from_data.item_type >= 44) {
            let sum: u16 = from_data.quantity.into() + to_data.quantity.into();
            if sum > 255 {
                let rest: u8 = (sum - 255).try_into().unwrap();
                self.set_slot_data(to_slot, from_data.item_type, 255, from_data.extra);
                self.set_slot_data(from_slot, to_data.item_type, rest, to_data.extra);
            } else {
                self.set_slot_data(to_slot, from_data.item_type, from_data.quantity + to_data.quantity, from_data.extra);
                self.clear_slot(from_slot);
            }
            return true;
        }
        self.set_slot_data(to_slot, from_data.item_type, from_data.quantity, from_data.extra);
        self.set_slot_data(from_slot, to_data.item_type, to_data.quantity, to_data.extra);
        true
    }

    fn get_item_amount(self: Inventory, item_type: u16) -> u32 {
        let mut total: u32 = 0;
        let mut slot: u8 = 0;
        loop {
            if slot >= self.inventory_size {
                break;
            }
            let slot_data = self.get_slot_data(slot);
            if slot_data.item_type == item_type {
                total += slot_data.quantity.into();
            }
            slot += 1;
        };
        total
    }

    fn get_slot_data(self: Inventory, slot: u8) -> SlotData {
        assert(slot < self.inventory_size, 'Invalid slot index');
        let felt_index = slot / 9;
        let slot_in_felt = slot % 9;
        let felt_value = match felt_index {
            0 => self.slots1,
            1 => self.slots2,
            2 => self.slots3,
            3 => self.slots4,
            _ => 0,
        };

        let shift: u256 = (slot_in_felt * 28).into();
        let power_val = fast_power_2_u256(shift);
        let slot_data: felt252 = (felt_value / power_val.try_into().unwrap()) & 0xFFFFFFF;

        let item_type = (slot_data / fast_power_2(18).try_into().unwrap()) & 0x3FF;
        let quantity = (slot_data / fast_power_2(10).try_into().unwrap()) & 0xFF;
        let extra = slot_data & 0x3FF;

        SlotData {
            item_type: item_type.try_into().unwrap(),
            quantity: quantity.try_into().unwrap(),
            extra: extra.try_into().unwrap()
        }
    }

    fn set_slot_data(ref self: Inventory, slot: u8, item_type: u16, quantity: u8, extra: u16) {
        assert(slot < self.inventory_size, 'Invalid slot index');
        let felt_index = slot / 9;
        let slot_in_felt = slot % 9;
        let shift: u256 = (slot_in_felt * 28).into();

        // Create mask for the slot we want to clear
        let slot_mask: felt252 = 0xFFFFFFF.into() * fast_power_2_u256(shift).try_into().unwrap();
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
            ((item_type.into() & 0x3FF) * fast_power_2(18).try_into().unwrap()) |
            ((quantity.into() & 0xFF) * fast_power_2(10).try_into().unwrap()) |
            (extra.into() & 0x3FF)
        ).into();

        // Shift into position
        let slot_data = slot_data * fast_power_2_u256(shift).try_into().unwrap();

        match felt_index {
            0 => { self.slots1 = self.slots1 | slot_data; },
            1 => { self.slots2 = self.slots2 | slot_data; },
            2 => { self.slots3 = self.slots3 | slot_data; },
            3 => { self.slots4 = self.slots4 | slot_data; },
            _ => {},
        };
    }

    fn clear_slot(ref self: Inventory, slot: u8) {
        assert(slot < self.inventory_size, 'Invalid slot index');
        self.set_slot_data(slot, 0, 0, 0)
    }

    fn add_items(ref self: Inventory, item_type: u16, mut quantity: u32) -> bool {
        // First try to fill existing slots of same item type that aren't full
        let mut slot: u8 = 0;
        while slot < self.inventory_size && quantity > 0 {
            let slot_data = self.get_slot_data(slot);
            if slot_data.item_type == item_type && slot_data.quantity < MAX_SLOTS_SIZE {
                let space_available = MAX_SLOTS_SIZE - slot_data.quantity;
                let amount_to_add = if quantity > space_available.into() {
                    space_available
                } else {
                    quantity.try_into().unwrap()
                };
                self.set_slot_data(slot, item_type, slot_data.quantity + amount_to_add, slot_data.extra);
                quantity -= amount_to_add.into();
            }
            slot += 1;
        };

        // If we still have quantity to add, find empty slots
        slot = 0;
        while slot < self.inventory_size && quantity > 0 {
            let slot_data = self.get_slot_data(slot);
            if slot_data.is_empty() {
                let amount_to_add = if quantity > MAX_SLOTS_SIZE.into() {
                    MAX_SLOTS_SIZE
                } else {
                    quantity.try_into().unwrap()
                };
                self.set_slot_data(slot, item_type, amount_to_add, 0);
                quantity -= amount_to_add.into();
            }
            slot += 1;
        };

        // If we still have quantity to add, there wasn't enough space
        assert(quantity == 0, 'Not enough space in inventory');
        true
    }

    fn remove_items(ref self: Inventory, item_type: u16, quantity: u32) -> bool {
        let mut remaining_quantity = quantity;
        let mut slot: u8 = 0;

        // First find slots with the item and remove from them
        while slot < self.inventory_size && remaining_quantity > 0 {
            let slot_data = self.get_slot_data(slot);
            if slot_data.item_type == item_type {
                let amount_to_remove = if remaining_quantity > slot_data.quantity.into() {
                    slot_data.quantity
                } else {
                    remaining_quantity.try_into().unwrap()
                };

                self.remove_item(slot, amount_to_remove);
                remaining_quantity -= amount_to_remove.into();
            }
            slot += 1;
        };

        // Check if we were able to remove all requested items
        assert(remaining_quantity == 0, 'Not enough items');
        true
    }

    fn world_structure_next_step(ref self: Inventory, ref build_inventory: Inventory) -> bool {
        let mut next_requirement = SlotData { item_type: 0, quantity: 0, extra: 0 };

        let slots_value: u256 = build_inventory.slots1.into();
        if slots_value > 0 {
            let mut i = 0;
            while i < 8 && next_requirement.quantity == 0 {
                next_requirement = build_inventory.get_slot_data(i);
                i += 1;
            }
        }

        if next_requirement.quantity > 0 {
            if self.get_item_amount(next_requirement.item_type) <= 0 {
                return false;
            }
            self.remove_items(next_requirement.item_type, 1);
            build_inventory.remove_items(next_requirement.item_type, 1);
            return true;
        }
        return false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_item_to_empty_slot() {
        let mut inventory = InventoryTrait::default(9);
        assert!(inventory.add_item(0, 1, 5));
        let data = inventory.get_slot_data(0);
        assert_eq!(data.item_type, 1);
        assert_eq!(data.quantity, 5);
    }

    #[test]
    fn test_add_item_to_occupied_slot() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        assert!(!inventory.add_item(0, 2, 3));
    }

    #[test]
    fn test_remove_item_partial() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        assert!(inventory.remove_item(0, 3));
        let data = inventory.get_slot_data(0);
        assert_eq!(data.quantity, 2);
    }

    #[test]
    fn test_remove_item_complete() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        assert!(inventory.remove_item(0, 5));
        let data = inventory.get_slot_data(0);
        assert!(data.is_empty());
    }

    #[test]
    fn test_remove_item_insufficient() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        assert!(!inventory.remove_item(0, 6));
    }

    #[test]
    fn test_move_item_success() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        assert!(inventory.move_item(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert!(from_data.is_empty());
        assert_eq!(to_data.item_type, 1);
        assert_eq!(to_data.quantity, 5);
    }

    #[test]
    fn test_move_item_merge() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        inventory.add_item(1, 1, 10);
        assert!(inventory.move_item(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert!(from_data.is_empty());
        assert_eq!(to_data.item_type, 1);
        assert_eq!(to_data.quantity, 15);
    }

    #[test]
    fn test_move_item_merge_max() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 10);
        inventory.add_item(1, 1, 251);
        assert!(inventory.move_item(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert_eq!(from_data.item_type, 1);
        assert_eq!(from_data.quantity, 6);
        assert_eq!(to_data.item_type, 1);
        assert_eq!(to_data.quantity, 255);
    }

    #[test]
    fn test_move_item_no_merge_for_tools() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 34, 1);
        inventory.add_item(1, 34, 1);
        assert!(inventory.move_item(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert_eq!(from_data.item_type, 34);
        assert_eq!(from_data.quantity, 1);
        assert_eq!(to_data.item_type, 34);
        assert_eq!(to_data.quantity, 1);
    }

    #[test]
    fn test_move_item_to_occupied() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        inventory.add_item(1, 2, 3);
        assert!(inventory.move_item(0, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory.get_slot_data(1);
        assert_eq!(from_data.item_type, 2);
        assert_eq!(from_data.quantity, 3);
        assert_eq!(to_data.item_type, 1);
        assert_eq!(to_data.quantity, 5);
    }

    #[test]
    fn test_move_item_to_other_inventory_occupied() {
        let mut inventory = InventoryTrait::default(9);
        let mut inventory2 = InventoryTrait::default(27);
        inventory.add_item(0, 1, 5);
        inventory2.add_item(1, 2, 3);
        assert!(inventory.move_item_between_inventories(0, ref inventory2, 1));
        let from_data = inventory.get_slot_data(0);
        let to_data = inventory2.get_slot_data(1);
        assert_eq!(from_data.item_type, 2);
        assert_eq!(from_data.quantity, 3);
        assert_eq!(to_data.item_type, 1);
        assert_eq!(to_data.quantity, 5);
    }

    #[test]
    fn test_slot_data_empty() {
        let data = SlotData { item_type: 0, quantity: 0, extra: 0 };
        assert!(data.is_empty());
    }

    #[test]
    fn test_slot_data_not_empty() {
        let data = SlotData { item_type: 1, quantity: 5, extra: 0 };
        assert!(!data.is_empty());
    }

    #[test]
    fn test_clear_slot() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        inventory.clear_slot(0);
        let data = inventory.get_slot_data(0);
        assert!(data.is_empty());
    }

    #[test]
    fn test_get_item_amount() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 5);
        inventory.add_item(1, 1, 3);
        inventory.add_item(2, 2, 4);
        assert_eq!(inventory.get_item_amount(1), 8);
        assert_eq!(inventory.get_item_amount(2), 4);
        assert_eq!(inventory.get_item_amount(3), 0);
    }

    #[test]
    #[should_panic(expected: ('Invalid slot index',))]
    fn test_invalid_slot_index() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(10, 1, 5);
    }

    #[test]
    fn test_add_items_split() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_items(1, 400);

        let slot0 = inventory.get_slot_data(0);
        assert_eq!(slot0.quantity, MAX_SLOTS_SIZE);

        let slot1 = inventory.get_slot_data(1);
        assert_eq!(slot1.quantity, 145);
    }

    #[test]
    fn test_add_items_to_existing() {
        let mut inventory = InventoryTrait::default(9);
        inventory.add_item(0, 1, 100);
        inventory.add_items(1, 200);

        let slot0 = inventory.get_slot_data(0);
        assert_eq!(slot0.item_type, 1);
        assert_eq!(slot0.quantity, 255);

        let slot1 = inventory.get_slot_data(1);
        assert_eq!(slot1.item_type, 1);
        assert_eq!(slot1.quantity, 45);
    }

    #[test]
    #[should_panic(expected: ('Not enough space in inventory',))]
    fn test_add_items_insufficient_space() {
        let mut inventory = InventoryTrait::default(1);
        inventory.add_items(1, 300);
    }
}
