# Memory Management Issues in Craft Island

This document outlines the three main memory management issues in the Craft Island game and proposes solutions for each.

## Issue 1: Unbounded Actor Accumulation

### Problem
The game currently loads all chunks and actors without any unloading mechanism. As players explore the world, the `Actors` TMap in `DojoCraftIslandManager` grows indefinitely, storing references to every spawned actor across all visited chunks.

### Impact
- Memory usage increases linearly with exploration
- No cleanup of distant or unused actors
- Potential for memory exhaustion in long play sessions
- Performance degradation as actor count increases

### Proposed Solution
Implement a chunk-based actor lifecycle management system:
- Track player position and maintain an "active radius" of chunks
- Unload actors outside the active radius
- Store unloaded chunk data in the cache for quick reloading
- Implement actor pooling for frequently spawned objects

## Issue 2: Spawn Queue Memory Growth

### Problem
While there is a 1000 item limit on the spawn queue, the queue processes only 10 items per frame. During rapid chunk loading or network updates, the queue can fill up, causing data loss when new items are rejected.

### Impact
- Lost spawn data when queue overflows
- Visible pop-in as spawn processing lags behind
- Memory spikes during heavy loading periods
- Inconsistent world state representation

### Proposed Solution
Implement a multi-tiered spawn system:
- Priority queue for nearby/visible spawns
- Background queue for distant spawns
- Dynamic processing rate based on frame time budget
- Compress spawn data for identical items in same chunk
- Serialize overflow to disk if needed

## Issue 3: Dojo Model Reference Retention

### Problem
The chunk cache stores direct references to Dojo model objects (`UDojoModelCraftIslandPocketIslandChunk`, etc.) indefinitely. These UObjects are never garbage collected, even when the data is no longer needed.

### Impact
- Persistent memory usage for all received blockchain data
- UObject overhead for every chunk, gatherable, and structure
- No way to free memory from old/unused model data
- Potential reference cycles preventing GC

### Proposed Solution
Implement a lightweight data caching system:
- Extract essential data from Dojo models into plain structs
- Allow original UObjects to be garbage collected
- Implement LRU (Least Recently Used) cache eviction
- Store only the data needed for reconstruction
- Use weak object pointers where Dojo references are necessary

## Implementation Priority

1. **Issue 1** should be addressed first as it has the most significant impact on gameplay and memory usage
2. **Issue 3** should be addressed second to prevent long-term memory leaks
3. **Issue 2** can be addressed last as the current overflow protection provides a workable (if imperfect) solution

## Additional Recommendations

- Implement memory usage monitoring and logging
- Add configurable memory budgets for different systems
- Create developer tools to visualize memory usage by chunk/region
- Consider using Unreal's World Composition or Level Streaming systems
- Profile memory usage patterns during typical gameplay sessions