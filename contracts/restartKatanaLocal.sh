asdf exec katana --dev --dev.no-fee --dev.seed 0 &
sleep 1 && asdf exec sozo build && asdf exec sozo migrate
