# Contributing to SM2K Co-op

Thanks for wanting to help. Anyone may fork, change and redistribute this project under the MIT license; pull
requests back here are welcome too.

## The one rule

**Never commit anything that comes from the game.** That includes the exe, `.pkr` archives, models, textures,
sounds, level files, decompiled or disassembled code, Ghidra or IDA projects, and memory dumps that contain game
code. Addresses, offsets, calling conventions and a short sentence about what something does are fine: they are
what the mod needs to work with the game. See [docs/LEGAL.md](docs/LEGAL.md). `.gitignore` blocks the common
cases. Please don't fight it.

Pull requests that add a no-CD patch, a crack, or anything else that defeats copy protection will be closed.

## Getting set up

1. Own a copy of Spider-Man (2000) for PC, patched to v1.3.
2. Build `dinput8.dll` (see README: `build.bat`, `make`, or `scripts/build-linux-clang.sh`).
3. Copy it next to `SpideyPC.exe`, play, and read `coop_log.txt`. Almost every behaviour of the mod is logged.

For research, use your own tools on your own copy, for example Ghidra, x32dbg or Cheat Engine. Keep what you learn
as notes and addresses in the source comments.

## Code layout

Everything is in `src/dinput8.c`, split into sections by banner comments:

| Section | What it does |
|---|---|
| version history (top comment) | what changed in each version and why |
| addresses (`A_*`), offsets (`OFF_*`, `E_*`) | game facts, with a comment each |
| player 2 state, carry-over | the second player's health, web and down state |
| split-screen renderer | collects the game's polygon list, draws it into two viewports |
| camera contexts A/B | swaps the camera object and camera globals per player |
| HUD | player 2's HUD (the game's own HUD rerun) and extras |
| co-op menu (F6) | built-in 5×7 font, items, `coop.ini` |
| downed / revive | death animation, marker, countdown, revive |
| enemy targeting | lets enemies pick the closer player |
| LAN co-op | sockets, events, puppet player |
| hooks, `InstallHooks`, `DllMain` | where everything is wired into the game |

Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) before touching hooks.

## Style

- C, compiled by MSVC (x86), MinGW and clang. Put declarations at the start of a block, and don't use C++ or
  compiler-specific features without a macro (see `COOP_ALIGN16`).
- `__fastcall(self, edx, ...)` emulates the game's `__thiscall` methods. Keep that convention for new hooks.
- Every new address gets a `#define` with a comment: what it is, how it was found, and whether it is verified.
- Patch call sites with `PatchCallTo`, which checks the original target. Never patch blindly.
- Log once per event, not every frame. The log is the only way users can help debug.

## Before you open a pull request

- [ ] It builds with at least one toolchain, with no new warnings (`-Wall -Wextra`).
- [ ] You tested it in the game and describe what you tested.
- [ ] Split-screen and single-player still work when your feature is off.
- [ ] No game-derived files were added.
- [ ] `CHANGELOG.md` has a line about the change.
- [ ] If an AI helped write the change, the PR says so (see [AI_DISCLOSURE.md](AI_DISCLOSURE.md)).

## Good first contributions

- Test LAN co-op on different networks and report with logs from both PCs.
- Find addresses for other exe versions (v1.0, regional releases) and add signature-checked support.
- Player 2 costume selection, PvP arenas: see [docs/ROADMAP.md](docs/ROADMAP.md).
- Translations of the docs.
