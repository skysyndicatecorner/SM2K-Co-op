# SM2K Co-op

A fan-made co-op mod for **Spider-Man (2000)**, PC port 2001 (`SpideyPC.exe` v1.3).
Play the whole campaign with a friend:

- **on one PC**, with split-screen and a second keyboard or gamepad;
- **on two PCs**, over LAN or a virtual LAN such as Radmin VPN.

[Русская версия](README.ru.md) · [Download](../../releases/latest) · [Changelog](CHANGELOG.md) · [Roadmap](docs/ROADMAP.md)

> Status: **v0.12, playable beta.** Split-screen co-op has been tested in the game by the author and is playable.
> LAN co-op (phase 1) is new and still in testing. Expect rough edges and send logs.

> This project was built with the help of AI (Claude by Anthropic), under the direction of a human author who
> tested every build in the game. See [AI_DISCLOSURE.md](AI_DISCLOSURE.md).

## What it does

| Mode | State | What you get |
|---|---|---|
| Split-screen (1 PC) | playable | Player 2 is a second Spider-Man with his own camera, own HUD in the game's style, own health and web carried between levels, keyboard or XInput gamepad. Enemies attack both players. A downed player 2 waits for help: player 1 walks up and holds a key to revive him. |
| LAN / Radmin VPN (2 PCs) | testing | Each player runs the game on his own PC and sees the partner moving and animated. Enemy deaths, trigger zones and level changes (end, restart, mission failed) are synced. |
| PvP | planned | Arenas from training and story levels with scripts removed; free-for-all, teams, capture the flag. |

Everything is set up in the mod's own menu: press **F6** in a level.

## Install

You need your own copy of Spider-Man (2000) for PC, updated to **v1.3**. The mod checks the exe and stays
inactive on other versions. The mod contains no game files.

1. Download `sm2k-coop-<version>-win32.zip` from [Releases](../../releases/latest).
2. Copy `dinput8.dll` (and optionally `coop.ini`) into the game folder, next to `SpideyPC.exe`.
3. Start the game and load a level. Press **F6** for the co-op menu.

To uninstall, delete `dinput8.dll`. The mod writes only `coop.ini`, `coop_log.txt` and `coop_log_prev.txt`
next to the game.

### Controls (split-screen)

| Player | Keys |
|---|---|
| Player 1 | the game's own controls |
| Player 2 (keyboard) | W A S D move, K jump, J punch, L kick, I web, U / P / O extra actions, H = Enter |
| Player 2 (gamepad) | stick or d-pad, A jump, B kick, X punch, Y web, LB / RB / RT extra, START = Enter |
| Both | F6 menu, F9 / F11 add / remove player 2, F10 bring player 2 over, F8 split on/off, F7 layout, hold Num0 next to a downed player 2 to revive him |

### Playing over the network

1. **Host:** press F6, set `LAN CO-OP` to `HOST`. Find your IP: `26.x.x.x` in Radmin VPN, or `192.168.x.x`
   from `ipconfig` on a home network.
2. **Friend:** press F6, select `HOST IP`, press Enter, type the IP and press Enter again. Then set
   `LAN CO-OP` to `JOIN`.
3. The status line in the menu should read `OK: <NICK>`. If you are on different levels, the friend can use
   `GO TO PARTNER`.

If Windows Firewall asks about `SpideyPC.exe`, allow it. Without the prompt, add the rule from an admin prompt:
`netsh advfirewall firewall add rule name="SM2K Co-op" dir=in action=allow protocol=UDP localport=27016`.

## Build from source

The mod is one C file (`src/dinput8.c`) that builds into a 32-bit proxy `dinput8.dll`.

| Toolchain | Command |
|---|---|
| Visual Studio (x86 Native Tools prompt) | `build.bat` |
| MSYS2 MINGW32 or Linux `gcc-mingw-w64-i686` | `make` |
| Linux with clang + lld, no MinGW | `scripts/build-linux-clang.sh` (reproducible, used for releases) |

Release DLLs are built by GitHub Actions from the tagged source. You can rebuild them yourself and compare the
SHA-256 (see [SECURITY.md](SECURITY.md)).

## How it works (short)

`dinput8.dll` is a proxy: the game loads it, it forwards `DirectInput8Create` to the real system DLL and patches
a few call sites in the game's code in memory. Player 2 is a second instance of the game's own player object,
updated with its own camera context. Split-screen renders the frame twice into two viewports. In LAN mode the
partner is a player object driven by the network. Details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Contributing

Anyone can take this project and improve it, which is the point of the MIT license. Read
[CONTRIBUTING.md](CONTRIBUTING.md) first. One rule matters more than the others: **never commit game files, the
exe, or decompiled or disassembled game code.**

## Legal

SM2K Co-op is an unofficial fan project. It is not affiliated with or endorsed by Marvel, Activision or any
other rights holder. Spider-Man and related names are trademarks of their owners and are used here only to say
which game the mod works with. The repository contains only original code and documentation. You need your own
copy of the game. More: [docs/LEGAL.md](docs/LEGAL.md).

Code: [MIT](LICENSE).
