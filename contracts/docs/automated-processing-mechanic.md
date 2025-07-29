# Automated Processing Mechanic - Implementation Plan

## Overview
Implement a system where players can "lock" onto specific processing tasks (grinding wheat, cutting logs, etc.) that automatically processes resources over time. During this locked state, the player cannot perform other actions.

## Core Concept
- Player initiates processing with a single `start_processing` transaction
- System calculates maximum processable items based on inventory
- Items are immediately converted in inventory
- Player is locked for the calculated duration
- During lock period, player is restricted from other actions
- No second transaction needed - unlock happens automatically when time expires

## Implementation Components

### 1. New Model: `ProcessingLock`
```cairo
#[derive(Copy, Drop, Serde)]
#[dojo::model]
struct ProcessingLock {
    #[key]
    player: ContractAddress,
    unlock_time: u64,        // 0 means not locked
    process_type: ProcessType,
    batches_to_process: u32, // Pre-calculated based on inventory
}

#[derive(Copy, Drop, Serde, PartialEq)]
enum ProcessType {
    None,
    GrindWheat,      // Wheat -> Flour
    CutLogs,         // Wood Log -> Planks
    SmeltOre,        // Iron Ore -> Iron Ingot
    // Add more as needed
}
```

### 2. Processing Configuration
```cairo
struct ProcessingConfig {
    input_item: Item,
    output_item: Item,
    input_amount: u32,
    output_amount: u32,
    time_per_batch: u64, // seconds
}

// Example: 1 wheat -> 1 flour every 5 seconds
fn get_processing_config(process_type: ProcessType) -> ProcessingConfig {
    match process_type {
        ProcessType::GrindWheat => ProcessingConfig {
            input_item: Item::Wheat,
            output_item: Item::Flour,
            input_amount: 1,
            output_amount: 1,
            time_per_batch: 5,
        },
        ProcessType::CutLogs => ProcessingConfig {
            input_item: Item::WoodLog,
            output_item: Item::Plank,
            input_amount: 1,
            output_amount: 4,
            time_per_batch: 10,
        },
        // ... more configurations
    }
}
```

### 3. New System Functions

#### `start_processing(world, process_type: ProcessType)` - Single Transaction!
1. Check if player is already locked (`unlock_time > current_timestamp`)
2. Get processing config for the process_type
3. Count available input items in inventory
4. Calculate maximum batches: `max_batches = available_items / input_amount`
5. Calculate total processing time: `total_time = max_batches * time_per_batch`
6. **Immediately process all items**:
   - Remove all input items from inventory
   - Add all output items to inventory
7. Set ProcessingLock:
   - `unlock_time = current_timestamp + total_time`
   - `process_type = process_type`
   - `batches_to_process = max_batches`
8. Emit event with completion time

#### `cancel_processing(world)` - Optional
1. Check if player is locked (`unlock_time > current_timestamp`)
2. Calculate remaining time: `remaining = unlock_time - current_timestamp`
3. Calculate uncompleted batches based on remaining time
4. Return unprocessed input items to inventory
5. Remove corresponding output items
6. Clear ProcessingLock (`unlock_time = 0`)
7. Emit cancellation event

### 4. Action Restrictions

#### Option A: Check in Every Action (Simple but Repetitive)
Add to every action function:
```cairo
fn hit_block(world: @IWorldDispatcher) {
    let processing_lock = get!(world, caller, ProcessingLock);
    let current_time = starknet::get_block_timestamp();
    assert!(processing_lock.unlock_time <= current_time, "Cannot perform action while processing");
    // ... rest of function
}
```

#### Option B: Middleware Pattern (Cleaner)
Create a trait that wraps actions:
```cairo
trait ProcessingGuard {
    fn ensure_not_locked(world: @IWorldDispatcher, player: ContractAddress) {
        let lock = get!(world, player, ProcessingLock);
        let current_time = starknet::get_block_timestamp();
        assert!(lock.unlock_time <= current_time, "Locked until {}", lock.unlock_time);
    }
}

// Use in actions:
fn hit_block(world: @IWorldDispatcher) {
    ProcessingGuard::ensure_not_locked(world, caller);
    // ... rest of function
}
```

#### Option C: Hybrid Approach (Recommended)
- Allow certain "safe" actions during processing (viewing inventory, camera movement)
- Block only actions that would conflict (crafting, mining, building)
- Create a whitelist of allowed actions

### 5. Client Integration

#### UI Components Needed:
1. Processing selection menu
2. Progress indicator showing:
   - Current process type
   - Time elapsed
   - Items processed so far
   - "Stop Processing" button
3. Visual feedback (character animation, particle effects)

#### Client State Management:
```cpp
// In DojoCraftIslandManager
bool IsPlayerLocked();
FProcessType GetCurrentProcess();
float GetTimeUntilUnlock();
float GetProcessingProgress(); // (unlock_time - current) / total_time
void StartProcessing(FProcessType ProcessType);
void CancelProcessing(); // Optional
```

### 6. Edge Cases & Considerations

1. **Inventory Space**: Must check available space BEFORE starting processing
2. **Lock Duration Limits**: Cap maximum lock time (e.g., 1 hour) to prevent excessive locking
3. **Partial Batches**: Only process complete batches (no fractional items)
4. **Time Source**: Use blockchain timestamp for consistency
5. **Cancellation Penalty**: Consider if cancelling should lose some resources

### 7. Future Enhancements

1. **Processing Efficiency**: Skills or tools that reduce time_per_batch
2. **Multi-Processing**: Allow processing multiple types with upgrades
3. **Processing Stations**: Require specific structures (mill for wheat, sawmill for logs)
4. **Queue System**: Set up processing queues for automatic switching
5. **Bonus Outputs**: Chance for extra items based on skill/tool quality

## Implementation Steps

### Phase 1: Core System
1. Create ProcessingState model
2. Implement start_processing and stop_processing functions
3. Add basic action restrictions
4. Test with wheat grinding

### Phase 2: Expand Processing Types
1. Add more ProcessType variants
2. Configure all processing recipes
3. Balance time and output ratios

### Phase 3: Client Integration
1. Create UI for processing selection
2. Add progress indicators
3. Implement animations and effects
4. Handle client-server state sync

### Phase 4: Polish & Features
1. Add processing stations requirement
2. Implement efficiency modifiers
3. Add achievements/rewards
4. Optimize gas usage

## Technical Notes

- Use `starknet::get_block_timestamp()` for unlock time
- Single transaction design reduces gas costs and complexity
- Consider max lock duration to prevent griefing (e.g., 1 hour max)
- Store recipes in a separate model for easier updates
- Emit events with unlock_time for client synchronization

## Security Considerations

- Validate inventory space before processing to prevent item loss
- Cap maximum lock duration (e.g., 1 hour) to prevent indefinite locking
- Ensure atomic operations for inventory updates
- Check for integer overflow in batch calculations
- Consider adding minimum batch requirements to prevent spam

## Advantages of This Approach

1. **Single Transaction**: More gas efficient and simpler UX
2. **Predictable**: Players know exactly when they'll be unlocked
3. **No State Tracking**: Only need to store unlock timestamp
4. **Automatic**: No need for second transaction or cleanup
5. **Griefing Resistant**: Can't lock resources indefinitely