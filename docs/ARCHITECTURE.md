# Architecture

How SM2K Co-op plugs into Spider-Man (2000, PC, `SpideyPC.exe` v1.3: 32-bit, image base `0x400000`, no ASLR).
All addresses below are virtual addresses in that exe.

## Engine facts the mod relies on

- The PC version is a port of the PlayStation engine, drawn through Direct3D 7.
- Fixed point is `4096 = 1.0`. Positions are `int32` in game units `<< 12`; world "up" is −Y.
- **Entities** live in doubly linked lists (`next` at +0x20, `prev` at +0x34). The player list head
  `0x6A9038` is also "the player" for most of the game's code.
- **Entity update**: `UpdateList` (`0x460FC0`) calls `PreUpdate` (`0x460ED0`), then `AnimTick`
  (`0x460DA0`, only when `+4 & 2`), then the virtual `Update` (vtable slot `+8`). Skipping `Update` freezes an
  entity while its animation keeps playing.
- **Game state** `0x60CFA4`:

  | Value | Meaning |
  |---|---|
  | 0 | playing |
  | 1 | restart level |
  | 2 | mission failed |
  | 3 | next level (name at `0x68285C`) |
  | 6 | checkpoint restart |
  | 7 | menu |
  | 8 | restart (pause menu) |
  | 9 | same `.trg` |

  `GameLoop` (`0x4559D0`) leaves right after `UpdateWorld` when the state is not 0. Then `DestroyAll`
  (`0x4437E0`) unloads the level.
- **Camera**: a single global camera object (`0x56F3B8`, size `0x2F4`) plus a block of camera globals
  (`0x56F000..0x56F400` and a few others). Player update code reads and writes it.
- **Rendering**: `RenderFrame` (`0x4555A0`) fills an ordering table of `0x1001` buckets (`0x6B7AA0`) and draws it.

## Proxy DLL

The game imports `DirectInput8Create` from `dinput8.dll`. The mod's `dinput8.dll` sits in the game folder,
forwards that call to the real DLL from the system directory, and on the first call (outside the loader lock)
runs `InstallHooks()`:

1. **Signature check** of five code locations. A different exe version → nothing is patched.
2. `DestroyAll`: its first 6 bytes become a `jmp` to `Hook_DestroyAll`; a trampoline runs the original.
3. The single `call UpdateWorld` in `GameLoop` → `Hook_UpdateWorld`. This is the per-frame entry point of the mod.
4. `call RenderFrame` → `Hook_Render` (split-screen), `call DrawHud` → `Hook_Hud`.
5. Smaller call-site patches through `PatchCallTo`, which verifies the original target first: AddHealth (pickups
   for player 2), the timer HUD, the entity-update distance checks (enemy targeting), and CommandPoint (LAN trigger
   sync).

## Player 2 (split-screen)

- Created with the game's own allocator and `CPlayer` constructor (`0x455390`, `0x4B9EB0`). Camera globals
  are snapshotted around the constructor. Player 2 is then moved behind player 1 in the list.
- Gets his own pad block, fed from keys or XInput through the game's `PadEntry` (`0x479540`).
- Gets a **private copy of the vtable**: its `Update` slot is a thunk that swaps in **camera context B** (camera
  object + camera globals + aim globals + letterbox) before the real `Update`, then swaps context A back. So
  player 2 moves relative to his own camera and never moves player 1's camera.
- **Health**: player 2's real health is pinned at 6× max so the game never kills him. Damage is the drop from
  the pinned value, booked on a virtual health bar. An empty bar → the mod's downed state: death animation `0xAB`,
  marker and countdown, revive with get-up animation `0xB1`.
- **Carry-over**: health, web and armour are saved at `DestroyAll` and restored on the next spawn. Player 2 is
  unlinked (never destroyed) before the game's sweep, because the `CPlayer` destructor writes the carry-over globals.

## Split-screen renderer

`Hook_Render` runs the game's `RenderFrame` twice: with camera context A, then B. Each time it collects the
ordering-table nodes into a pool, scales and clips them into that player's viewport, and draws them. The HUD
call is rerun for player 2 with his values temporarily in place. HUD animation globals are saved and restored, so
nothing runs twice.

## Enemy targeting

For every entity update, the hooked distance checks make the **nearer** player "the player" (`0x6A9038`) for the
duration of that entity's `Update`, with hysteresis. Enemies then pick a target and wake up for either player.

## LAN co-op (phase 1)

- **Transport.** UDP. A network thread receives into a queue; the game thread processes it in
  `Lan_PreWorld` / `Lan_PostWorld` around `UpdateWorld`. Every datagram carries:

  | Field | Size or purpose |
  |---|---|
  | header | 20 bytes: magic `SPLN`, protocol, sequence, event ack, session id |
  | body | optional: a 72-byte player snapshot or a 32-byte HELLO |
  | events | up to 12 reliable 64-byte events, resent until acknowledged |

- **Puppet.** The partner is a second `CPlayer` whose vtable `Update` is replaced by `Puppet_Update`. It
  interpolates position and angles 100 ms in the past, starts the partner's animation with `SetAnim` (`0x460C10`)
  or `CycleAnim` (`0x460D00`), then runs `0x460E80` (hook points + render matrix), as `CPlayer::Update` does at its
  end. `Hit` (vtable `+0xC`) is swallowed.
- **World events** (each tagged with a hash of the level name):

  | Event | Detection | Applied on the other PC |
  |---|---|---|
  | `EV_KILL` | an enemy seen alive reaches hp ≤ 0; matched by spawn node `+0xDE` | a lethal message through the enemy's own `Hit`; echo suppressed |
  | `EV_CMDPOINT` | a trigger zone fires (`0x4DE8D0`); zones fire once, so a duplicate is harmless | the same zone is fired |
  | `EV_LEVEL` | game state 1/2/3/8/9 at `DestroyAll` | the same state and level name are written. A generation counter prevents loops; on a tie the host wins |

## Logging

Everything important goes to `coop_log.txt` (the previous run is kept as `coop_log_prev.txt`). A vectored
exception handler logs crashes with registers, stack and the mod's current stage.

## Address map

These are all the `A_*` addresses defined in `src/dinput8.c` (generated from the source). Offsets inside objects
(`OFF_*`, `E_*`, `CAM_*`) are documented next to their `#define`.

| Name | Address | Notes |
|---|---|---|
| `A_PLAYER_HEAD` | `0x006A9038` | head of CPlayer list (also "the" player pointer) |
| `A_GAME_STATE` | `0x0060CFA4` | 0 while a level is being played |
| `A_ALLOC` | `0x00455390` | void* __cdecl alloc(int size), returns zeroed |
| `A_PLAYER_CTOR` | `0x004B9EB0` | CPlayer* __fastcall ctor(this in ECX) |
| `A_PAD_ENTRY` | `0x00479540` | void __cdecl PadEntry(char* entry, int pressed) |
| `A_UPDATEWORLD` | `0x00455400` | void __cdecl UpdateWorld(void) |
| `A_UPDATEWORLD_CALL` | `0x00455A8B` | the single "call UpdateWorld" inside GameLoop |
| `A_DESTROYALL` | `0x004437E0` | void __cdecl DestroyAll(void) |
| `A_CAMERA` | `0x0056F3B8` | global pointer to the camera object (size 0x2F4) |
| `A_RENDER` | `0x004555A0` | void __cdecl RenderFrame(void) (called from GameLoop) |
| `A_MAINLOOP_LO` | `0x004559D0` | GameLoop function range, used to find the call site |
| `A_MAINLOOP_HI` | `0x00455C90` |  |
| `A_BEGIN_SCENE` | `0x00505E00` | void __cdecl BeginFrame(int,int): lazily called by the first draw of a frame |
| `A_SW_RENDER` | `0x006B78F8` | int: 0 = Direct3D hardware path |
| `A_D3D_DEVICE` | `0x006B791C` | IDirect3DDevice7* |
| `A_OT_HEADS` | `0x006B7AA0` | 0x1001 bucket heads, drawn from index 0x1000 down to 0 |
| `A_SCENE_FLAG` | `0x006BBAA4` | byte: BeginScene done |
| `A_ZBUF_FLAG` | `0x006BBAA5` | byte: z-buffer mode (opaque polys are drawn at once) |
| `A_NODE_COUNT` | `0x00AC08F4` | nodes used this frame (max 0x3C00) |
| `A_SCREEN_W` | `0x00568154` | int, screen width in pixels |
| `A_SCREEN_H` | `0x00568158` | int, screen height in pixels |
| `A_HUD_DRAW` | `0x004658C0` | void __cdecl DrawHud(void): reads the player-list head (DAT_006A9038) |
| `A_HUD_CALL` | `0x0045561A` | the single "call DrawHud" inside RenderFrame |
| `A_HUD_TIMER` | `0x00461D00` | void DrawTimerHud(void): mission timer + level-script HUD callbacks |
| `A_HUD_TIMER_CALL` | `0x00465960` | the "call 0x461D00" inside DrawHud |
| `A_LEVEL_ID` | `0x004DE770` | u32 __cdecl LevelId(void): 0x201 = level 2 act 1, ... |
| `A_HUD_ENABLED` | `0x0054D47C` | int: DrawHud draws the player part only when != 0 |
| `A_HUD_ARMOR_FACE` | `0x0060F754` | frame*: "costarm" face icon, set while the armour power-up is on |
| `A_HUD_BOSS` | `0x0060F788` | entity*: the boss whose bar FUN_00464270 draws (0 = none) |
| `A_HUD_COMPASS_ON` | `0x0060F77C` | byte: compass target set (FUN_00463800 / FUN_00463850) |
| `A_PLAYER_ANIM` | `0x004C9130` | void __thiscall CPlayer::PlayAnim(int anim, int first, int last) |
| `A_PLAY_SFX` | `0x00471EA0` | int __cdecl PlaySfx(int id, int *pos, int flags) |
| `A_DIE` | `0x004BDFF0` | void __thiscall CPlayer::Die(char outcome): 1 = mission failed |
| `A_AIM_EXIT` | `0x004C3810` | void __thiscall CPlayer::LeaveAim(void) |
| `A_WEBLINE_FREE` | `0x004F6260` | void __thiscall WebLine::Release(void) |
| `A_CAM_MATRIX` | `0x0056F224` | short[9] camera rotation (4096 = 1.0), inside the camera block |
| `A_CAM_POS` | `0x0056F1B4` | int[3] camera position in game units (raw >> 12) |
| `A_PROJ_H` | `0x0054F03C` | int projection distance (276) |
| `A_PROJ_W` | `0x0054F040` | int virtual screen width  (512) |
| `A_PROJ_HGT` | `0x0054F044` | int virtual screen height (240) |
| `A_LETTERBOX` | `0x00660F80` | int aim/cut-scene bar height, drawn by FUN_004737F0 |
| `A_LETTERBOX2` | `0x0060F76C` | int the HUD's copy of it |
| `A_CAM_SNAP` | `0x00416AA0` | void __thiscall Camera::SnapToFocus(void), acts only in mode 3 |
| `A_PSX_W` | `0x0061B5FC` | virtual screen width the renderer scales from (FUN_004737F0 uses it) |
| `A_PSX_H` | `0x00628614` | virtual screen height |
| `A_PLAYER_VTBL_UPDATE` | `0x004C65C0` |  |
| `A_LIST_UPDATE` | `0x00460FC0` | void __cdecl UpdateList(void *listHeadPtr, int) |
| `A_DIST` | `0x004E6220` | int __cdecl Dist(const int *a, const int *b) (fast, >>12) |
| `A_DIST_CALL_ACTIVE` | `0x0046102E` | call Dist inside UpdateList: awake entity vs "the player" |
| `A_DIST_CALL_WAKE` | `0x00461187` | call Dist inside the sleeping-entity check |
| `A_PRE_UPDATE` | `0x00460ED0` | void __thiscall PreUpdate(entity) |
| `A_PRE_CALL_1` | `0x00461111` |  |
| `A_PRE_CALL_2` | `0x00461126` |  |
| `A_ADDHEALTH` | `0x004BD750` |  |
| `A_ADDHEALTH_CALL1` | `0x0046908F` |  |
| `A_ADDHEALTH_CALL2` | `0x0046B8A4` |  |
| `A_FRAME_COUNTER` | `0x006B4CA8` |  |
| `A_ENT_BODYUPD` | `0x00460E80` | void thiscall Ent::UpdateBody(desc): hook points + matrix (end of CPlayer::Update) |
| `A_UPDMATRIX` | `0x00475F50` | void cdecl UpdateMatrix(ent): angles +0x14 -> matrix +0x164, pos >> 12 -> +0x178 |
| `A_HOOKPOINTS` | `0x00454450` | void cdecl CalcHookPoints(ent) (needs +0x188, allocated by A_ENT_BODYUPD) |
| `A_PLAYER_COLL` | `0x005564E4` | collision descriptor CPlayer passes to A_ENT_BODYUPD |
| `A_SETANIM` | `0x00460C10` | void thiscall SetAnim(u16 anim, int first, int last) |
| `A_CYCLEANIM` | `0x00460D00` | void thiscall CycleAnim(u16 anim, s8 dir) |
| `A_PLAYER_HITFN` | `0x004BD890` | CPlayer vtable slot +0xC: int thiscall Hit(msg*) |
| `A_MODEL_SLOTS` | `0x006B2440` | 40 model slots, stride 0x44; +0x1C = animation table (dword 0 = count) |
| `A_BADDIES` | `0x0056E990` | list head: enemies |
| `A_SLEEPERS` | `0x0060DAB4` | list head: sleeping entities |
| `A_CMDPOINT` | `0x004DE8D0` | CommandPoint_Trigger(key): fires a trigger zone once (+5 = fired) |
| `A_LEVEL_NAME` | `0x0068285C` | level file name, 9 bytes ("l1a2_t") |
| `A_RESTART_NAME` | `0x00682865` | restart point name, up to 50 chars |
| `A_DIFFICULTY` | `0x0054D474` |  |
