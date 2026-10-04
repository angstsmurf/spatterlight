# Shop Shaper (ALPHA sandbox, no ending): best-reachable route.
# Buys two different produce items (same-name duplicates trigger disambiguation menus),
# stocks them at chosen prices, restocks, tries value/remove/upgrade, waits, then visits
# the furniture store. Stock lists are random per visit but deterministic under the oracle seed.
to grocery store
go to produce department
buy red apple
buy blackberries
to shop
stock red apple
12
stock blackberries
30
value red apple
look
remove blackberries
i
stock blackberries
25
upgrade shop
wait
wait
wait
wait
wait
look
to hub
to furniture store
look
to hub
