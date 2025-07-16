# Dojo Plugin Memory Analysis

This document provides a comprehensive analysis of memory leaks and management issues in the Dojo Plugin for Unreal Engine, along with recommendations based on the available memory management functions in dojo.h.

## Major Memory Leaks Identified

### 1. Static Instance Memory Leak
**Location:** DojoHelpers.cpp, Line 11-15
```cpp
ADojoHelpers* ADojoHelpers::Instance = nullptr;
```
**Issue:** Static instance is never properly cleaned up, leading to memory leaks on application shutdown.
**Impact:** Memory leak on exit, potential crash if accessed after destruction.

### 2. ToriiClient Never Freed
**Location:** DojoHelpers.cpp, Line 30
```cpp
toriiClient = FDojoModule::ClientNew(TCHAR_TO_UTF8(*toriiUrl), worldFelt);
```
**Issue:** `toriiClient` is created but never freed in destructor.
**Solution Available:** Use `client_free()` from dojo.h (line 1644)

### 3. Subscription Pointer Leak
**Location:** DojoHelpers.cpp, Lines 22-24
```cpp
if (subscription) {
    FDojoModule::SubscriptionCancel(subscription);
}
```
**Issue:** Only cancels subscription but doesn't free the memory.
**Solution Available:** After `subscription_cancel()`, the pointer should be deleted or set to nullptr.

### 4. Account Objects Never Freed
**Location:** DojoHelpers.cpp, Lines 46, 60-66
```cpp
Account* CreateAccount = FDojoModule::CreateAccount(provider, privateKeyFelt, accountAddressStr);
Account* BurnerAccount = FDojoModule::CreateBurner(provider, accountFelt, burnerKeyFelt);
```
**Issue:** Account pointers are created but never freed.
**Solution Available:** Use `account_free()` from dojo.h (line 1668)

### 5. ControllerAccount Memory Leak
**Location:** DojoHelpers.cpp, Lines 141-147
```cpp
void ADojoHelpers::OnControllerAccountProxy(ControllerAccount* controller)
{
    if (Instance && IsValid(Instance))
    {
        Instance->OnControllerAccount.Broadcast(controller);
    }
}
```
**Issue:** ControllerAccount pointer received in callback is never freed.
**Solution Available:** Need to track and free using appropriate function.

### 6. Provider Instance Leak
**Location:** DojoHelpers.cpp (multiple locations)
```cpp
Provider* provider = FDojoModule::ProviderNew(TCHAR_TO_UTF8(*RPC));
```
**Issue:** Provider instances created but never freed.
**Solution Available:** Use `provider_free()` from dojo.h (line 1652)

## UObject Management Issues

### 1. NewObject Without Proper Outer
**Locations:** DojoHelpers.cpp, Lines 432, 456, 479, 498, 516, 536
```cpp
UDojoModelCraftIslandPocketPlayer* Player = NewObject<UDojoModelCraftIslandPocketPlayer>();
```
**Issue:** UObjects created without proper outer, vulnerable to premature garbage collection.
**Fix:** Use `NewObject<UDojoModelCraftIslandPocketPlayer>(this)` or provide appropriate outer.

### 2. Potential Double-Delete of UObjects
**Location:** DojoHelpers.cpp, Lines 636-645
```cpp
ParsedModels.Add(Player);
OnDojoModelUpdated.Broadcast(Player);
```
**Issue:** UObjects broadcasted via delegate but ownership unclear, could lead to double-delete.

## Memory Management Functions Available in dojo.h

### Cleanup Functions Available but Not Used:
1. **`client_free()`** (line 1644) - Should be used for toriiClient
2. **`provider_free()`** (line 1652) - Should be used for Provider instances
3. **`account_free()`** (line 1668) - Should be used for Account instances
4. **`model_free()`** (line 1660) - Should be used for Model/Struct instances
5. **`ty_free()`** (line 1676) - Should be used for Type instances
6. **`entity_free()`** (line 1684) - Should be used for Entity instances
7. **`error_free()`** (line 1692) - Should be used for Error instances
8. **`world_metadata_free()`** (line 1700) - Should be used for World metadata
9. **`carray_free()`** (line 1709) - Should be used for C arrays
10. **`string_free()`** (line 1717) - Should be used for C strings

## Thread Safety Issues

### 1. Static Instance Access Without Synchronization
**Location:** DojoHelpers.cpp, Lines 134-136, 225-226
```cpp
if (Instance && IsValid(Instance))
{
    Instance->OnControllerAccount.Broadcast(controller);
}
```
**Issue:** No mutex protection, could lead to use-after-free in multi-threaded scenarios.

### 2. Async Operations Without Synchronization
**Location:** DojoHelpers.cpp, Lines 142-147, 153-166, 172-185
**Issue:** Async operations modifying shared state without proper synchronization.

## Raw Memory Management Issues

### 1. malloc Without RAII
**Location:** DojoModule.cpp, Lines 210-214, 242-246
```cpp
FieldElement* felts = (FieldElement*)malloc(Signature.Num() * sizeof(FieldElement));
// ... operations ...
free(felts);
```
**Issue:** If exception occurs between malloc and free, memory leaks.
**Fix:** Use RAII wrapper or TArray.

### 2. Mixed C/C++ Memory Management
**Issue:** Using C-style memory management in C++ code makes it error-prone.
**Fix:** Create RAII wrappers for all C resources.

## Recommended Solutions

### 1. Implement RAII Wrappers
Create smart pointer wrappers for Dojo resources:
```cpp
template<typename T>
using TDojoUniquePtr = TUniquePtr<T, TDeleterFunctionPointer<T, void(*)(T*)>>;

// Example usage:
TDojoUniquePtr<ToriiClient> Client(
    FDojoModule::ClientNew(...),
    &client_free
);
```

### 2. Add Destructor Cleanup
```cpp
ADojoHelpers::~ADojoHelpers()
{
    if (subscription) {
        FDojoModule::SubscriptionCancel(subscription);
        subscription = nullptr;
    }
    
    if (toriiClient) {
        client_free(toriiClient);
        toriiClient = nullptr;
    }
    
    // Clean up all other resources...
}
```

### 3. Track Resource Ownership
Maintain clear ownership of all allocated resources:
- Use TMap to track Account pointers
- Use TArray to track Model pointers
- Implement cleanup methods for each resource type

### 4. Implement Thread-Safe Singleton
```cpp
static ADojoHelpers* GetInstance()
{
    FScopeLock Lock(&SingletonMutex);
    return Instance;
}
```

### 5. Use Unreal's Memory Management
- Replace raw pointers with TSharedPtr/TWeakPtr where appropriate
- Use FMemory instead of malloc/free
- Leverage Unreal's garbage collection for UObjects

## Priority Fixes

1. **Critical:** Add cleanup in destructor for toriiClient, accounts, and provider
2. **High:** Fix UObject creation without proper outer
3. **High:** Implement thread-safe access to static instance
4. **Medium:** Replace malloc/free with RAII
5. **Medium:** Add proper error handling for all allocations

## Memory Usage Monitoring

Implement memory tracking:
```cpp
// Track active resources
static int32 ActiveClients = 0;
static int32 ActiveAccounts = 0;
static int32 ActiveProviders = 0;

// Log on creation/destruction
UE_LOG(LogDojo, Log, TEXT("Active resources - Clients: %d, Accounts: %d, Providers: %d"), 
    ActiveClients, ActiveAccounts, ActiveProviders);
```

## Conclusion

The Dojo Plugin has significant memory management issues that could lead to:
- Memory leaks during gameplay
- Crashes on shutdown
- Undefined behavior in multi-threaded scenarios
- Resource exhaustion in long sessions

All the necessary cleanup functions are available in dojo.h but are not being used. Implementing proper RAII patterns and leveraging Unreal's memory management systems would resolve most of these issues.