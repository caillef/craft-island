use craft_island_pocket::helpers::{
    math::{fast_power_2},
};

fn get_position_id(x: u64, y: u64, z: u64) -> u128 {
    x.into() * fast_power_2(80) + y.into() * fast_power_2(40) + z.into()
}

fn get_block_at_pos(blocks: u128, x: u64, y: u64, z: u64) -> u64 {
    let shift: u128 = fast_power_2(((x + y * 4 + z * 16) * 4).into()).into();
    return ((blocks / shift) % 8).try_into().unwrap();
}

#[derive(Copy, Drop, Serde)]
#[dojo::model]
pub struct IslandChunk {
    #[key]
    pub island_owner: felt252,
    #[key]
    pub island_id: u16,
    #[key]
    pub chunk_id: u128, // pos x y z & 42 mask bits
    pub version: u8,
    pub blocks1: u128,
    pub blocks2: u128,
}


#[generate_trait]
pub impl IslandChunkImpl of IslandChunkTrait {
    fn update_block(ref self: IslandChunk, x: u64, y: u64, z: u64, tool: u16) {
        if z % 4 < 2 {
            let block_info = get_block_at_pos(self.blocks2, x % 4, y % 4, z % 2);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 41 && block_info == 1 {
                self.blocks2 -= block_info.into() * shift.into();
                self.blocks2 += 2 * shift.into();
            }
        } else {
            let block_info = get_block_at_pos(self.blocks1, x % 4, y % 4, z % 2);
            assert(block_info > 0, 'Error: block does not exist');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            if tool == 18 && block_info == 1 {
                self.blocks1 -= block_info.into() * shift.into();
                self.blocks1 += 2 * shift.into();
            }
        }
    }

    fn place_block(ref self: IslandChunk, x: u64, y: u64, z: u64, block_id: u16) {
        // Calculate local coordinates and determine which block storage to use
        let x_local = x % 4;
        let y_local = y % 4;
        let z_local = z % 2;
        let lower_layer = z % 4 < 2;

        // Get the appropriate block storage
        let blocks = if lower_layer { self.blocks2 } else { self.blocks1 };

        // Check if position is empty
        let block_info = get_block_at_pos(blocks, x_local, y_local, z_local);
        assert(block_info == 0, 'Error: block exists');

        // Calculate bit shift for updating block
        let index = ((x_local + y_local * 4 + z_local * 16) * 4).into();
        let shift: u128 = fast_power_2(index).into();

        // Place the block
        if lower_layer {
            self.blocks2 += block_id.into() * shift;
        } else {
            self.blocks1 += block_id.into() * shift;
        }

        // If block placed above ground level, check if grass is underneath
        if z_local == 1 {
            let block_under_info = get_block_at_pos(blocks, x_local, y_local, 0);

            // Convert grass to dirt when covered
            if block_under_info == 1 {
                let under_shift = fast_power_2(((x_local + y_local * 4) * 4).into()).into();

                if lower_layer {
                    self.blocks2 = self.blocks2 - block_under_info.into() * under_shift + 2 * under_shift;
                } else {
                    self.blocks1 = self.blocks1 - block_under_info.into() * under_shift + 2 * under_shift;
                }
            }
        }
    }

    fn remove_block(ref self: IslandChunk, x: u64, y: u64, z: u64) -> u16 {
        // Calculate local coordinates
        let x_local = x % 4;
        let y_local = y % 4;
        let z_local = z % 2;
        let lower_layer = z % 4 < 2;

        // Get the appropriate block storage
        let blocks = if lower_layer { self.blocks2 } else { self.blocks1 };

        // Check if position has a block
        let block_info = get_block_at_pos(blocks, x_local, y_local, z_local);
        if block_info == 0 {
            return 0; // No block to remove
        }

        // Calculate bit shift for updating block
        let index = ((x_local + y_local * 4 + z_local * 16) * 4).into();
        let shift = fast_power_2(index).into();

        // Remove the block
        if lower_layer {
            self.blocks2 -= block_info.into() * shift;
        } else {
            self.blocks1 -= block_info.into() * shift;
        }

        return block_info.try_into().unwrap();
    }
}
