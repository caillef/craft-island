slot deployments delete craftislandpocket katana -f
sleep 1
slot deployments delete craftislandpocket torii -f
sleep 3
slot deployments create craftislandpocket katana --dev --dev.no-fee --dev.seed 0 --version v1.3.1
sleep 1
slot deployments create craftislandpocket torii --world 0x060014dc9f45c5488ee736ccebc06fef0ab5abf0206bea412bcd7cce79b78f35 --rpc https://api.cartridge.gg/x/craftislandpocket/katana
sleep 1
sozo migrate --release

echo Testing spawn...
sozo execute craft_island_pocket-actions spawn --release
