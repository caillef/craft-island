#!/bin/bash
set -euo pipefail
pushd $(dirname "$0")/..

export RPC_URL="http://localhost:5050";

export WORLD_ADDRESS=$(cat ./manifests/dev/deployment/manifest.json | jq -r '.world.address')

# sozo execute --world <WORLD_ADDRESS> <CONTRACT> <ENTRYPOINT>
sozo execute --world $WORLD_ADDRESS craft_island_pocke-actions spawn --wait
