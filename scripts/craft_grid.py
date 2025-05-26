# USAGE
# python ./scripts/craft_grid.py 34,0,0,32,0,0,0,0,0

#!/usr/bin/env python3
import sys

def generate_craft_number(craft_grid):
    """Generate a number representing the craft grid.
    Each slot has 10 bits for ID, followed by 18 bits of zeros."""
    result = 0

    # Each slot is 28 bits total: 10 bits for ID + 18 bits of zeros
    bits_per_slot = 28

    # Process each slot
    for grid_pos, item_id in enumerate(craft_grid):
        if item_id == 0:
            continue  # Skip empty slots

        # Get the binary position for this grid position
        binary_pos = grid_pos

        # Calculate the shift amount
        shift_amount = binary_pos * bits_per_slot

        # For each slot, we need to put the ID in the first 10 bits (from right)
        # followed by 18 bits of zeros
        slot_value = (item_id << 18)  # Shift ID left by 18 bits to make room for zeros

        # Add this slot's contribution to the result
        result += (slot_value << shift_amount)

    return result

def main():
    if len(sys.argv) < 2:
        print("Usage: python craft_grid.py \"Item1,Item2,Item3,Item4,Item5,Item6,Item7,Item8,Item9\"")
        print("Example: python craft_grid.py \"Stone,Stone,Stone,Stone,,Stone,Stone,Stone,Stone\"")
        return

    # Split the input string by commas
    input_list = sys.argv[1].split(',')

    # Convert each element to a number (0 for empty slots)
    craft_grid = []
    for item in input_list:
        if item == '':
            craft_grid.append(0)  # Empty slot
        else:
            # Just convert the string to an integer
            try:
                item_id = int(item)
            except ValueError:
                # If it's not a valid integer, default to 1
                item_id = 1
            craft_grid.append(item_id)

    # Generate and print the craft number
    craft_number = generate_craft_number(craft_grid)
    print(f"\nCraft Number: {craft_number}")

if __name__ == "__main__":
    main()
