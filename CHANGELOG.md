# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/). Versions before 0.12 were private test builds.
The full technical notes for each version are in the header comment of `src/dinput8.c`.

## [0.12] — 2026-10-12

First public release.

### Added
- **LAN co-op, phase 1.** Each player runs the game on his own PC, over LAN or a virtual LAN such as Radmin VPN,
  ZeroTier or Tailscale, on UDP port 27016.
  - The partner appears as a second Spider-Man driven by the network: position, rotation and animation, smoothed
    over 100 ms.
  - Synced events: enemy deaths (matched by spawn node), trigger zones, level end, restart and mission failed.
    Both PCs load together.
  - F6 menu: `LAN CO-OP` (off / host / join), `HOST IP` (typed in the menu), `PORT`, `SYNC ENEMIES`,
    `GO TO PARTNER`, plus a status line.
  - `coop.ini` section `[lan]`: `mode`, `host_ip`, `port`, `nick`, `sync_world`, `sync_level`.
  - Session ids, so restarting the network on one side is safe. Events are sent only while the partner is on the
    same level.
- A reproducible build on Linux (`scripts/build-linux-clang.sh`), a Makefile for MinGW, and GitHub Actions CI.

### Changed
- In LAN mode the local player 2, split-screen and player 2 auto-spawn are off.
- `__declspec(align)` replaced by a portable macro so MinGW and clang builds keep the alignment.

### Known limitations
- Each PC fights its own copy of the enemies; only deaths are shared. Some enemy types (Black Cat, Lizard, clones
  and others) ignore the network kill and stay alive on the other PC.
- The partner can't be hurt over the network, and the downed/revive state isn't synced yet.
- The partner is shown in your costume.

## [0.11] — 2026-10-12
### Added
- Optional XInput gamepad for player 2.
### Fixed
- Player 2 keeps his own health, web fluid, cartridges and armour between levels. Before, he got player 1's values.

## [0.10] — 2026-10-12
### Added
- In-game co-op menu (F6) with a built-in 5×7 font. Settings are saved to `coop.ini`.
- Player 2 auto-spawn at level start, and health pickups for player 2.

## [0.9] — 2026-10-11
### Fixed
- The camera broke when player 2 joined during a cut-scene (Black Cat dialog in the first level). Cut-scenes are now
  detected; player 1 is shown full screen while player 2 waits.
### Added
- Player 2's HUD now uses the game's own HUD.

## [0.8]
### Added
- Downed state: death animation, a "!" marker with a countdown above player 2, revive by holding Num0 next to him
  (he gets up with 30% health), and the death counts when time runs out.
- Aim letterbox bars only for the player who aims.
### Fixed
- The revive radius was a few centimetres, so reviving never triggered.

## [0.7]
- Player 2 can roam freely; no teleport back to player 1.
- First version of the downed state.

## [0.6]
- Enemies react to both players and target the nearer one, with hysteresis.
- Both players can aim at the same time.

## [0.5]
- Player 2's controls follow his own camera. Player 2 no longer turns player 1's camera.

## [0.4]
- Player 2 has his own HUD (health, web gauge, cartridges).

## [0.3]
- Real split-screen: the frame is rendered twice, once per player's camera.
