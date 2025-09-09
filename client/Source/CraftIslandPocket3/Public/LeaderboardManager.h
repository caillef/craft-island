#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "LeaderboardManager.generated.h"

// Forward declarations
class ADojoCraftIslandManager;
class UDojoModel;

UCLASS(BlueprintType, Blueprintable)
class CRAFTISLANDPOCKET3_API ULeaderboardEntry : public UObject
{
    GENERATED_BODY()

public:
    ULeaderboardEntry();

    UPROPERTY(BlueprintReadWrite, Category = "Leaderboard")
    int32 Index = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Leaderboard")
    FString Name = "";

    UPROPERTY(BlueprintReadWrite, Category = "Leaderboard")
    FString Amount = "0";

    // Function to initialize the entry with values
    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void Initialize(int32 InIndex, const FString& InName, const FString& InAmount);
};

// Delegate for notifying Blueprint when leaderboard updates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderboardUpdated, const TArray<ULeaderboardEntry*>&, LeaderboardEntries);

// Structure to cache player data for leaderboard
USTRUCT()
struct FPlayerLeaderboardData
{
    GENERATED_BODY()

    FString PlayerAddress;
    FString PlayerName;
    int32 Coins = 0;

    FPlayerLeaderboardData()
    {
        PlayerAddress = "";
        PlayerName = "";
        Coins = 0;
    }

    FPlayerLeaderboardData(const FString& InPlayerAddress, int32 InCoins, const FString& InPlayerName = "")
        : PlayerAddress(InPlayerAddress), PlayerName(InPlayerName), Coins(InCoins)
    {
    }
};

UCLASS(BlueprintType, Blueprintable)
class CRAFTISLANDPOCKET3_API ULeaderboardManager : public UUserWidget
{
    GENERATED_BODY()

public:
    ULeaderboardManager(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;

public:
    // Blueprint event that gets broadcast when leaderboard updates
    UPROPERTY(BlueprintAssignable, Category = "Leaderboard")
    FOnLeaderboardUpdated OnLeaderboardUpdated;

    // Reference to DojoCraftIslandManager to connect to PlayerData updates
    UPROPERTY(BlueprintReadWrite, Category = "Leaderboard")
    ADojoCraftIslandManager* DojoManager = nullptr;

    // Function to set the DojoManager reference (called from Blueprint)
    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void SetDojoManager(ADojoCraftIslandManager* InDojoManager);

    // Function called when PlayerData is updated
    UFUNCTION()
    void OnPlayerDataUpdated(UDojoModelCraftIslandPocketPlayerData* PlayerData);

    // Function to manually refresh the leaderboard
    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void RefreshLeaderboard();

    // Get current leaderboard entries (for debugging or direct access)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Leaderboard")
    TArray<ULeaderboardEntry*> GetCurrentLeaderboard() const;

    // Get specific player's rank (returns -1 if not found)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Leaderboard")
    int32 GetPlayerRank(const FString& PlayerAddress) const;

    // Test function to add sample data for debugging
    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void AddTestData();

    // Blueprint implementable event for custom UI updates
    UFUNCTION(BlueprintImplementableEvent, Category = "Leaderboard")
    void OnLeaderboardChanged(const TArray<ULeaderboardEntry*>& LeaderboardEntries);

private:
    // Cache of player data for leaderboard
    TArray<FPlayerLeaderboardData> CachedPlayerData;

    // Current top 10 leaderboard entries
    TArray<ULeaderboardEntry*> CurrentLeaderboard;

    // Number of entries to show in leaderboard
    static constexpr int32 MAX_LEADERBOARD_ENTRIES = 10;


    // Update a specific player's data in cache
    void UpdatePlayerInCache(const FString& PlayerAddress, int32 Coins, const FString& PlayerName = "");

    // Rebuild leaderboard from cached data
    void RebuildLeaderboard();

    // Convert player address to display name (can be enhanced later)
    FString GetPlayerDisplayName(const FString& PlayerAddress) const;

    // Format coin amount with K, M, B, T suffixes
    FString FormatCoinAmount(int32 Coins) const;

    // Sort cached player data by coins (descending)
    void SortPlayerDataByCoins();

};
