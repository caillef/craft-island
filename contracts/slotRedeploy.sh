slot deployments delete craftislandpocket katana -f
sleep 1
slot deployments delete craftislandpocket torii -f
sleep 3
slot deployments create craftislandpocket katana --dev --dev.no-fee --dev.seed 0
sleep 1
slot deployments create craftislandpocket torii --world 0x020f3516c97a58c881b17849e18117cc50073c67af78c52ba5ded5ced03f0bab --rpc https://api.cartridge.gg/x/craftislandpocket/katana
sleep 1
sozo migrate --release

echo Testing spawn...
sozo execute craft_island_pocket-actions spawn --release
