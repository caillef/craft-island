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
    pub island_id: felt252,
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
        if z % 4 < 2 {
            let block_info = get_block_at_pos(self.blocks2, x % 4, y % 4, z % 2);
            assert(block_info == 0, 'Error: block exists');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            self.blocks2 += block_id.into() * shift.into();

            // If grass under, replace with dirt
            if z % 2 == 1 {
                let block_under_info = get_block_at_pos(self.blocks2, x % 4, y % 4, z % 2 - 1);
                if block_under_info == 1 {
                    let shift: u128 = fast_power_2(
                        (((x % 4) + (y % 4) * 4 + (z % 2 - 1) * 16) * 4).into()
                    )
                        .into();
                    self.blocks2 -= block_under_info.into() * shift.into();
                    self.blocks2 += 2 * shift.into();
                }
            } else {}
        } else {
            let block_info = get_block_at_pos(self.blocks1, x % 4, y % 4, z % 2);
            assert(block_info == 0, 'Error: block exists');
            let shift: u128 = fast_power_2((((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into())
                .into();
            self.blocks1 += block_id.into() * shift.into();

            // If grass under, replace with dirt
            if z % 2 == 1 {
                let block_under_info = get_block_at_pos(self.blocks1, x % 4, y % 4, z % 2 - 1);
                if block_under_info == 1 {
                    let shift: u128 = fast_power_2(
                        (((x % 4) + (y % 4) * 4 + (z % 2 - 1) * 16) * 4).into()
                    )
                        .into();
                    self.blocks1 -= block_under_info.into() * shift.into();
                    self.blocks1 += 2 * shift.into();
                }
            } else {}
        }
    }

    fn remove_block(ref self: IslandChunk, x: u64, y: u64, z: u64) -> u16 {
        let mut block_id: u16 = 0;
        let mut layer: u128 = if z % 4 < 2 { self.blocks2 } else { self.blocks1 };
        let block_info = get_block_at_pos(layer, x % 4, y % 4, z % 2);
        if block_info == 0 {
            return 0;
        }
        let index: u128 = (((x % 4) + (y % 4) * 4 + (z % 2) * 16) * 4).into();
        let mut shift: u128 = fast_power_2(index).into();
        if z % 4 < 2 {
            self.blocks2 -= (block_info.into() * shift.into());
        } else {
            self.blocks1 -= (block_info.into() * shift.into());
        }
        block_id = block_info.try_into().unwrap();
       return block_id;
    }
}
