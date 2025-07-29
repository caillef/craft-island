# Space 1 Return Issue Analysis

## Problem Description
When trying to return to space 1 from another space, the player remains in the new space instead of being teleported back.

## Potential Issues

### 1. Server-Side Space Update Not Happening
**Issue**: The `RequestGoBackHome` function might not be properly updating the player's space on the server.
- The `visit` function might be failing
- The server might not be sending back the updated PlayerData
- The space ID parameter might be incorrect (should it be 1 or 0?)

**Debug Steps**:
- Check if `CallCraftIslandPocketActionsVisit` is being called with the correct space ID
- Verify the server logs to see if the visit action is executed
- Check if a PlayerData update is sent after the visit call

### 2. PlayerData Update Not Triggering Space Change
**Issue**: The PlayerData update might not be detected as a space change.
- The comparison `CurrentSpaceOwner != PlayerData->CurrentSpaceOwner` might fail if the owner is the same
- The space ID might not be changing as expected
- The PlayerData might not be for the current player

**Debug Steps**:
- Log the incoming PlayerData values
- Log what CurrentSpaceOwner and CurrentSpaceId are before the update
- Verify `IsCurrentPlayer()` is returning true

### 3. Space Change Detection Logic Issues
**Issue**: The space change detection might have logical errors.
```cpp
bool bSpaceChanged = (CurrentSpaceOwner != PlayerData->CurrentSpaceOwner ||
                     CurrentSpaceId != PlayerData->CurrentSpaceId);
```
- If going from space 2 to space 1, both owned by the same player, only the ID changes
- The condition should trigger, but might not if there's a timing issue

### 4. RequestGoBackHome Implementation
**Current Implementation**:
```cpp
void ADojoCraftIslandManager::RequestGoBackHome()
{
    if (DojoHelpers)
    {
        DojoHelpers->CallCraftIslandPocketActionsVisit(Account, 1);
    }
}
```

**Potential Issues**:
- Is space ID 1 correct? Or should it be 0?
- Should we use the player's actual home space ID from stored data?
- Is the Account parameter correct?

### 5. Timing Issues
**Issue**: The client might be trying to handle the space change before the server confirms it.
- The actors might be getting hidden/shown at the wrong time
- The teleportation might happen before the space actually changes

### 6. Actor Visibility Logic
**Issue**: The logic for determining when we're "returning to space 1" might be incorrect.
```cpp
bool bReturningToSpace1 = (PlayerData->CurrentSpaceOwner == Account.Address && PlayerData->CurrentSpaceId == 1);
```
- This checks the NEW space, which is correct
- But Account.Address might not match CurrentSpaceOwner format

### 7. Cache and Loading Issues
**Issue**: The space 1 actors might not be properly tracked.
- The `bSpace1ActorsHidden` flag might be in the wrong state
- The actors might have been destroyed instead of hidden
- The Actors map might not contain the space 1 actors anymore

## Recommended Debug Actions

### 1. Add Comprehensive Logging
```cpp
void ADojoCraftIslandManager::RequestGoBackHome()
{
    UE_LOG(LogTemp, Warning, TEXT("RequestGoBackHome: Called, current space: %s:%d"), 
        *CurrentSpaceOwner, CurrentSpaceId);
    UE_LOG(LogTemp, Warning, TEXT("RequestGoBackHome: Account.Address = %s"), *Account.Address);
    
    if (DojoHelpers)
    {
        UE_LOG(LogTemp, Warning, TEXT("RequestGoBackHome: Calling visit with space_id = 1"));
        DojoHelpers->CallCraftIslandPocketActionsVisit(Account, 1);
    }
}
```

### 2. Enhanced PlayerData Logging
```cpp
void ADojoCraftIslandManager::HandlePlayerData(UDojoModel* Object)
{
    // ... existing code ...
    UE_LOG(LogTemp, Warning, TEXT("HandlePlayerData: Received update - Player: %s, Space: %s:%d"), 
        *PlayerData->Player, *PlayerData->CurrentSpaceOwner, PlayerData->CurrentSpaceId);
    UE_LOG(LogTemp, Warning, TEXT("HandlePlayerData: Current tracking - Owner: %s, Space: %d"), 
        *CurrentSpaceOwner, CurrentSpaceId);
    UE_LOG(LogTemp, Warning, TEXT("HandlePlayerData: Account.Address = %s"), *Account.Address);
    // ... rest of code ...
}
```

### 3. Check Server Response
- Monitor network traffic to see if the visit command is sent
- Check if a PlayerData update comes back after the visit
- Verify the PlayerData contains the expected space information

### 4. Verify Space IDs
- Confirm that space 1 is the correct ID for the home space
- Check if the server uses 0-based or 1-based indexing for spaces
- Verify that the player's actual home space ID matches what we're using

## Most Likely Causes

1. **Server not updating player space**: The visit(1) call might not be working as expected
2. **PlayerData not being sent**: The server might not emit a PlayerData update after visit
3. **Owner comparison failing**: Account.Address format might not match CurrentSpaceOwner format
4. **Wrong space ID**: Using 1 when it should be 0, or vice versa

## Next Steps

1. Add the debug logging suggested above
2. Test the RequestGoBackHome function and check the logs
3. Verify the server is receiving and processing the visit command
4. Check if PlayerData updates are being sent and received
5. Confirm the correct space ID for the home space
6. Verify the owner string format matches between Account.Address and space owners