# Craft Island - Project Architecture Overview

## Project Description
Craft Island is a fully on-chain autonomous world game built on Starknet using the Dojo framework. It's similar to Animal Crossing or Minecraft, where players explore, gather resources, craft items, and build their islands. The unique aspect is that everything is stored on-chain - all game state, player inventories, world chunks, and game logic are persisted on the blockchain.

## Architecture Overview

### 1. Client Layer - Unreal Engine 5.6
**Location:** `/client/`

The game client is built with Unreal Engine 5.6 and contains:

#### C++ Source Code (`/client/Source/CraftIslandPocket3/`)
- **DojoCraftIslandManager**: Core manager class handling blockchain interactions and game state synchronization
- **BaseBlock**: Base class for all block types in the game (grass, dirt, stone, etc.)
- **BaseObject**: Base class for interactive objects and items
- **BaseWorldStructure**: Base class for buildings and structures
- **CraftIslandChunks**: Manages the chunk-based world system
- **DojoHelpers**: Helper functions for Dojo blockchain integration

#### Blueprint Assets (`/client/Content/`)
- Game logic blueprints (DojoGameLogic, CraftIslandGameInstance)
- Block implementations (B_Boulder, B_House, B_OakSapling, etc.)
- UI components (UserInterface, InventorySlot, UI_StoneCraft)
- Camera systems (TopCamera, OrbitController)

### 2. Smart Contract Layer - Cairo/Dojo
**Location:** `/contracts/`

Uses Dojo framework v1.7.0-alpha.1 on Starknet:

#### Core Systems (`/contracts/src/systems/`)
- **actions.cairo**: Main game actions including:
  - `spawn()`: Player spawning
  - `hit_block()`: Block interaction/mining
  - `craft()`: Item crafting
  - `open_container()`: Inventory management
  - `drop()`, `take()`: Item manipulation

#### Models (`/contracts/src/models/`)
- **inventory.cairo**: Inventory system with multiple types (hotbar, inventory, craft, storage, structure building, machine)
- **islandchunk.cairo**: Chunk-based world storage (4x4x4 blocks per chunk)
- **worldstructure.cairo**: World structures and buildings
- **gatherableresource.cairo**: Resource gathering mechanics

#### Infrastructure Scripts
- `restartKatanaSlot.sh`: Katana (local blockchain) management
  - **Important**: Before running this script, check if Katana is already running with `ps aux | grep katana`
  - If found, kill existing instance: `kill -9 <PID>`
- `restartToriiLocal.sh`: Torii (indexer) management
- Multiple deployment configurations (dev, release, second)
- **Note**: We use local Katana + local Torii for development, but will switch to Starknet testnet/mainnet + Sepolia Torii (on Slot) for production. Never Katana on Slot.

### 3. Data Layer
**Location:** `/data/`
- **items.json**: Complete item definitions including Unreal Engine actor classes
- **items.csv**: Simplified item data for easy editing
- Item icons and UI sprites

### 4. Documentation
**Location:** `/docs/`
- Next.js-based documentation website
- Game mechanics documentation
- Model and system documentation

## Key Technical Details

### Blockchain Integration
- All game state is stored on-chain using Dojo's ECS (Entity Component System) model
- Player actions are transactions on Starknet
- Game client synchronizes state through Torii indexer
- Uses Dojo Unreal Engine plugin for seamless integration

### World Management
- Chunk-based world system (4x4x4 blocks per chunk)
- Each chunk is stored as a single entity on-chain
- Dynamic loading and unloading of chunks based on player position

### Inventory System
- Multiple inventory types for different contexts
- Slot-based storage using bit manipulation for efficiency
- Maximum 255 slots per inventory
- All inventory state persisted on-chain

### Development Workflow
1. Smart contracts are written in Cairo and deployed using Dojo
2. Unreal Engine client connects to the blockchain through Dojo plugin
3. Player actions trigger transactions that update on-chain state
4. Torii indexer provides real-time state updates to connected clients

## Running the Project

### Smart Contracts
```bash
# local
sozo build
sozo migrate

# remote version (deployed on Slot)
sozo build --release
sozo migrate --release
```

### Client
1. Open `/client/CraftIslandPocket3.uproject` in Unreal Engine 5.6
2. Configure Dojo connection settings in game instance
3. Play in editor or build for target platform

## Important Commands
When making changes to the codebase:
- For contract changes: Run `sozo build` to check if it builds and `sozo migrate` to update
- For client changes: Compile in Unreal Engine editor

## Adding New Items
When adding new items to the game:
1. Add the item to `/data/items.csv` with all required fields
2. Add the item to `/data/items.json` with matching ID and properties
3. Update `/client/Source/CraftIslandPocket3/Public/E_Item.h` to add the item to the E_Item enum
4. If the item can be bought/sold, update the shop logic in contracts
