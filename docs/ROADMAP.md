# Roadmap

Items are roughly in priority order. Want one? Comment on the matching issue or open one.

## Split-screen (playable)
- [ ] Player 2 costume selection.
- [ ] Shared end-of-level stats: enemies beaten and revives per player.

## LAN co-op
**Phase 1 (v0.12, testing):** partner puppet; sync of enemy deaths, trigger zones, level end, restart and mission
failed.

**Phase 2:**
- [ ] Damage to the puppet goes to its owner (`Hit` message forwarded instead of swallowed).
- [ ] Downed / revive over the network, the same as split-screen player 2.
- [ ] The partner's costume on the puppet: load a second costume model, using the model slot at entity `+0x1F`.
- [ ] Nick and ready check in the menu before the level starts.

**Phase 3:**
- [ ] Enemies whose `Hit` ignores network kills (Black Cat, Lizard, clones, …): remove them directly and fire their
      death trigger.
- [ ] Shared cut-scenes: wait for both players.
- [ ] Boss fights.

## PvP ("Versus")
Research is done. Planned order:
1. **Load an arena from the mod menu.** Write the level name to `0x68285C`, then set game state 8. Good arenas are
   the training halls `LBA1–4` (survival) and `LCA1–4` (rooftops).
2. **Turn off scripts.** Skip the trigger spawn calls (`0x4DEB01`, `0x4DEDA2`, per frame `0x45551D`), so no
   enemies or cut-scenes.
3. **Melee hits on players.** Melee only checks the enemy list (`0x4C7120`, called from `0x4B3178`); a player
   check is needed there. Both players use pinned real health with a virtual health bar.
4. **Web attacks on players** (`0x4F9C7F`, `0x4C6455`).
5. **Modes:** free-for-all, then teams, then capture the flag, with a pickup as the flag.
6. **Different costumes** for both players (hard: one player model per level).

## Technical
- [ ] A fallback loader (a different proxy DLL name or an ASI loader) in case `dinput8.dll` proxies stop working.
- [ ] Support for other exe versions.
- [ ] Code signing for release builds, to reduce antivirus false positives.
- [ ] Split `src/dinput8.c` into modules once there is a test setup.
