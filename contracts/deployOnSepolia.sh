#!/bin/bash

# Craft Island - Sepolia Deployment Script
# This script handles deployment to Sepolia testnet with proper timeout settings

set -e  # Exit on error

echo "🏝️  Craft Island - Deploying to Sepolia"
echo "======================================="

# Check if we're in the contracts directory
if [ ! -f "Scarb.toml" ]; then
    echo "❌ Error: Must be run from the contracts directory"
    exit 1
fi

# Set longer timeout for Sepolia (5 minutes)
export STARKNET_RPC_REQUEST_TIMEOUT=300

echo "⚙️  Setting RPC timeout to 300 seconds..."
echo ""

# Build the contracts
echo "🔨 Building contracts for Sepolia..."
sozo build --profile sepolia

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Build successful!"
echo ""

# Run migration
echo "🚀 Starting migration to Sepolia..."
echo "⏳ This may take several minutes due to network latency..."
echo ""

sozo migrate --profile sepolia

if [ $? -ne 0 ]; then
    echo "❌ Migration failed!"
    echo "💡 Tips:"
    echo "   - Check your account has enough ETH for gas"
    echo "   - Verify your RPC endpoint is working"
    echo "   - Try increasing STARKNET_RPC_REQUEST_TIMEOUT if timeout persists"
    exit 1
fi

echo ""
echo "✅ Deployment successful!"
echo ""

# Extract world address from manifest
WORLD_ADDRESS=$(grep -A1 '"world":' manifest_sepolia.json | grep '"address":' | cut -d'"' -f4)
if [ ! -z "$WORLD_ADDRESS" ]; then
    echo "🌍 World deployed at: $WORLD_ADDRESS"
fi

# Extract actions contract address
ACTIONS_ADDRESS=$(grep -B2 '"name": "actions"' manifest_sepolia.json | grep '"address":' | head -1 | cut -d'"' -f4)
if [ ! -z "$ACTIONS_ADDRESS" ]; then
    echo "📜 Actions contract at: $ACTIONS_ADDRESS"
fi

echo ""
echo "📝 Manifest saved to: manifest_sepolia.json"
echo ""
echo "🎉 Deployment complete!"
echo ""
echo "Next steps:"
echo "1. Update your client configuration with the world address"
echo "2. Verify contracts on Starkscan: https://sepolia.starkscan.co/"
echo "3. Test your deployment with the client"