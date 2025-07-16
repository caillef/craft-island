# Memory Leak Fixes Summary

This document summarizes the critical memory leak fixes implemented in the Dojo Plugin and DojoHelpers.

## Implemented Fixes

### 1. RAII Wrapper for FieldElement Arrays (DojoModule.h)
- **Added:** `FFieldElementArrayWrapper` class with proper RAII semantics
- **Benefits:** Automatic cleanup of FieldElement arrays, exception-safe memory management
- **Usage:** Replaced all malloc/free patterns in ExecuteRaw() and ExecuteFromOutside()

### 2. Comprehensive Resource Cleanup (DojoHelpers.cpp)
- **Added:** Proper destructor and EndPlay cleanup
- **Fixed:** ToriiClient, Subscription, Account, and Provider memory leaks
- **Tracking:** Added arrays to track allocated resources for proper cleanup

### 3. Thread-Safe Singleton Access (DojoHelpers.cpp/h)
- **Added:** FCriticalSection for thread-safe access to static instance
- **Fixed:** Race conditions in multi-threaded scenarios
- **Pattern:** GetGlobalInstance() with proper locking

### 4. UObject Creation with Proper Outer (DojoHelpers.cpp)
- **Fixed:** All NewObject calls now use GetTransientPackage() as outer
- **Before:** `NewObject<UDojoModelCraftIslandPocketGatherableResource>()`
- **After:** `NewObject<UDojoModelCraftIslandPocketGatherableResource>(GetTransientPackage())`

### 5. Global Resource Tracking (DojoHelpers.cpp)
- **Added:** Global counters for all resource types
- **Tracking:** ToriiClients, Accounts, Providers, Subscriptions
- **Debugging:** Enhanced LogResourceUsage() with leak detection

### 6. Provider Cleanup on Failure (DojoModule.cpp)
- **Fixed:** Provider cleanup on account creation failure
- **Pattern:** Always free provider even if account creation fails

## Memory Management Best Practices Applied

### 1. RAII Pattern
```cpp
// Automatic cleanup with FFieldElementArrayWrapper
FFieldElementArrayWrapper feltsWrapper(nbFelts);
// No manual free needed - destructor handles it
```

### 2. Resource Tracking
```cpp
// Track all allocated resources
AllocatedAccounts.Add(account);
GlobalActiveAccounts++;
```

### 3. Proper Cleanup Sequence
```cpp
// Cancel operations before freeing resources
FDojoModule::SubscriptionCancel(subscription);
subscription = nullptr;
// Then free the client
client_free(toriiClient);
```

### 4. Thread Safety
```cpp
FScopeLock Lock(&InstanceMutex);
// Safe access to singleton
```

## Testing Memory Leaks

To verify the fixes, use the LogResourceUsage() function:

```cpp
// In Blueprint or C++
DojoHelpers->LogResourceUsage();
```

This will output:
- Local resource counts
- Global resource counts
- Memory leak warnings if global > local counts
- Estimated memory usage

## Impact

These fixes prevent:
- Memory leaks during normal gameplay
- Crashes on shutdown
- Resource exhaustion in long sessions
- Race conditions in multi-threaded scenarios

All memory management functions from dojo.h are now properly utilized:
- `client_free()` for ToriiClient
- `provider_free()` for Provider
- `account_free()` for Account
- `subscription_cancel()` for Subscription