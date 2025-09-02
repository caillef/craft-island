# Blueprint Modification Tasks for Universal Action Batching

This document lists all the Blueprint modifications needed to complete the universal action batching implementation in Craft Island.

## Overview
The C++ backend is complete and compiled. Now we need to update the Blueprint UI components to use the new queuing system instead of direct calls, and to handle optimistic updates.

## 1. InventorySlot Blueprint (`Content/CraftIsland/UI/InventorySlot.uasset`)

### Task 1.1: Replace Direct Inventory Move Calls
- **Current**: Calls `RequestInventoryMoveItem` directly through GameInstance
- **Change to**: Call `RequestQueueInventoryMove` on GameInstance
- **Steps**:
  1. Find the drag & drop logic in the Blueprint
  2. Replace the event call from `GameInstance->RequestInventoryMoveItem` to `GameInstance->RequestQueueInventoryMove`
  3. Keep the same parameters: FromInventory, FromSlot, ToInventory, ToSlot

### Task 1.2: Add Optimistic Visual Update
- **Add**: Immediate visual feedback when item is moved
- **Steps**:
  1. After calling `RequestQueueInventoryMove`, immediately update the visual slot
  2. Store the "pending" state in a variable
  3. Add a subtle visual indicator (e.g., slight transparency or border glow)

### Task 1.3: Listen for Optimistic Update Events
- **Bind to**: `OnOptimisticInventoryMove` delegate from GameInstance
- **Logic**:
  1. When event fires, check if it affects this slot
  2. Update visual state accordingly
  3. Handle rollback case (move item back if transaction fails)

## 2. UserInterface Blueprint (`Content/CraftIsland/UI/UserInterface.uasset`)

### Task 2.1: Add Pending Actions Counter
- **Add**: UI element showing pending action count
- **Steps**:
  1. Create a text widget for "Pending Actions: X"
  2. Bind to `OnActionQueueUpdate` delegate
  3. Update the counter when event fires
  4. Hide when count is 0

### Task 2.2: Add Force Send Button
- **Add**: Button to force send pending actions
- **Steps**:
  1. Create button widget "Execute Now" or similar
  2. On click, call `RequestFlushActionQueue` on GameInstance
  3. Only show when `GameInstance->GetPendingActionCount() > 0`

### Task 2.3: Listen for All Optimistic Events
- **Bind to**: All optimistic update delegates
- **Delegates**:
  - `OnOptimisticInventoryMove`
  - `OnOptimisticCraft`
  - `OnOptimisticSell`
- **Purpose**: Update any global UI elements that need to reflect optimistic changes

## 3. Craft Interface Blueprints

### Task 3.1: UI_StoneCraftRecipes (`Content/CraftIsland/UI/UI_StoneCraftRecipes.uasset`)
- **Current**: Calls craft directly
- **Change to**: Call `RequestQueueCraft` on GameInstance
- **Steps**:
  1. Find craft button click logic
  2. Replace direct call with `GameInstance->RequestQueueCraft(ItemId)`
  3. Add visual feedback for pending craft

### Task 3.2: General Craft UI Updates
- **Add**: Optimistic feedback
- **Steps**:
  1. Bind to `OnOptimisticCraft` delegate
  2. Show success/pending animation immediately
  3. Handle rollback if craft fails

## 4. Shop/Trade Interfaces

### Task 4.1: Buy Interface
- **Current**: Direct buy call
- **Change to**: Call `RequestQueueBuy` on GameInstance
- **Steps**:
  1. Collect selected items and quantities
  2. Call `GameInstance->RequestQueueBuy(ItemIds, Quantities)`
  3. Show pending purchase indicator

### Task 4.2: Sell Interface (`Content/CraftIsland/UI/DeliveryUI.uasset`)
- **Current**: Direct sell call
- **Change to**: Call `RequestQueueSell` on GameInstance
- **Steps**:
  1. Replace sell button logic
  2. Call `GameInstance->RequestQueueSell()` instead of direct call
  3. Bind to `GameInstance->OnOptimisticSell` for immediate feedback

## 5. Visual Feedback System

### Task 5.1: Create Pending State Materials/Styles
- **Create**: Visual indicators for pending actions
- **Suggestions**:
  1. Semi-transparent overlay material
  2. Animated border/glow effect
  3. Loading spinner for slots
  4. Color coding (blue = pending, green = confirmed, red = failed)

### Task 5.2: Create Reusable Pending Indicator Widget
- **Create**: Widget that can be added to any action button
- **Features**:
  1. Shows loading state
  2. Animates while pending
  3. Shows success/fail result
  4. Auto-hides after confirmation

## 6. Processing UI Updates

### Task 6.1: Update Processing Start
- **Find**: Where processing is initiated
- **Change**: Use queued version if needed
- **Note**: StartProcess might not need queuing if it's a longer operation

### Task 6.2: Update Cancel Processing
- **Change**: Could be queued with other actions
- **Consider**: Whether this should bypass queue for immediate response

## 7. Testing Checklist

After implementing all changes, test these scenarios:

### 7.1: Basic Functionality
- [ ] Single inventory move works
- [ ] Multiple rapid inventory moves batch correctly
- [ ] Craft action queues properly
- [ ] Buy/Sell work through queue
- [ ] Visual feedback appears immediately

### 7.2: Batching Behavior
- [ ] Multiple moves batch into single transaction
- [ ] Mixed action types batch appropriately
- [ ] Force send button works
- [ ] Timeout behavior (15 seconds) works

### 7.3: Visual Feedback
- [ ] Pending items show visual indicator
- [ ] Success confirmation updates visuals
- [ ] Rollback returns items to original position
- [ ] Pending counter updates correctly

### 7.4: Edge Cases
- [ ] Moving same item multiple times rapidly
- [ ] Filling up queue (test with many actions)
- [ ] Network failure handling
- [ ] Switching screens while actions pending

## 8. Optional Enhancements

### 8.1: Advanced Visual Feedback
- Add ghost/preview of item in destination slot
- Show estimated time until batch sends
- Add success/fail sound effects
- Create smooth transitions between states

### 8.2: Smart Batching UI
- Show which actions are being batched together
- Allow user to configure batch wait time
- Show gas savings estimate
- Add "batch mode" toggle for power users

### 8.3: Error Handling UI
- Show specific error messages for failed actions
- Add retry button for failed batches
- Log action history for debugging
- Add undo/redo for recent actions

## Implementation Order

1. **Start with**: InventorySlot changes (most common action)
2. **Then**: UserInterface pending counter
3. **Next**: Craft and Shop interfaces
4. **Finally**: Polish and visual enhancements

## Notes

- All queue methods are already exposed as `BlueprintCallable`
- All delegates are `BlueprintAssignable` and ready to use
- The C++ handles all the complex batching logic
- Blueprints just need to call queue methods and handle events
- Test with small batches first, then scale up

## Debugging Tips

- Use `GameInstance->GetPendingActionCount()` to check queue state
- Print Log when delegates fire to trace flow
- Add debug UI showing current queue contents
- Use `GameInstance->RequestFlushActionQueue()` if actions get stuck

## GameInstance Delegate Summary

### New Queue Methods (Use These):
- `RequestQueueInventoryMove(FromInventory, FromSlot, ToInventory, ToSlot)`
- `RequestQueueCraft(ItemId)`
- `RequestQueueSell()`
- `RequestQueueBuy(ItemIds[], Quantities[])`
- `RequestQueueVisit(SpaceId)`
- `RequestFlushActionQueue()`
- `GetPendingActionCount()` (returns int32)

### Optimistic Update Events (Bind to These):
- `OnOptimisticInventoryMove` - Fired when inventory move is queued
- `OnOptimisticCraft` - Fired when craft is queued
- `OnOptimisticSell` - Fired when sell is queued
- `OnActionQueueUpdate` - Fired when queue count changes

### Old Methods (Keep for Now, But Don't Use):
- `RequestInventoryMoveItem` - Will be deprecated
- `RequestCraft` - Will be deprecated
- `RequestSell` - Will be deprecated
- `RequestBuy` - Will be deprecated

Remember: The goal is to make the UI feel instant (optimistic updates) while batching happens invisibly in the background!