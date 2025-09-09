#include "LeaderboardManager.h"
#include "DojoCraftIslandManager.h"
#include "CraftIslandGameInst.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

ULeaderboardEntry::ULeaderboardEntry()
{
    Index = 0;
    Name = "";
    Amount = "0";
}

void ULeaderboardEntry::Initialize(int32 InIndex, const FString& InName, const FString& InAmount)
{
    Index = InIndex;
    Name = InName;
    Amount = InAmount;
}

ULeaderboardManager::ULeaderboardManager(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Initialize arrays
    CachedPlayerData.Empty();
    CurrentLeaderboard.Empty();
}

void ULeaderboardManager::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Log, TEXT("LeaderboardManager: NativeConstruct called"));
}

void ULeaderboardManager::SetDojoManager(ADojoCraftIslandManager* InDojoManager)
{
    if (!InDojoManager)
    {
        return;
    }

    DojoManager = InDojoManager;
    
    // Connect to GameInstance's UpdatePlayerData delegate
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UCraftIslandGameInst* CraftIslandGI = Cast<UCraftIslandGameInst>(GameInstance))
        {
            CraftIslandGI->UpdatePlayerData.AddDynamic(this, &ULeaderboardManager::OnPlayerDataUpdated);
        }
    }
    
    RefreshLeaderboard();
}

void ULeaderboardManager::RefreshLeaderboard()
{
    UE_LOG(LogTemp, Log, TEXT("LeaderboardManager: RefreshLeaderboard called"));
    
    if (!DojoManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("LeaderboardManager: No DojoManager reference, cannot refresh leaderboard"));
        return;
    }

    // For now, we'll rebuild from what we have in cache
    // TODO: In the future, we might want to fetch all PlayerData from DojoManager
    RebuildLeaderboard();
}

TArray<ULeaderboardEntry*> ULeaderboardManager::GetCurrentLeaderboard() const
{
    return CurrentLeaderboard;
}

int32 ULeaderboardManager::GetPlayerRank(const FString& PlayerAddress) const
{
    for (int32 i = 0; i < CurrentLeaderboard.Num(); ++i)
    {
        // For now, compare against the display name
        // TODO: This should be enhanced to properly match player addresses
        if (CurrentLeaderboard[i]->Name.Contains(PlayerAddress) || 
            PlayerAddress.Contains(CurrentLeaderboard[i]->Name))
        {
            return i + 1; // Ranks are 1-indexed
        }
    }
    
    return -1; // Not found in top 10
}

void ULeaderboardManager::OnPlayerDataUpdated(UDojoModelCraftIslandPocketPlayerData* PlayerData)
{
    if (!PlayerData)
    {
        return;
    }
    
    // Extract player address, coins and name from PlayerData
    FString PlayerAddress = PlayerData->Player;
    int32 Coins = PlayerData->Coins;
    FString PlayerName = PlayerData->Name;
    
    // Update player in cache
    UpdatePlayerInCache(PlayerAddress, Coins, PlayerName);
    
    // Rebuild leaderboard with updated data
    RebuildLeaderboard();
}

void ULeaderboardManager::UpdatePlayerInCache(const FString& PlayerAddress, int32 Coins, const FString& PlayerName)
{
    // Find existing player in cache
    bool bPlayerFound = false;
    for (FPlayerLeaderboardData& PlayerData : CachedPlayerData)
    {
        if (PlayerData.PlayerAddress == PlayerAddress)
        {
            PlayerData.Coins = Coins;
            if (!PlayerName.IsEmpty())
            {
                PlayerData.PlayerName = PlayerName;
            }
            bPlayerFound = true;
            break;
        }
    }
    
    // If player not found, add them to cache
    if (!bPlayerFound)
    {
        CachedPlayerData.Add(FPlayerLeaderboardData(PlayerAddress, Coins, PlayerName));
    }
}

void ULeaderboardManager::RebuildLeaderboard()
{
    // Sort the cached data by coins (descending)
    SortPlayerDataByCoins();
    
    // Clear current leaderboard
    CurrentLeaderboard.Empty();
    
    // Build new leaderboard from top players
    int32 EntriesToAdd = FMath::Min(CachedPlayerData.Num(), MAX_LEADERBOARD_ENTRIES);
    
    for (int32 i = 0; i < EntriesToAdd; ++i)
    {
        const FPlayerLeaderboardData& PlayerData = CachedPlayerData[i];
        FString DisplayName = GetPlayerDisplayName(PlayerData.PlayerAddress);
        FString CoinsString = FormatCoinAmount(PlayerData.Coins);
        
        // Create new ULeaderboardEntry object
        ULeaderboardEntry* Entry = NewObject<ULeaderboardEntry>(this);
        Entry->Initialize(i + 1, DisplayName, CoinsString);
        CurrentLeaderboard.Add(Entry);
    }
    
    UE_LOG(LogTemp, Log, TEXT("LeaderboardManager: Rebuilt leaderboard with %d entries"), CurrentLeaderboard.Num());
    
    // Broadcast the updated leaderboard to Blueprint via delegate
    OnLeaderboardUpdated.Broadcast(CurrentLeaderboard);
    
    // Also call the Blueprint implementable event
    OnLeaderboardChanged(CurrentLeaderboard);
}

FString ULeaderboardManager::GetPlayerDisplayName(const FString& PlayerAddress) const
{
    // First try to find a name for this player address in our cache
    for (const FPlayerLeaderboardData& PlayerData : CachedPlayerData)
    {
        if (PlayerData.PlayerAddress == PlayerAddress)
        {
            // If player has a name set, use it
            if (!PlayerData.PlayerName.IsEmpty())
            {
                return PlayerData.PlayerName;
            }
            break;
        }
    }
    
    // Fall back to showing last 6 characters of address
    if (PlayerAddress.Len() > 6)
    {
        return FString::Printf(TEXT("...%s"), *PlayerAddress.Right(6));
    }
    
    return PlayerAddress;
}

FString ULeaderboardManager::FormatCoinAmount(int32 Coins) const
{
    if (Coins < 1000)
    {
        return FString::FromInt(Coins);
    }
    else if (Coins < 1000000) // 1K to 999.9K
    {
        float Value = Coins / 1000.0f;
        if (Value >= 100.0f)
        {
            return FString::Printf(TEXT("%.0fK"), Value); // 100K, 999K
        }
        else
        {
            return FString::Printf(TEXT("%.1fK"), Value); // 1.2K, 99.9K
        }
    }
    else if (Coins < 1000000000) // 1M to 999.9M
    {
        float Value = Coins / 1000000.0f;
        if (Value >= 100.0f)
        {
            return FString::Printf(TEXT("%.0fM"), Value); // 100M, 999M
        }
        else
        {
            return FString::Printf(TEXT("%.1fM"), Value); // 1.2M, 99.9M
        }
    }
    else if (Coins < 1000000000000LL) // 1B to 999.9B
    {
        float Value = Coins / 1000000000.0f;
        if (Value >= 100.0f)
        {
            return FString::Printf(TEXT("%.0fB"), Value); // 100B, 999B
        }
        else
        {
            return FString::Printf(TEXT("%.1fB"), Value); // 1.2B, 99.9B
        }
    }
    else // 1T+
    {
        float Value = Coins / 1000000000000.0f;
        if (Value >= 100.0f)
        {
            return FString::Printf(TEXT("%.0fT"), Value); // 100T+
        }
        else
        {
            return FString::Printf(TEXT("%.1fT"), Value); // 1.2T, 99.9T
        }
    }
}

void ULeaderboardManager::SortPlayerDataByCoins()
{
    // Sort by coins in descending order (highest first)
    CachedPlayerData.Sort([](const FPlayerLeaderboardData& A, const FPlayerLeaderboardData& B) {
        return A.Coins > B.Coins;
    });
}


void ULeaderboardManager::AddTestData()
{
    UE_LOG(LogTemp, Log, TEXT("LeaderboardManager: Adding test data"));
    
    // Clear existing data
    CachedPlayerData.Empty();
    
    // Add some test players with different coin amounts
    UpdatePlayerInCache(TEXT("0x1234567890abcdef"), 1000);
    UpdatePlayerInCache(TEXT("0x2345678901bcdefb"), 850);
    UpdatePlayerInCache(TEXT("0x3456789012cdefab"), 750);
    UpdatePlayerInCache(TEXT("0x4567890123defabc"), 600);
    UpdatePlayerInCache(TEXT("0x5678901234efabcd"), 500);
    UpdatePlayerInCache(TEXT("0x6789012345fabcde"), 400);
    UpdatePlayerInCache(TEXT("0x7890123456abcdef"), 300);
    UpdatePlayerInCache(TEXT("0x8901234567bcdefg"), 200);
    UpdatePlayerInCache(TEXT("0x9012345678cdefgh"), 150);
    UpdatePlayerInCache(TEXT("0xa123456789defghi"), 100);
    UpdatePlayerInCache(TEXT("0xb234567890efghij"), 75);
    UpdatePlayerInCache(TEXT("0xc345678901fghijk"), 50);
    
    // Rebuild leaderboard with test data
    RebuildLeaderboard();
}
