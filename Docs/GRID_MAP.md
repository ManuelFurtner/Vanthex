Aktuelle Standardkarte: FOUNDRY. Siehe FOUNDRY.md. Dieser Text beschreibt die vorherige GRID-Version.

# GRID — expanded energy facility

Default playable map: 200 x 240 metres, 48,000 square metres (four times the
previous GRID area). Runtime geometry is built from VanthexGridMap.cpp; Entry
remains the bootstrap rather than a separately authored .umap.

The reactor district retains the neutral core, two extraction yards, protected
3v3 starts and its elevated bridge. Four outer districts add 24 maintenance
hideouts with offset exits, 48 internal cover blocks, cargo screens, generator
banks, overhead power buses, and four upper service platforms.

## Lifts and power

There are five lifts total: the original reactor lift and one 6-metre lift in
each outer district. Each outer platform also has a walkable emergency ramp.
F calls or operates a lift from either landing. Platforms stop during an outage.

Four sector terminals control four independent circuits. F starts a four-second
hack. Remain within 2.5 metres and retain line of sight; damage, death, leaving
range or the round ending cancels it. C uses the existing ARC remote interaction
(up to 12 metres, 15-second ability cooldown), with the same sustained hack.
Only one player can hack a terminal at a time.

A completed hack toggles sector power. Linked lifts, lamps and the first
sector's reactor Shift devices lose power. Another completed hack restores it.
Terminals retain emergency power, so a blackout is reversible. Ambient daylight,
route markers and emergency indicator lights remain visible. This is local
sector power, not a total-world blackout. HUD shows circuit state, nearby
interaction instructions and hacking progress. Round reset restores all power.

Authority checks, interaction range, line of sight and progress run on the
server. Circuit membership, power, lift home position and hacking progress
replicate to clients.

## Validation

Run-Checks.ps1 covers historical mechanics, enlarged-map paths/ramps, circuit
propagation, an actual player riding/calling a lift, hack completion and
cancellation, and two-process networking including replicated sector outage.
Reports are in Saved/VanthexSelfTest.txt, VanthexMapTest.txt,
VanthexEnergyTest.txt, NetworkHost.txt and NetworkClient.txt.

-VanthexMapShots captures eight map views in Saved/GridMapShots.
-VanthexBotRouteProbe logs bot movement. Bots currently use ground routes and do
not choose tactical power sabotage or upper-floor flanks. This is a playable
prototype layout; competitive balance and finished art need playtesting.

Backup: _versions/20261101_before-mega-grid.

Verified 2026-11-01: Editor and Game builds PASS; 43 mechanics checks PASS;
69 map checks PASS; 17 energy/lift checks PASS (including F interaction and
actual rider transport). Host and client both reported 15 devices with the
correct three devices offline in sector 2. Remaining sectors stayed powered.

