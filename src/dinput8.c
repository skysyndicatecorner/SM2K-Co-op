/* SPDX-License-Identifier: MIT
 * SM2K Co-op - fan-made co-op mod for Spider-Man (2000, PC port 2001, SpideyPC.exe v1.3).
 * Copyright (c) 2026 SM2K Co-op contributors. See LICENSE and docs/LEGAL.md.
 * Not affiliated with or endorsed by Marvel, Activision or any rights holder of the game.
 * Written with the help of an AI assistant (Claude by Anthropic) under human direction and in-game testing;
 * see AI_DISCLOSURE.md. Contains no code or data from the game: only addresses and notes needed to interoperate.
 */
/*
 * Spider-Man (2001 PC) - COOP prototype v0.12  (LAN CO-OP + IN-GAME MENU + SPLIT-SCREEN + GAME-STYLE P2 HUD + P2 OWN CAMERA + ENEMIES SEE BOTH PLAYERS + DOWNED/REVIVE + CUT-SCENE SAFE)
 *
 * v0.12 changes (vs 0.11):
 *   - LAN co-op, phase 1: each player runs the game on his own PC (LAN or Radmin VPN, UDP 27016). F6 menu:
 *     LAN CO-OP = HOST / JOIN, HOST IP, PORT, SYNC ENEMIES, GO TO PARTNER; coop.ini [lan] mode/host_ip/port/nick.
 *     The partner is shown as a network-driven second Spider-Man ("puppet"). Synced: enemy deaths (by trigger node),
 *     trigger zones, level end / restart / mission failed (both PCs load together). In LAN mode the local player 2,
 *     split-screen and auto-spawn are off. Session ids make a network restart on one side safe.
 *
 * v0.11 changes (vs 0.10):
 *   - P2 keeps his own health, web fluid, cartridges and armour between levels (before, the new P2 of the next level
 *     got full health and the game's carry-over web values, i.e. P1's). Saved at DestroyAll / removal, restored at
 *     spawn; a checkpoint restart (game state 2/9) restores the values P2 had when the level started.
 *   - optional XInput gamepad for P2 (menu item P2 GAMEPAD, coop.ini p2_gamepad): stick/d-pad move, A jump, B kick,
 *     X punch, Y web, LB/RB/RT extra actions, START = Enter. Off by default: the game itself may also read that pad.
 *
 * v0.10 changes (vs 0.9):
 *   - in-game co-op menu (F6, in a level): the world is paused while it is open (UpdateWorld is not called).
 *     Items: spawn/remove P2, split on/off, layout, P2 HUD style, revive time, what happens when P2 bleeds out,
 *     auto-spawn. Drawn with a built-in 5x7 font from plain quads. Settings are saved to coop.ini next to the exe.
 *   - auto-spawn: P2 is created ~1.5 s after a level / checkpoint restart starts (not during cut-scenes);
 *     removing P2 by hand (F11 / menu) switches it off until the next level.
 *   - health pickups heal P2: the two AddHealth (FUN_004BD750) call sites are hooked; for P2 the heal goes to his
 *     virtual health (the game only heals when +0xE2 < +0xEF0, and P2's +0xE2 is pinned at 6 x max).
 *   - revive time, bleed-out outcome and P2 HUD style are runtime settings now (defaults = the #defines).
 *
 * v0.9 changes (vs 0.8):
 *   - CUT-SCENE FIX (CUTSCENE_FIX): P2 is removed at every level load and the Black Cat dialog starts as soon as
 *     L1A2 is entered, so F9 was pressed during it: InitCamB copied camera A in the scripted mode 5 (+ bars) and
 *     nothing ever switched camera B back. Now cut-scenes are detected (bars 0x660F80 while P1 is not aiming,
 *     IgnoreBruceInput P1+0xE18 >= 16); during them P1 is shown full screen, P2 waits (no input, no damage,
 *     countdown paused, F9 deferred); afterwards camera B is rebuilt from A, P2 is put next to P1 if far away.
 *     InitCamB never inherits modes 4/5/6/7 or the bars (forced mode 3 + the game's snap FUN_00416AA0).
 *   - GAME-STYLE P2 HUD (P2_HUD_STYLE 1): P2's view runs the game's own DrawHud (health bar, web gauge,
 *     cartridges, compass, timer, boss bar) with P2's virtual health written into +0xE2 for that one call;
 *     shared HUD globals (timer/compass flash, RNG) are saved/restored around it, level-script timers are not
 *     run twice. P2 badge, death counter and the downed badge are drawn on top. P2_HUD_STYLE 0 = v0.8 HUD.
 *
 * HUD patch (on top of v0.8):
 *   - P2's HUD now uses the GAME'S OWN HUD (P2_HUD_STYLE 1): in P2's pass the game's DrawHud (0x4658C0) runs with
 *     the list head = P2 (as every other part of that pass). For the duration of that one call:
 *       * P2's real health (+0xE2, pinned at 6 x max) is replaced by the virtual health scaled to +0xEF0 (the max
 *         the HUD divides by) and put back right after. (v0.4 saw "broken values": with 6 x max the HUD's black
 *         "missing health" rectangle got a NEGATIVE width.)
 *       * the globals DrawHud animates (timer/compass flashes, the game's RNG, the timer stamp) are saved before and
 *         restored after, so running the HUD twice per frame does not double any animation or consume random numbers.
 *       * the armour face icon (0x60F754, P1's armour) is hidden unless P2 himself has armour (+0x5E9).
 *       * the timer sub-call (0x461D00) is hooked: on the 4 levels where it only calls a level-script HUD function
 *         (0x201, 0x501, 0x505, 0x806, unknown side effects) it is skipped in P2's pass.
 *     The game's HUD works in its 512x240 virtual screen scaled by the screen size globals, so the split-screen
 *     code scales it into P2's half exactly like P1's HUD. Compass (camera B -> correct for P2), mission timer and
 *     boss bar are shown in both views (P2_HUD_COMPASS / P2_HUD_TIMER / P2_HUD_BOSSBAR).
 *   - small extra elements in the game's layout: a "P2" badge, the death counter and a downed badge (m:ss).
 *   - P2_HUD_STYLE 0 (or a hook that could not be installed) = the v0.8 flat-quad HUD (DrawP2Hud).
 *
 * v0.8 changes (vs 0.7):
 *   - DOWNED state finished:
 *       * when P2's bar is empty he first finishes his fall (input off, max P2_FALL_WAIT_MS ms, until he touches
 *         the ground: CPlayer+0xE0 bit 1), then plays the game's own death animation (anim 0xAB + death sound 0x24,
 *         the same pair the game's Die() at 0x4BDFF0 uses) and STAYS LYING where he fell. His Update stays skipped,
 *         so the game's "dead -> game over after 120 frames" logic (state 0x80, 0x4B2892) never runs for him; the
 *         animation itself is advanced by UpdateList (0x460DA0) and holds its last frame.
 *       * a red "!" marker is drawn ABOVE P2 in the 3D view of BOTH players (projected with the game's own camera
 *         matrix 0x56F224 / camera position 0x56F1B4 / projection 0x54F03C.., exactly like DrawReticle does), with a
 *         m:ss countdown under it. If P2 is off screen the marker sticks to the edge of the view (a pointer).
 *       * P1 revives him by standing next to him and HOLDING Num0 (or Insert) for P2_REVIVE_HOLD_FRAMES frames: an
 *         animated key prompt (pulsing key cap, pressed look while held, progress bar) is shown under the marker.
 *         P2 then plays the game's get-up animation (0xB1 in knock-down state 0x800000, as after a knock-down)
 *         and continues with P2_REVIVE_HP_PCT % (30 %) health.
 *       * FIX: v0.7 compared the RAW distance with 0x2800 (= 2.5 game units = a few centimetres), so walking up
 *         to P2 could never revive him. The radius is now P2_REVIVE_RADIUS_U game units (raw >> 12).
 *       * if the countdown runs out the death COUNTS: the marker turns grey, P2's death counter (in his HUD) goes up
 *         and after P2_DEATH_DELAY_MS the game's own death outcome runs (P2_DEATH_MODE 1 = same as Spider-Man dying:
 *         mission failed / checkpoint, via Die(1) 0x4BDFF0). P2_DEATH_MODE 0 = P2 gets up next to P1 instead.
 *   - aim bars: the black bars at the top and bottom while aiming are DAT_00660F80 (bar height, drawn by
 *     FUN_004737F0 in every RenderFrame) + its HUD copy DAT_0060F76C. They are now part of each player's camera
 *     context, so only the player who aims gets them. Cut-scene bars (set by scripts in P1's context) are still
 *     shown in both views.
 *
 * v0.7 changes (vs 0.6):
 *   - P2 may roam freely: the leash (teleport back to P1 when far away) is OFF (P2_LEASH_ENABLED).
 *   - P2 no longer teleports when his bar is empty. He is DOWNED instead: he stays where he is, his Update is
 *     skipped (he stands/lies frozen in his last pose), his input is ignored and enemies ignore him.
 *       * a 60 s countdown starts (P2_DOWN_SECONDS). It is shown in the game window's title bar
 *         ("! P2 DOWN - revive in 0:42") and in the log - drawing it inside the 3D view needs the game's text
 *         routines, which are not hooked yet.
 *       * if P1 stands within P2_REVIVE_RADIUS of P2 for P2_REVIVE_HOLD_FRAMES frames, P2 is revived with
 *         P2_REVIVE_HP_PCT % of his health (and P2_REVIVE_INVULN_FRAMES frames of protection).
 *       * if the countdown runs out the death is counted: P2 stays down until the level changes
 *         (F10 = emergency revive next to P1, F9 after F11 = new P2).
 *     NOT done yet (needs more reverse engineering): death/lying animation, the "!" marker above P2 and
 *     the button-press animation of P1 when reviving.
 *   - aim mode: the YAW accumulators (DAT_006A7FFC, DAT_006A8D54) are now part of each player's camera context
 *     like the pitch ones; "aim dbg" log lines print the aim state of both players while aiming.
 *
 * v0.6 changes (vs 0.5):
 *   - NEW: enemies (and every other world entity) now react to BOTH players. The game updates all
 *     entities in one loop (FUN_00460FC0). For each entity that loop measures the distance to "the player"
 *     (the list head DAT_006A9038 = P1; entities far from it are put to sleep) and the entity's Update reads
 *     the same global to find its target. We hook that loop (3 call sites + the 9 callers) and, per
 *     entity, make the NEAREST player "the player" for the duration of that entity's Update.
 *       * hysteresis, so enemies do not flip between players: a target is only changed after
 *         ENEMY_RETARGET_FRAMES frames AND only if the other player is clearly closer
 *         (ENEMY_RETARGET_PCT_SQ), and never while the enemy is within ENEMY_LOCK_DIST of its target.
 *       * sleeping enemies wake up when EITHER player is near.
 *       * if P1 is dead, everybody targets P2.
 *   - aim mode state that the game keeps in plain globals (DAT_006A818C, DAT_006A8D44, DAT_006A82B4) is now
 *     part of each player's camera context, so P1 and P2 can aim at the same time without disturbing each other.
 *   - log lines for camera-mode changes of both cameras and for the aim flag of both players (to diagnose
 *     the aiming view), plus enemy targeting statistics every 300 frames.
 *
 * v0.5 changes (vs 0.4):
 *   - FIX: Player 2's controls were relative to PLAYER 1's camera and Player 2's movement states
 *     (run / jump / web-swing / land) rewrote PLAYER 1's camera settings (angle/distance presets,
 *     camera position, swing look-at matrices). The game has ONE global camera; every CPlayer::Update
 *     (vtable slot +8 = 0x4C65C0) reads and writes it. Now P2 gets its own copy of the vtable whose
 *     Update slot is a thunk: it swaps in camera B (P2's camera) before P2's Update and swaps camera A
 *     back afterwards. So W/A/S/D move P2 relative to what P2 sees, and P2 can no longer turn P1's
 *     camera. (Only active while split-screen is on and camera B exists; otherwise P2 behaves as v0.4.)
 *   - the saved camera state now also includes the yaw tween rate DAT_0056EFF8 (written by FUN_00417AE0),
 *     which lies just outside the old 0x56F000..0x56F400 block.
 *
 * v0.4 changes (vs 0.3b):
 *   - The game's HUD is no longer drawn in P2's half of the screen. The game draws its HUD from the
 *     head of the player list, so in pass 2 it showed a P2 HUD with broken values (P2's real health
 *     is pinned to a high value) or P1's. Its single call site in RenderFrame (0x45561A) is now
 *     redirected and skipped during pass 2. (The compass is part of that HUD and disappears too.)
 *   - P2 has its own HUD, drawn by this DLL with plain coloured quads:
 *       row 1: "P2" tag + health bar        (virtual P2 health, as before)
 *       row 2: web-fluid gauge             (CPlayer+0x5D4, 0x1000 = full cartridge)
 *       row 3: cartridge icon, 2-digit count and 10 pips   (CPlayer+0x5D8)
 *     All values are P2's own: P2's web use drains P2's gauge and cartridges.
 *
 * v0.3b: the game begins its Direct3D scene LAZILY (on the first draw call of the frame), so the
 *        split-screen code now starts the scene itself; every reason for not splitting is logged.
 *
 * v0.3 changes (vs 0.2):
 *   - REAL SPLIT-SCREEN: each player has his own camera and his own view.
 *     The game's frame renderer is run twice per frame (once for P1, once for P2);
 *     the polygons of each pass are scaled/clipped into their half of the screen.
 *   - P2 has its own health bar in his half of the screen (the game's HUD is P1-only).
 *   - FIXED the crash of v0.2 (web-swing / roof jump): the "midpoint camera" used a fake
 *     entity without a model; the camera asked it for model hook points -> NULL deref.
 *     The midpoint camera is gone (split-screen replaces it).
 *   - P2 health is now taken from the game's own constructor value (v0.1/0.2 forced 600
 *     while Spider-Man has 100). P2 cannot die: damage is tracked on the P2 health bar,
 *     when the bar is empty P2 is "revived" next to P1.
 *
 * dinput8.dll proxy: forwards DirectInput8Create to the real DLL and, on first
 * call, installs hooks into SpideyPC.exe (v1.3, ImageBase 0x400000, no ASLR).
 *
 * Keys:
 *   F9   spawn Player 2 (a second CPlayer) next to Player 1
 *   F10  teleport Player 2 to Player 1 (if P2 is down: emergency revive with 30 %)
 *   F11  remove Player 2 from the player list
 *   Num0 (or Insert), held by Player 1 next to a downed Player 2: revive (~1.5 s)
 *   F8   split-screen on / off
 *   F7   split layout: top/bottom -> left/right -> left/right (keep aspect) -> top/bottom (keep aspect)
 * Player 2 keys: W/A/S/D move, K jump, J punch, L kick, I web, U/P/O extra
 * actions (= numpad 7/3/9 of Player 1), H = Enter-equivalent.
 *
 * Build: 32-bit (x86) only. See README.md.
 */
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS      /* inet_addr */
#include <winsock2.h>                 /* v0.12 LAN co-op (must come before windows.h) */
#ifndef SIO_UDP_CONNRESET                    /* lives in mstcpip.h */
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include <windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#if defined(_MSC_VER)
#define COOP_ALIGN16 __declspec(align(16))
#else
#define COOP_ALIGN16 __attribute__((aligned(16)))
#endif

typedef uint8_t  u8;
typedef int8_t   s8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;

/* ------------------------------------------------------------------ */
/*  Game addresses (SpideyPC.exe 1.3, built 2001-08-15)                */
/* ------------------------------------------------------------------ */
#define A_PLAYER_HEAD        0x006A9038u  /* head of CPlayer list (also "the" player pointer) */
#define A_GAME_STATE         0x0060CFA4u  /* 0 while a level is being played                  */
#define A_ALLOC              0x00455390u  /* void* __cdecl alloc(int size), returns zeroed    */
#define A_PLAYER_CTOR        0x004B9EB0u  /* CPlayer* __fastcall ctor(this in ECX)            */
#define A_PAD_ENTRY          0x00479540u  /* void __cdecl PadEntry(char* entry, int pressed)  */
#define A_UPDATEWORLD        0x00455400u  /* void __cdecl UpdateWorld(void)                   */
#define A_UPDATEWORLD_CALL   0x00455A8Bu  /* the single "call UpdateWorld" inside GameLoop    */
#define A_DESTROYALL         0x004437E0u  /* void __cdecl DestroyAll(void)                    */

#define A_CAMERA             0x0056F3B8u  /* global pointer to the camera object (size 0x2F4)   */
#define CAM_OFF_FOCUS        0xFCu    /* camera+0xFC = entity the camera follows (P1)         */
#define CAM_OFF_FOCUS2       0x13Cu   /* camera+0x13C = copy of the focus used by look-at     */
#define CAM_OFF_MODE         0x2A0u   /* camera mode, 3 = normal follow                       */
#define CAM_SIZE             0x2F4u

/* renderer (Direct3D 7, "PS1-like" ordering table of pre-transformed polygons) */
#define A_RENDER             0x004555A0u  /* void __cdecl RenderFrame(void) (called from GameLoop) */
#define A_MAINLOOP_LO        0x004559D0u  /* GameLoop function range, used to find the call site   */
#define A_MAINLOOP_HI        0x00455C90u
#define A_BEGIN_SCENE        0x00505E00u  /* void __cdecl BeginFrame(int,int): lazily called by the first draw of a frame */
#define A_SW_RENDER          0x006B78F8u  /* int: 0 = Direct3D hardware path                       */
#define A_D3D_DEVICE         0x006B791Cu  /* IDirect3DDevice7*                                     */
#define A_OT_HEADS           0x006B7AA0u  /* 0x1001 bucket heads, drawn from index 0x1000 down to 0 */
#define OT_BUCKETS           0x1001
#define A_SCENE_FLAG         0x006BBAA4u  /* byte: BeginScene done                                 */
#define A_ZBUF_FLAG          0x006BBAA5u  /* byte: z-buffer mode (opaque polys are drawn at once)  */
#define A_NODE_COUNT         0x00AC08F4u  /* nodes used this frame (max 0x3C00)                    */
#define A_SCREEN_W           0x00568154u  /* int, screen width in pixels                           */
#define A_SCREEN_H           0x00568158u  /* int, screen height in pixels                          */

#define SPLIT_DEFAULT        1        /* 1 = split-screen is ON as soon as P2 exists (F8 toggles) */
#define SPLIT_LAYOUT_DEFAULT 0        /* 0 top/bottom, 1 left/right, 2 left/right keep aspect, 3 top/bottom keep aspect */
#define P2_HIDE_GAME_HUD     1        /* in P2's view also hide the game's message overlays (4 gate flags) */
#define P2_CUSTOM_HUD        1        /* skip the game's HUD in P2's view and draw our own (health, web, cartridges) */
#define A_HUD_DRAW           0x004658C0u  /* void __cdecl DrawHud(void): reads the player-list head (DAT_006A9038)  */
#define A_HUD_CALL           0x0045561Au  /* the single "call DrawHud" inside RenderFrame                          */
#define OFF_WEB_FLUID        0x5D4u   /* int: web fluid left in the current cartridge, 0x1000 = full             */
#define OFF_WEB_CARTS        0x5D8u   /* int: spare web cartridges (the game allows up to 10)                    */
#ifndef P2_HUD_STYLE
#define P2_HUD_STYLE         1        /* with P2_CUSTOM_HUD: 1 = the game's own DrawHud drawn for P2 (game look), 0 = v0.8 flat quads */
#endif
#ifndef P2_HUD_TIMER
#define P2_HUD_TIMER         1        /* style 1: show the (shared) mission timer in P2's view too                 */
#endif
#ifndef P2_HUD_BOSSBAR
#define P2_HUD_BOSSBAR       1        /* style 1: show the boss health bar in P2's view too                        */
#endif
#ifndef P2_HUD_COMPASS
#define P2_HUD_COMPASS       1        /* style 1: show the compass in P2's view (drawn with camera B = P2's view)  */
#endif
#ifndef P2_HUD_TAG
#define P2_HUD_TAG           1        /* style 1: small "P2" badge next to the health bar                          */
#endif
#define P2_TAG_VX            0x9C     /* "P2" badge position in the HUD's 512x240 virtual screen (+ HUD offset)   */
#define P2_TAG_VY            0x16
#define P2_EXTRA_VX          0x20     /* death counter / downed badge row (below the web cartridge counter)       */
#define P2_EXTRA_VY          0x56
#define A_HUD_TIMER          0x00461D00u  /* void DrawTimerHud(void): mission timer + level-script HUD callbacks      */
#define A_HUD_TIMER_CALL     0x00465960u  /* the "call 0x461D00" inside DrawHud                                      */
#define A_LEVEL_ID           0x004DE770u  /* u32 __cdecl LevelId(void): 0x201 = level 2 act 1, ...                  */
#define A_HUD_ENABLED        0x0054D47Cu  /* int: DrawHud draws the player part only when != 0                       */
#define A_HUD_ARMOR_FACE     0x0060F754u  /* frame*: "costarm" face icon, set while the armour power-up is on        */
#define A_HUD_BOSS           0x0060F788u  /* entity*: the boss whose bar FUN_00464270 draws (0 = none)               */
#define A_HUD_COMPASS_ON     0x0060F77Cu  /* byte: compass target set (FUN_00463800 / FUN_00463850)                 */
#define OFF_MAXHP            0xEF0u   /* int: max health, = health set by the ctor (600/200/100/80)              */
#define OFF_ARMOR_ON         0x5E9u   /* byte: armour power-up active (red bar over the health bar)              */
#define OFF_HUD_OFF1         0xE18u   /* int:  != 0 -> DrawHud skips the player part                             */
#define OFF_HUD_OFF2         0x1ACu   /* byte: != 0 -> DrawHud skips the player part                             */
#define P2_LEASH_ENABLED     0           /* 0 = P2 may roam freely, 1 = teleport P2 back when far (v0.6)   */
#define P2_DOWN_SECONDS      60          /* time P1 has to reach a downed P2                              */
#define P2_REVIVE_HOLD_FRAMES 45         /* frames P1 must HOLD the revive key next to P2 (~1.5 s)        */
#define P2_REVIVE_RADIUS_U   300         /* game distance units (raw >> 12, the unit of the game's Dist()) */
#define P2_REVIVE_KEY1       VK_NUMPAD0  /* P1 revive key (P1 plays on the numpad by default)             */
#define P2_REVIVE_KEY2       VK_INSERT   /* the same key with NumLock off                                 */
#define P2_REVIVE_HP_PCT     30          /* health after the revive                                       */
#define P2_FALL_WAIT_MS      1500        /* empty bar in the air: wait this long at most for the landing  */
#define P2_DEATH_MODE        1           /* countdown expired: 1 = game's own death outcome, 0 = P2 gets up next to P1 */
#define P2_DEATH_DELAY_MS    2500        /* grey marker shown this long before the death outcome          */
#define P2_MARK_UP_U         260         /* marker height above P2's position, game units (~2 m)          */
#define P2_REVIVE_INVULN_FRAMES 120      /* frames without damage after the revive                        */
#define P2_DOWN_TITLE        1           /* show the countdown in the window title bar                    */
#define P2_LEASH_DIST        5242880.0   /* 0x500000  ~12.8 m, used when split-screen is OFF      */
#define P2_LEASH_DIST_SPLIT  16777216.0  /* 0x1000000 ~41 m,   used when split-screen is ON       */

#define PLAYER_SIZE   0xEFCu
#define OFF_POS       0x08u   /* int X, Y, Z (12-bit fixed point, 4096 = 1.0) at +8,+0xC,+0x10 */
#define OFF_NEXT      0x20u
#define OFF_PREV      0x34u
#define OFF_HEALTH    0xE2u   /* short                                           */
#define OFF_INITDONE  0x53Cu  /* byte: first-Update init already done            */
#define OFF_PAD       0xE0Cu  /* pointer to this player's pad block              */

#define P2_OFFSET_X   0x1800  /* where P2 appears relative to P1 (1.5 units)      */

/* v0.8: death / get-up / marker (all found in the disassembly of SpideyPC.exe 1.3) */
#define OFF_VEL        0x60u   /* int X, Y, Z velocity                                            */
#define OFF_GROUNDF    0xE0u   /* byte, bit 1 = touching the ground                                */
#define OFF_ANIM       0x12Au  /* u16 current animation                                            */
#define OFF_GETUPFLAG  0x54Eu  /* byte, 1 = "get up when 0xAF/0xB1 ends" (knock-down state)        */
#define OFF_AIMFLAG    0x8EAu  /* byte, 1 = aiming                                                 */
#define OFF_WEBLINE    0xE6Cu  /* web line object while swinging                                   */
#define OFF_STATE      0xE1Cu  /* state bits (0x80 dead, 0x800000 knocked down, 4 ...)             */
#define STATE_KNOCKDOWN 0x800000u
#define ANIM_DEATH     0xABu   /* the death animation Die() plays on the ground                    */
#define ANIM_GETUP     0xB1u   /* get-up animation after a knock-down                              */
#define SFX_DEATH      0x24
#define A_PLAYER_ANIM  0x004C9130u /* void __thiscall CPlayer::PlayAnim(int anim, int first, int last)  */
#define A_PLAY_SFX     0x00471EA0u /* int __cdecl PlaySfx(int id, int *pos, int flags)                 */
#define A_DIE          0x004BDFF0u /* void __thiscall CPlayer::Die(char outcome): 1 = mission failed    */
#define A_AIM_EXIT     0x004C3810u /* void __thiscall CPlayer::LeaveAim(void)                          */
#define A_WEBLINE_FREE 0x004F6260u /* void __thiscall WebLine::Release(void)                           */
#define A_CAM_MATRIX   0x0056F224u /* short[9] camera rotation (4096 = 1.0), inside the camera block    */
#define A_CAM_POS      0x0056F1B4u /* int[3] camera position in game units (raw >> 12)                  */
#define A_PROJ_H       0x0054F03Cu /* int projection distance (276)                                     */
#define A_PROJ_W       0x0054F040u /* int virtual screen width  (512)                                   */
#define A_PROJ_HGT     0x0054F044u /* int virtual screen height (240)                                   */
#define A_LETTERBOX    0x00660F80u /* int aim/cut-scene bar height, drawn by FUN_004737F0               */
#define A_LETTERBOX2   0x0060F76Cu /* int the HUD's copy of it                                          */

/* v0.8c: cut-scene / scripted-camera handling (see cutscene_report.md) */
#define CUTSCENE_FIX          1
#define OFF_CUTSCENE          0x1ACu  /* byte: CutSceneScript running (set 0x4BC1A0, cleared 0x4BD2CD)              */
#define OFF_IGNOREINPUT       0xE18u  /* int: IgnoreBruceInput countdown (set 0x4C30A0, decremented 0x4C679D)       */
#define CS_SIG_LETTERBOX      1       /* WideScreen bars (0x4E04A3) while P1 is not aiming                          */
#define CS_SIG_IGNOREINPUT    2       /* P1 input disabled by a script for >= CUTSCENE_E18_MIN frames               */
#define CS_SIG_SCRIPT         4       /* P1 runs a CutSceneScript (also short auto-moves without bars)              */
#define CS_SIG_FIXEDCAM       8       /* live camera in a scripted fixed mode 4/5/6 (also gameplay event cams)      */
#define CUTSCENE_SIGNALS      (CS_SIG_LETTERBOX | CS_SIG_IGNOREINPUT)   /* which signals switch the cut-scene mode on */
#define CUTSCENE_E18_MIN      16      /* 0x4BA97A writes 15 for a short non-script stun: ignore that                */
#define CUTSCENE_HOLD_FRAMES  20      /* stay in cut-scene mode this long after the last signal                     */
#define CUTSCENE_TP_DIST_U    1000    /* after a cut-scene P2 is put next to P1 if farther (game units, ~10 m)      */
#define A_CAM_SNAP            0x00416AA0u /* void __thiscall Camera::SnapToFocus(void), acts only in mode 3       */

/* ------------------------------------------------------------------ */
/*  Small helpers                                                      */
/* ------------------------------------------------------------------ */
static HANDLE g_log = INVALID_HANDLE_VALUE;

static void Log(const char *fmt, ...)
{
    char buf[512];
    DWORD n, w;
    va_list ap;
    if (g_log == INVALID_HANDLE_VALUE) return;
    va_start(ap, fmt);
    n = (DWORD)_vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n > sizeof(buf) - 3) n = sizeof(buf) - 3;
    buf[n++] = '\r'; buf[n++] = '\n';
    WriteFile(g_log, buf, n, &w, NULL);
    FlushFileBuffers(g_log);
}

static u32  rd32(u32 a) { return *(volatile u32 *)(uintptr_t)a; }
static u8   rd8 (u32 a) { return *(volatile u8  *)(uintptr_t)a; }
static s16  rd16(u32 a) { return *(volatile s16 *)(uintptr_t)a; }
static void wr32(u32 a, u32 v) { *(volatile u32 *)(uintptr_t)a = v; }
static void wr16(u32 a, s16 v) { *(volatile s16 *)(uintptr_t)a = v; }
static void wr8 (u32 a, u8 v)  { *(volatile u8  *)(uintptr_t)a = v; }

static int PatchBytes(u32 addr, const void *src, size_t n)
{
    DWORD old;
    void *p = (void *)(uintptr_t)addr;
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(p, src, n);
    VirtualProtect(p, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, n);
    return 1;
}

static int SigMatch(u32 addr, const u8 *sig, size_t n)
{
    return memcmp((const void *)(uintptr_t)addr, sig, n) == 0;
}

static int GameHasFocus(void)
{
    DWORD pid = 0;
    HWND h = GetForegroundWindow();
    if (!h) return 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

static int KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

/* edge-triggered hotkey */
static int HotKey(int vk, int *prev)
{
    int now = KeyDown(vk);
    int hit = now && !*prev;
    *prev = now;
    return hit;
}

/* ------------------------------------------------------------------ */
/*  Player 2 state                                                     */
/* ------------------------------------------------------------------ */
static u32 g_p2 = 0;                                  /* CPlayer* of player 2 (0 = none)  */
static COOP_ALIGN16 u8 g_pad2[0x400];        /* Player 2's pad block (stride 0x18C used by game) */
static unsigned g_frame = 0;
static int g_split = SPLIT_DEFAULT;                   /* split-screen requested                       */
static int g_layout = SPLIT_LAYOUT_DEFAULT;
/* v0.10: settings changeable in the in-game co-op menu (F6), saved to coop.ini next to SpideyPC.exe */
static int g_cfgHudGame   = P2_HUD_STYLE;               /* 1 = game-style P2 HUD, 0 = simple            */
static int g_cfgDownSecs  = P2_DOWN_SECONDS;            /* revive countdown                              */
static int g_cfgDeathMode = P2_DEATH_MODE;              /* 1 = mission failed, 0 = P2 respawns next to P1 */
static int g_cfgP2Pad     = 0;                          /* 1 = P2 also reads XInput controller 1   */
static int g_cfgAutoSpawn = 1;                          /* create P2 automatically when a level starts   */
static int g_userRemoved  = 0;                          /* P2 removed by the user: no auto-spawn this level */
static int g_menuOpen     = 0;                          /* in-game co-op menu shown (the world is paused) */
static int g_splitOk = 0;                             /* render hook installed and pool allocated     */
static int g_p2MaxHp = 100;                           /* P2's health as set by the game's constructor */
static int g_p2VHp = 100;                             /* "virtual" health shown on P2's bar           */
static int g_p2Pin = 600;                             /* the real health value P2 is pinned to        */
static int g_p2Down = 0;                              /* 0 alive, 1 lying (countdown), 2 dead (death counted), 3 falling (bar empty, not landed yet) */
#define P2_FROZEN() (g_p2Down == 1 || g_p2Down == 2)  /* Update skipped: lies still in the death pose */
static int g_p2FallMs = 0;                            /* state 3: time spent waiting for the landing   */
static int g_p2DeadMs = 0;                            /* state 2: time left before the death outcome   */
static int g_p2DeathFired = 0;                        /* state 2: outcome already triggered            */
static int g_p2Deaths = 0;                            /* deaths counted this session (P2 HUD)          */
static int g_p1Near = 0;                              /* P1 is within the revive radius of downed P2   */
static int g_reviveKey = 0;                           /* P1 holds the revive key (for the prompt)      */
static int g_p2DownMs = 0;                            /* countdown, milliseconds left                 */
static unsigned g_p2ReviveHold = 0;                   /* frames P1 has stood next to P2               */
static unsigned g_p2Invuln = 0;                       /* frames of protection after a revive          */
static DWORD g_p2LastTick = 0;
static char g_titleSaved[256];
static int g_cutscene = 0;                            /* v0.8c: cut-scene / scripted camera active (incl. hold) */
static int g_csHold = 0;                              /* frames left before the cut-scene mode ends             */
static int g_csSeen = 0;                              /* signals seen during the current cut-scene (for the log) */
static unsigned g_csFrames = 0;
static int g_spawnPending = 0;                        /* F9 pressed during a cut-scene: spawn when it ends      */
static int g_bSnap = 0;                               /* snap camera B to P2 at its next pass-2 render          */
static int g_titleSet = 0;

/* secs < 0 restores the original window title */
static void P2DownTitle(int secs)
{
#if P2_DOWN_TITLE
    static int lastShown = -2;
    HWND w;
    char buf[400];
    if (!GameHasFocus()) return;
    w = GetForegroundWindow();
    if (secs < 0) {
        if (g_titleSet) { SetWindowTextA(w, g_titleSaved); g_titleSet = 0; }
        lastShown = -2;
        return;
    }
    if (secs == lastShown) return;
    lastShown = secs;
    if (!g_titleSet) { GetWindowTextA(w, g_titleSaved, (int)sizeof(g_titleSaved)); g_titleSet = 1; }
    wsprintfA(buf, "%s   |   ! P2 DOWN - revive in %d:%02d", g_titleSaved, secs / 60, secs % 60);
    SetWindowTextA(w, buf);
#else
    (void)secs;
#endif
}
static volatile int g_stage = 0;                      /* where the hook is right now (for crash logs) */
static volatile int g_hideHud = 0;                    /* 1 while P2's pass is rendered: skip the game's HUD */
static int g_hudHooked = 0;                           /* HUD call site redirected                        */
static volatile int g_p2GameHud = 0;                  /* 1 while P2's pass is rendered: DrawHud is run for P2 (style 1) */
static int g_inP2GameHud = 0;                         /* inside that DrawHud call                        */
static int g_p2GameHudRan = 0;                        /* this frame P2 got the game's HUD                */
static int g_hudTimerHooked = 0;                      /* the timer call inside DrawHud is redirected     */
static s32 g_p2HudLb = 0;                             /* HUD offset (DAT_0060F76C) P2's HUD was drawn with */

typedef void  (__cdecl   *PfnVoid)(void);
typedef void *(__cdecl   *PfnAlloc)(int);
typedef void *(__fastcall *PfnCtor)(void *);
typedef void  (__cdecl   *PfnPadEntry)(char *, int);

static PfnVoid g_origDestroyAll = NULL;               /* trampoline to the original DestroyAll */

static int InPlayerList(u32 node)
{
    u32 n = rd32(A_PLAYER_HEAD);
    int guard = 0;
    while (n && guard++ < 16) {
        if (n == node) return 1;
        n = rd32(n + OFF_NEXT);
    }
    return 0;
}

static void UnlinkPlayer(u32 p)
{
    u32 prev = rd32(p + OFF_PREV);
    u32 next = rd32(p + OFF_NEXT);
    if (prev) wr32(prev + OFF_NEXT, next); else if (rd32(A_PLAYER_HEAD) == p) wr32(A_PLAYER_HEAD, next);
    if (next) wr32(next + OFF_PREV, prev);
    wr32(p + OFF_NEXT, 0);
    wr32(p + OFF_PREV, 0);
}

/* The CPlayer constructor rewrites a lot of shared globals (camera settings block
 * and a few flags). We snapshot them and restore after construction so that the
 * existing player/camera are not disturbed. */
typedef struct { u32 addr; u32 size; u8 *copy; } Region;
static Region g_regions[] = {
    { 0x006A7000u, 0x1E00u, NULL },   /* camera/settings globals 0x6A7000-0x6A8E00 */
    { 0x006A9040u, 0x0008u, NULL },   /* DAT_006A9040/41 flags                      */
    { 0x0060F750u, 0x0030u, NULL },   /* DAT_0060F754 / 0060F770                    */
};

static void SnapshotRegions(void)
{
    size_t i;
    for (i = 0; i < sizeof(g_regions) / sizeof(g_regions[0]); i++) {
        if (!g_regions[i].copy) g_regions[i].copy = (u8 *)HeapAlloc(GetProcessHeap(), 0, g_regions[i].size);
        if (g_regions[i].copy) memcpy(g_regions[i].copy, (void *)(uintptr_t)g_regions[i].addr, g_regions[i].size);
    }
}

static void RestoreRegions(void)
{
    size_t i;
    for (i = 0; i < sizeof(g_regions) / sizeof(g_regions[0]); i++)
        if (g_regions[i].copy) memcpy((void *)(uintptr_t)g_regions[i].addr, g_regions[i].copy, g_regions[i].size);
}

/* ------------------------------------------------------------------ */
/*  Leash                                                              */
/* ------------------------------------------------------------------ */
static void TeleportPlayer2(const char *why);

static void LeashPlayer2(void)
{
    static unsigned lastLog = 0;
    u32 p1 = rd32(A_PLAYER_HEAD);
    double dx, dy, dz, lim;
    if (!P2_LEASH_ENABLED) return;
    if (!g_p2 || !p1 || p1 == g_p2) return;
    lim = g_split ? P2_LEASH_DIST_SPLIT : P2_LEASH_DIST;
    dx = (double)(s32)rd32(g_p2 + OFF_POS)     - (double)(s32)rd32(p1 + OFF_POS);
    dy = (double)(s32)rd32(g_p2 + OFF_POS + 4) - (double)(s32)rd32(p1 + OFF_POS + 4);
    dz = (double)(s32)rd32(g_p2 + OFF_POS + 8) - (double)(s32)rd32(p1 + OFF_POS + 8);
    if (dx * dx + dy * dy + dz * dz > lim * lim) {
        if (g_frame - lastLog > 120 || lastLog == 0) { Log("leash: P2 too far from P1, teleporting"); lastLog = g_frame; }
        TeleportPlayer2(NULL);
    }
}

/* ------------------------------------------------------------------ */
/*  SPLIT-SCREEN                                                       */
/*                                                                     */
/*  How the game draws (from the decompilation):                       */
/*   - RenderFrame() (0x4555A0) updates the camera, then every 3D draw */
/*     call builds POLYGON NODES that are transformed to SCREEN        */
/*     coordinates already and inserted into 0x1001 depth buckets      */
/*     (0x6B7AA0). Node = { next, texture, mode|flags<<16, vertexCount,*/
/*     vertices[] } with 28-byte vertices {x,y,z,rhw,color,u,v}.       */
/*   - at the end of the frame the buckets are drawn back to front with*/
/*     DrawPrimitive(TRIANGLEFAN, FVF 0x144) and the buffer is flipped.*/
/*                                                                     */
/*  So: run RenderFrame() for P1 -> scale + clip every node into the   */
/*  P1 rectangle and copy it to our own pool -> empty the buckets ->   */
/*  swap in P2's camera and run RenderFrame() again -> same for the P2 */
/*  rectangle -> put both sets back into the buckets. The game then    */
/*  draws and flips them in its normal end-of-frame code.              */
/* ------------------------------------------------------------------ */
typedef struct { float x, y, z, rhw; u32 col; float u, v; } TLV;     /* 28 bytes */
typedef char TLV_size_check[(sizeof(TLV) == 28) ? 1 : -1];
typedef struct { float x0, y0, x1, y1; } Rect;

#define MAXV        16
#define NODE_BYTES  (16u + 28u * MAXV)
#define POOL_BYTES  (16u * 1024u * 1024u)

typedef struct { u32 head[OT_BUCKETS]; u32 tail[OT_BUCKETS]; } Chain;
static Chain g_c1, g_c2;
static u8   *g_pool = NULL;
static u32   g_poolUsed = 0;

static u32 PoolNode(void)
{
    u32 p;
    if (!g_pool || g_poolUsed + NODE_BYTES > POOL_BYTES) return 0;
    p = (u32)(uintptr_t)(g_pool + g_poolUsed);
    g_poolUsed += NODE_BYTES;
    return p;
}

static void ChainAppend(Chain *c, int h, u32 node)
{
    wr32(node, 0);
    if (c->tail[h]) wr32(c->tail[h], node); else c->head[h] = node;
    c->tail[h] = node;
}

static void ClearOT(void)
{
    int h;
    for (h = 0; h < OT_BUCKETS; h++) wr32(A_OT_HEADS + 4u * (u32)h, 0);
    wr32(A_NODE_COUNT, 0);
}

/* ---- polygon clipping (Sutherland-Hodgman against the view rectangle) ---- */
static TLV LerpV(const TLV *a, const TLV *b, float t)
{
    TLV r;
    float wa = a->rhw, wb = b->rhw, w;
    int i;
    r.x = a->x + (b->x - a->x) * t;
    r.y = a->y + (b->y - a->y) * t;
    r.z = a->z + (b->z - a->z) * t;
    w = wa + (wb - wa) * t;
    r.rhw = w;
    if (wa > 1e-12f && wb > 1e-12f && w > 1e-12f) {      /* perspective-correct texture coordinates */
        r.u = (a->u * wa + (b->u * wb - a->u * wa) * t) / w;
        r.v = (a->v * wa + (b->v * wb - a->v * wa) * t) / w;
    } else {
        r.u = a->u + (b->u - a->u) * t;
        r.v = a->v + (b->v - a->v) * t;
    }
    r.col = 0;
    for (i = 0; i < 32; i += 8) {
        float ca = (float)((a->col >> i) & 0xFF), cb = (float)((b->col >> i) & 0xFF);
        int c = (int)(ca + (cb - ca) * t + 0.5f);
        if (c < 0) c = 0; if (c > 255) c = 255;
        r.col |= (u32)c << i;
    }
    return r;
}

static int ClipEdge(const TLV *in, int n, TLV *out, int axis, int keepGreater, float limit)
{
    int i, m = 0;
    for (i = 0; i < n; i++) {
        const TLV *a = &in[i];
        const TLV *b = &in[(i + 1) % n];
        float da = (axis == 0 ? a->x : a->y) - limit;
        float db = (axis == 0 ? b->x : b->y) - limit;
        int ina, inb;
        if (!keepGreater) { da = -da; db = -db; }
        ina = da >= 0.0f; inb = db >= 0.0f;
        if (ina && m < MAXV) out[m++] = *a;
        if (ina != inb && m < MAXV) {
            TLV r = LerpV(a, b, da / (da - db));
            if (axis == 0) r.x = limit; else r.y = limit;
            out[m++] = r;
        }
    }
    return m;
}

/* scale a polygon into rectangle r and clip it; returns the new vertex count (0 = invisible) */
static int TransformClip(const TLV *src, int n, const Rect *r, float sx, float sy, TLV *out)
{
    TLV a[MAXV], b[MAXV];
    int i, m, inside = 1;
    for (i = 0; i < n; i++) {
        a[i] = src[i];
        if (a[i].x != a[i].x || a[i].y != a[i].y) return 0;            /* NaN */
        a[i].x = r->x0 + a[i].x * sx;
        a[i].y = r->y0 + a[i].y * sy;
        if (!(a[i].x >= r->x0 && a[i].x <= r->x1 && a[i].y >= r->y0 && a[i].y <= r->y1)) inside = 0;
    }
    if (inside) { memcpy(out, a, (size_t)n * sizeof(TLV)); return n; }
    m = ClipEdge(a, n, b, 0, 1, r->x0); if (m < 3) return 0;
    m = ClipEdge(b, m, a, 0, 0, r->x1); if (m < 3) return 0;
    m = ClipEdge(a, m, b, 1, 1, r->y0); if (m < 3) return 0;
    m = ClipEdge(b, m, a, 1, 0, r->y1); if (m < 3) return 0;
    memcpy(out, a, (size_t)m * sizeof(TLV));
    return m;
}

typedef struct { unsigned nodes, kept, dropped; float minx, miny, maxx, maxy; } CollectStat;

/* copy every node of the game's buckets into our pool, scaled+clipped into r */
static void CollectPass(const Rect *r, float W, float H, Chain *c, CollectStat *st)
{
    float sx = (r->x1 - r->x0) / W, sy = (r->y1 - r->y0) / H;
    int h;
    memset(c, 0, sizeof(*c));
    memset(st, 0, sizeof(*st));
    st->minx = st->miny = 1e30f; st->maxx = st->maxy = -1e30f;
    for (h = 0; h < OT_BUCKETS; h++) {
        u32 n = rd32(A_OT_HEADS + 4u * (u32)h);
        while (n) {
            u32 next, cnt;
            if (++st->nodes > 60000u) return;                              /* runaway-list guard */
            if (IsBadReadPtr((const void *)(uintptr_t)n, 16)) { Log("split: bad node pointer %08X in bucket %d", n, h); return; }
            next = rd32(n);
            cnt  = rd32(n + 12);
            if (cnt >= 3 && cnt <= 8 && !IsBadReadPtr((const void *)(uintptr_t)(n + 16), cnt * sizeof(TLV))) {
                TLV in[8], out[MAXV];
                u32 i, p;
                int m;
                memcpy(in, (const void *)(uintptr_t)(n + 16), cnt * sizeof(TLV));
                for (i = 0; i < cnt; i++) {
                    if (in[i].x < st->minx) st->minx = in[i].x;
                    if (in[i].y < st->miny) st->miny = in[i].y;
                    if (in[i].x > st->maxx) st->maxx = in[i].x;
                    if (in[i].y > st->maxy) st->maxy = in[i].y;
                }
                m = TransformClip(in, (int)cnt, r, sx, sy, out);
                p = (m >= 3) ? PoolNode() : 0;
                if (p) {
                    wr32(p + 4, rd32(n + 4));
                    wr32(p + 8, rd32(n + 8));
                    wr32(p + 12, (u32)m);
                    memcpy((void *)(uintptr_t)(p + 16), out, (size_t)m * sizeof(TLV));
                    ChainAppend(c, h, p);
                    st->kept++;
                } else st->dropped++;
            } else st->dropped++;
            n = next;
        }
    }
}

/* ---- tiny untextured rectangles (P2 health bar, divider) ---- */
static void AddQuad(Chain *c, int bucket, float x0, float y0, float x1, float y1, u32 argb)
{
    u32 p = PoolNode();
    TLV *v;
    int i;
    if (!p) return;
    wr32(p + 4, 0);            /* no texture */
    wr32(p + 8, 0);            /* mode 0 (opaque), default flags */
    wr32(p + 12, 4);
    v = (TLV *)(uintptr_t)(p + 16);
    v[0].x = x0; v[0].y = y0;
    v[1].x = x1; v[1].y = y0;
    v[2].x = x1; v[2].y = y1;
    v[3].x = x0; v[3].y = y1;
    for (i = 0; i < 4; i++) { v[i].z = 0.0f; v[i].rhw = 1.0f; v[i].col = argb; v[i].u = 0.0f; v[i].v = 0.0f; }
    ChainAppend(c, bucket, p);
}

/* ---- Player 2 HUD, drawn with plain coloured quads (no textures needed) ---- */
static void Box(Chain *c, float x0, float y0, float x1, float y1, u32 argb)
{
    AddQuad(c, 0, x0, y0, x1, y1, argb);
}

/* 7-segment glyphs. bits: 1=a(top) 2=b(upper right) 4=c(lower right) 8=d(bottom)
 *                         0x10=e(lower left) 0x20=f(upper left) 0x40=g(middle)  */
static const u8 kSeg7[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
#define SEG_P 0x73u

static void Glyph(Chain *c, float x, float y, float w, float h, unsigned segs, u32 col)
{
    float t = w * 0.30f, m = y + h * 0.5f;
    if (t < 2.0f) t = 2.0f;
    if (segs & 0x01) Box(c, x + t,     y,         x + w - t, y + t,              col);
    if (segs & 0x02) Box(c, x + w - t, y,         x + w,     m,                  col);
    if (segs & 0x04) Box(c, x + w - t, m,         x + w,     y + h,              col);
    if (segs & 0x08) Box(c, x + t,     y + h - t, x + w - t, y + h,              col);
    if (segs & 0x10) Box(c, x,         m,         x + t,     y + h,              col);
    if (segs & 0x20) Box(c, x,         y,         x + t,     m,                  col);
    if (segs & 0x40) Box(c, x + t,     m - t * 0.5f, x + w - t, m + t * 0.5f,    col);
}

static void Number2(Chain *c, float x, float y, float w, float h, int v, u32 col)
{
    if (v < 0)  v = 0;
    if (v > 99) v = 99;
    Glyph(c, x,                  y, w, h, kSeg7[v / 10], col);
    Glyph(c, x + w + w * 0.45f,  y, w, h, kSeg7[v % 10], col);
}

static void DrawP2Hud(Chain *c, const Rect *r)
{
    float vw = r->x1 - r->x0, vh = r->y1 - r->y0;
    float s = vw / 320.0f, sy = vh / 240.0f;
    float x, y, hx, tagw, bh, bw, gap, wy, wh, cy, px;
    float hpFrac, webFrac;
    int carts, fluid, i, full;
    u32 hpCol;

    if (!g_p2) return;
    if (sy < s) s = sy;
    if (s < 0.6f) s = 0.6f;
    x = r->x0 + 8.0f * s;  y = r->y0 + 8.0f * s;
    tagw = 24.0f * s;  bh = 12.0f * s;  bw = 110.0f * s;  gap = 4.0f * s;
    hx = x + tagw + gap;

    hpFrac = (g_p2MaxHp > 0) ? (float)g_p2VHp / (float)g_p2MaxHp : 1.0f;
    if (hpFrac < 0.0f) hpFrac = 0.0f;
    if (hpFrac > 1.0f) hpFrac = 1.0f;
    hpCol = (hpFrac > 0.5f) ? 0xFF30D030u : (hpFrac > 0.25f) ? 0xFFE0C020u : 0xFFE03020u;

    fluid = (s32)rd32(g_p2 + OFF_WEB_FLUID);
    carts = (s32)rd32(g_p2 + OFF_WEB_CARTS);
    if (fluid < 0)      fluid = 0;
    if (fluid > 0x1000) fluid = 0x1000;
    if (carts < 0)      carts = 0;
    if (carts > 99)     carts = 99;
    webFrac = (float)fluid / 4096.0f;

    /* row 1: "P2" tag + health bar */
    Box(c, x, y, x + tagw, y + bh, 0xFF2060E0u);
    Glyph(c, x + 3.0f * s,  y + 2.0f * s, 6.0f * s, bh - 4.0f * s, SEG_P,   0xFFFFFFFFu);
    Glyph(c, x + 12.0f * s, y + 2.0f * s, 6.0f * s, bh - 4.0f * s, kSeg7[2], 0xFFFFFFFFu);
    Box(c, hx - 2.0f * s, y - 2.0f * s, hx + bw + 2.0f * s, y + bh + 2.0f * s, 0xFF000000u);     /* frame      */
    Box(c, hx, y, hx + bw, y + bh, 0xFF501010u);                                                /* background */
    if (hpFrac > 0.0f) Box(c, hx, y, hx + bw * hpFrac, y + bh, hpCol);                          /* fill       */

    /* row 2: web-fluid gauge */
    wy = y + bh + 6.0f * s;  wh = 9.0f * s;
    Box(c, x, wy, x + tagw, wy + wh, 0xFF1A3C8Cu);                                              /* tag        */
    Box(c, x + 9.0f * s, wy + 2.0f * s, x + 15.0f * s, wy + wh - 2.0f * s, 0xFFB8D8FFu);        /* drop icon  */
    Box(c, hx - 2.0f * s, wy - 2.0f * s, hx + bw + 2.0f * s, wy + wh + 2.0f * s, 0xFF000000u);
    Box(c, hx, wy, hx + bw, wy + wh, 0xFF101830u);
    if (webFrac > 0.0f) Box(c, hx, wy, hx + bw * webFrac, wy + wh, (webFrac > 0.15f) ? 0xFF40B0FFu : 0xFFE05030u);
    for (i = 1; i < 4; i++)                                                                     /* quarter ticks */
        Box(c, hx + bw * 0.25f * (float)i - 0.75f * s, wy, hx + bw * 0.25f * (float)i + 0.75f * s, wy + wh, 0xFF101830u);

    /* row 3: cartridge icon, two-digit count, 10 pips */
    cy = wy + wh + 6.0f * s;
    Box(c, x + 5.0f * s, cy, x + 15.0f * s, cy + 14.0f * s, 0xFF2A5CD0u);                       /* body       */
    Box(c, x + 5.0f * s, cy, x + 15.0f * s, cy + 3.0f * s,  0xFFB8D0FFu);                       /* cap        */
    Box(c, x + 7.0f * s, cy + 7.0f * s, x + 13.0f * s, cy + 9.0f * s, 0xFFFFFFFFu);             /* label      */
    Number2(c, hx, cy, 8.0f * s, 14.0f * s, carts, 0xFFFFFFFFu);
    px = hx + 8.0f * s * 2.45f + 8.0f * s;
    full = (carts > 10) ? 10 : carts;
    for (i = 0; i < 10; i++) {
        float x0 = px + (float)i * 8.5f * s;
        Box(c, x0, cy + 3.0f * s, x0 + 6.0f * s, cy + 11.0f * s, (i < full) ? 0xFF40B0FFu : 0xFF182848u);
    }

    /* row 4 (v0.8): deaths counted (countdown expired) - red cross icon + count */
    if (g_p2Deaths > 0) {
        float dy = cy + 14.0f * s + 6.0f * s;
        Box(c, x + 4.0f * s, dy + 5.0f * s, x + 16.0f * s, dy + 9.0f * s, 0xFFE03030u);
        Box(c, x + 8.0f * s, dy + 1.0f * s, x + 12.0f * s, dy + 13.0f * s, 0xFFE03030u);
        Number2(c, hx, dy, 8.0f * s, 14.0f * s, g_p2Deaths, 0xFFFF8080u);
    }
}

/* ---- style 1: the game's own HUD drawn for P2 ---- */
/* globals DrawHud (and its callees) CHANGE on every call. Saved before P2's call and restored after it, so the
 * second call per frame has no lasting effect:
 *   0x60F774 timer value, 0x60F778 its frame stamp (DrawHud), 0x60F780 last shown second (tick sound),
 *   0x60F784/85 timer flash flags + their positions 0x60F66C / 0x60F74C (FUN_00461D00),
 *   0x60F768 compass flash counter (FUN_00463860), 0x6B4C7C/0x6B4B90/0x6B4B94 the game's RNG (FUN_004E5DA0). */
static const u32 kHudVolatile[] = { 0x0060F774u, 0x0060F778u, 0x0060F780u, 0x0060F784u, 0x0060F66Cu, 0x0060F74Cu,
                                    0x0060F768u, 0x006B4C7Cu, 0x006B4B90u, 0x006B4B94u };
#define HUD_VOLATILE_N (sizeof(kHudVolatile) / sizeof(kHudVolatile[0]))

static void GameHudForP2(void)
{
    u32 p = g_p2, saved[HUD_VOLATILE_N], face, boss;
    s16 pinned;
    s32 mx;
    int v;
    size_t i;
    u8 compass;

    if (!p || rd32(A_PLAYER_HEAD) != p) return;       /* not P2's list order: draw nothing (the fallback HUD is drawn) */
    mx = (s32)rd32(p + OFF_MAXHP);                     /* the max DrawHud divides by */
    if (mx < 1 || mx > 30000) mx = g_p2MaxHp;
    v = (g_p2MaxHp > 0) ? (int)((double)g_p2VHp * (double)mx / (double)g_p2MaxHp + 0.5) : mx;
    if (g_p2Down) v = 0;
    if (v < 0) v = 0;
    if (v > mx) v = mx;

    pinned = rd16(p + OFF_HEALTH);
    for (i = 0; i < HUD_VOLATILE_N; i++) saved[i] = rd32(kHudVolatile[i]);
    face = rd32(A_HUD_ARMOR_FACE); boss = rd32(A_HUD_BOSS); compass = rd8(A_HUD_COMPASS_ON);
    if (!rd8(p + OFF_ARMOR_ON)) wr32(A_HUD_ARMOR_FACE, 0);   /* the armour face belongs to P1's armour */
#if !P2_HUD_BOSSBAR
    wr32(A_HUD_BOSS, 0);
#endif
#if !P2_HUD_COMPASS
    wr8(A_HUD_COMPASS_ON, 0);
#endif
    wr16(p + OFF_HEALTH, (s16)v);                      /* virtual health, only for this call */

    g_inP2GameHud = 1;
    ((PfnVoid)(uintptr_t)A_HUD_DRAW)();
    g_inP2GameHud = 0;

    wr16(p + OFF_HEALTH, pinned);                      /* exact old value: Hook_UpdateWorld sees no "damage" */
    wr8(A_HUD_COMPASS_ON, compass); wr32(A_HUD_BOSS, boss); wr32(A_HUD_ARMOR_FACE, face);
    for (i = 0; i < HUD_VOLATILE_N; i++) wr32(kHudVolatile[i], saved[i]);
    g_p2GameHudRan = 1;
}

#if P2_CUSTOM_HUD && P2_HUD_STYLE == 1
/* DrawHud's first callee (0x461D00): mission timer, and on 4 levels a level-script HUD callback (FUN_0047D470).
 * In P2's call the script callbacks are skipped (unknown side effects); the timer is drawn if P2_HUD_TIMER. */
static void __cdecl Hook_HudTimer(void)
{
    if (g_inP2GameHud) {
#if P2_HUD_TIMER
        u32 lvl = ((u32 (__cdecl *)(void))(uintptr_t)A_LEVEL_ID)();
        if (lvl == 0x201u || lvl == 0x501u || lvl == 0x505u || lvl == 0x806u) return;
        ((PfnVoid)(uintptr_t)A_HUD_TIMER)();
#endif
        return;
    }
    ((PfnVoid)(uintptr_t)A_HUD_TIMER)();
}
#endif

/* the game's HUD (health, web, cartridges, compass) reads the player-list head. In P2's pass:
 * style 1 runs it for P2 with his virtual health (GameHudForP2), style 0 skips it and P2 gets DrawP2Hud() */
static void __cdecl Hook_Hud(void)
{
    if (g_p2GameHud) { GameHudForP2(); return; }
    if (g_hideHud) return;
    ((PfnVoid)(uintptr_t)A_HUD_DRAW)();
}

static void MakeViews(float W, float H, Rect *a, Rect *b)
{
    switch (g_layout) {
    case 1:  a->x0 = 0;         a->y0 = 0;         a->x1 = W * 0.5f;  a->y1 = H;
             b->x0 = W * 0.5f;  b->y0 = 0;         b->x1 = W;         b->y1 = H;          break;
    case 2:  a->x0 = 0;         a->y0 = H * 0.25f; a->x1 = W * 0.5f;  a->y1 = H * 0.75f;
             b->x0 = W * 0.5f;  b->y0 = H * 0.25f; b->x1 = W;         b->y1 = H * 0.75f;  break;
    case 3:  a->x0 = W * 0.25f; a->y0 = 0;         a->x1 = W * 0.75f; a->y1 = H * 0.5f;
             b->x0 = W * 0.25f; b->y0 = H * 0.5f;  b->x1 = W * 0.75f; b->y1 = H;          break;
    default: a->x0 = 0;         a->y0 = 0;         a->x1 = W;         a->y1 = H * 0.5f;
             b->x0 = 0;         b->y0 = H * 0.5f;  b->x1 = W;         b->y1 = H;          break;
    }
}

static void DrawDivider(Chain *c, float W, float H)
{
    if (g_layout == 1 || g_layout == 2) AddQuad(c, 0, W * 0.5f - 1.5f, 0.0f, W * 0.5f + 1.5f, H, 0xFF202020u);
    else                                AddQuad(c, 0, 0.0f, H * 0.5f - 1.5f, W, H * 0.5f + 1.5f, 0xFF202020u);
}

/* ------------------------------------------------------------------ */
/*  v0.8: "!" marker + countdown + revive prompt over a downed P2       */
/* ------------------------------------------------------------------ */
#define A_PSX_W 0x0061B5FCu      /* virtual screen width the renderer scales from (FUN_004737F0 uses it)  */
#define A_PSX_H 0x00628614u      /* virtual screen height                                                 */

typedef struct { int valid; float x, y; } MarkPos;   /* valid: 0 no, 1 projected, 2 behind the camera; x,y in screen pixels (W x H) */
static MarkPos g_markA, g_markB;

/* any convex polygon (3 or 4 corners) in bucket 0 */
static void AddPoly(Chain *c, int n, const float *xy, u32 argb)
{
    u32 p = PoolNode();
    TLV *v;
    int i;
    if (!p) return;
    wr32(p + 4, 0);
    wr32(p + 8, 0);
    wr32(p + 12, (u32)n);
    v = (TLV *)(uintptr_t)(p + 16);
    for (i = 0; i < n; i++) {
        v[i].x = xy[2 * i]; v[i].y = xy[2 * i + 1];
        v[i].z = 0.0f; v[i].rhw = 1.0f; v[i].col = argb; v[i].u = 0.0f; v[i].v = 0.0f;
    }
    ChainAppend(c, 0, p);
}

static int SaneInt(u32 addr, int lo, int hi, int def)
{
    s32 v = (s32)rd32(addr);
    return (v >= lo && v <= hi) ? (int)v : def;
}

/* Project P2's position (P2_MARK_UP_U above it; the world's up is -Y) with the CURRENT camera globals, the same
 * way DrawReticle (FUN_004C4940) does it: v = (pos >> 12) - campos; RotTransPers (FUN_0046DBC0) with the camera
 * matrix; sx = H*x/z + W/2, sy = H*y/z + Hgt/2 in the 512x240 virtual screen; then to pixels like FUN_004737F0. */
static MarkPos ProjectP2(float W, float H)
{
    MarkPos r;
    const s16 *m = (const s16 *)(uintptr_t)A_CAM_MATRIX;
    double vx, vy, vz, x, y, z, ph, pw, phg, vw, vh;
    r.valid = 0; r.x = r.y = 0.0f;
    if (!g_p2) return r;
    if (m[0] == 0 && m[1] == 0 && m[2] == 0 && m[4] == 0 && m[8] == 0) return r;
    vx = (double)((s32)rd32(g_p2 + OFF_POS)     >> 12) - (double)(s32)rd32(A_CAM_POS);
    vy = (double)((s32)rd32(g_p2 + OFF_POS + 4) >> 12) - (double)P2_MARK_UP_U - (double)(s32)rd32(A_CAM_POS + 4);
    vz = (double)((s32)rd32(g_p2 + OFF_POS + 8) >> 12) - (double)(s32)rd32(A_CAM_POS + 8);
    x = ((double)m[0] * vx + (double)m[1] * vy + (double)m[2] * vz) / 4096.0;
    y = ((double)m[3] * vx + (double)m[4] * vy + (double)m[5] * vz) / 4096.0;
    z = ((double)m[6] * vx + (double)m[7] * vy + (double)m[8] * vz) / 4096.0;
    ph  = SaneInt(A_PROJ_H,   50, 5000, 276);
    pw  = SaneInt(A_PROJ_W,   64, 4096, 512);
    phg = SaneInt(A_PROJ_HGT, 64, 4096, 240);
    vw  = SaneInt(A_PSX_W,    64, 4096, (int)pw);
    vh  = SaneInt(A_PSX_H,    64, 4096, (int)phg);
    if (z < 16.0) {                       /* behind the camera: bottom edge, on P2's side */
        r.x = (x >= 0.0) ? W : 0.0f; r.y = H; r.valid = 2;
        return r;
    }
    r.x = (float)((ph * x / z + pw  * 0.5) * (double)W / vw);
    r.y = (float)((ph * y / z + phg * 0.5) * (double)H / vh);
    if (r.x != r.x || r.y != r.y) return r;
    r.valid = 1;
    return r;
}

static u32 LerpCol(u32 a, u32 b, float t)
{
    u32 r = 0; int i;
    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
    for (i = 0; i < 32; i += 8) {
        float ca = (float)((a >> i) & 0xFF), cb = (float)((b >> i) & 0xFF);
        r |= (u32)(int)(ca + (cb - ca) * t + 0.5f) << i;
    }
    return r;
}

/* the marker in one view. isP1View: draw the key prompt (P1 is the one who revives). */
static void DrawDownMarker(Chain *c, const Rect *r, MarkPos mp, float W, float H, int isP1View)
{
    float rw = r->x1 - r->x0, rh = r->y1 - r->y0, s, mx, my, bw, bh, pulse, dw, dh, tw, tx, ty, xx;
    float tri[6];
    DWORD now = GetTickCount();
    int secs, dead;
    u32 badge, digit;

    if (!g_p2 || !P2_FROZEN() || !mp.valid || W < 1.0f || H < 1.0f) return;
    dead = (g_p2Down == 2);
    s = rh / 240.0f; if (rw / 320.0f < s) s = rw / 320.0f; if (s < 0.6f) s = 0.6f;
    bw = 18.0f * s; bh = 26.0f * s;
    mx = r->x0 + mp.x * (rw / W);
    my = r->y0 + mp.y * (rh / H);                       /* = bottom tip of the pointer */
    /* keep it inside the view: off-screen it sits at the edge and works as a pointer */
    if (mx < r->x0 + bw)           mx = r->x0 + bw;
    if (mx > r->x1 - bw)           mx = r->x1 - bw;
    if (my < r->y0 + bh + 10.0f * s) my = r->y0 + bh + 10.0f * s;
    if (my > r->y1 - 52.0f * s)    my = r->y1 - 52.0f * s;
    my -= 6.0f * s;                                      /* badge bottom (pointer tip is 6 px lower) */

    secs = (g_p2DownMs + 999) / 1000; if (secs < 0) secs = 0;
    if (dead) { badge = 0xFF6A6A6Au; digit = 0xFFA0A0A0u; }
    else if (secs <= 10) {                               /* last 10 s: fast hard blink */
        pulse = ((now / 180u) & 1u) ? 1.0f : 0.0f;
        badge = LerpCol(0xFF700A0Au, 0xFFFF2020u, pulse); digit = 0xFFFF6060u;
    } else {
        pulse = 0.5f + 0.5f * (float)sin((double)now * 0.0105);
        badge = LerpCol(0xFFB01414u, 0xFFFF6A2Au, pulse); digit = 0xFFFFFFFFu;
    }

    /* pointer + badge with a black outline */
    tri[0] = mx - 7.0f * s; tri[1] = my;  tri[2] = mx + 7.0f * s; tri[3] = my;  tri[4] = mx; tri[5] = my + 8.0f * s;
    AddPoly(c, 3, tri, 0xFF000000u);
    Box(c, mx - bw * 0.5f - 2.0f * s, my - bh - 2.0f * s, mx + bw * 0.5f + 2.0f * s, my + 2.0f * s, 0xFF000000u);
    Box(c, mx - bw * 0.5f, my - bh, mx + bw * 0.5f, my, badge);
    tri[0] = mx - 5.0f * s; tri[1] = my;  tri[2] = mx + 5.0f * s; tri[3] = my;  tri[4] = mx; tri[5] = my + 5.5f * s;
    AddPoly(c, 3, tri, badge);
    /* "!" */
    Box(c, mx - 2.2f * s, my - bh + 4.0f * s, mx + 2.2f * s, my - 10.0f * s, 0xFFFFFFFFu);
    Box(c, mx - 2.2f * s, my - 7.5f * s,      mx + 2.2f * s, my - 3.5f * s,  0xFFFFFFFFu);

    /* countdown m:ss under the pointer */
    dw = 6.5f * s; dh = 11.0f * s;
    tw = dw + 4.0f * s + dw * 2.45f;
    tx = mx - tw * 0.5f; ty = my + 11.0f * s;
    Box(c, tx - 3.0f * s, ty - 2.0f * s, tx + tw + 3.0f * s, ty + dh + 2.0f * s, 0xFF101010u);
    Glyph(c, tx, ty, dw, dh, kSeg7[(secs / 60) % 10], digit);
    xx = tx + dw + 1.5f * s;
    Box(c, xx, ty + dh * 0.25f, xx + 1.6f * s, ty + dh * 0.25f + 1.6f * s, digit);
    Box(c, xx, ty + dh * 0.70f, xx + 1.6f * s, ty + dh * 0.70f + 1.6f * s, digit);
    Number2(c, xx + 2.5f * s, ty, dw, dh, secs % 60, digit);

    /* revive prompt: animated key cap "0" (Num0) + progress bar */
    if (!dead && g_p1Near) {
        float py = ty + dh + 6.0f * s, k = 15.0f * s, off = 0.0f, fr;
        if (isP1View) {
            if (g_reviveKey) off = 2.0f * s;                                           /* pressed */
            else k *= 1.0f + 0.12f * (0.5f + 0.5f * (float)sin((double)now * 0.016));  /* "press me" pulse */
            Box(c, mx - k * 0.5f, py + 2.0f * s, mx + k * 0.5f, py + k + 2.0f * s, 0xFF000000u);          /* key side */
            Box(c, mx - k * 0.5f, py + off, mx + k * 0.5f, py + k + off - 1.0f * s,
                g_reviveKey ? 0xFFB8C8B8u : 0xFFF2F2F2u);                                                  /* key top  */
            Glyph(c, mx - 3.5f * s, py + off + 3.0f * s, 7.0f * s, k - 7.0f * s, kSeg7[0], 0xFF202020u);
            py += k + 5.0f * s;
        }
        fr = (float)g_p2ReviveHold / (float)P2_REVIVE_HOLD_FRAMES;
        if (fr > 1.0f) fr = 1.0f;
        Box(c, mx - 20.0f * s, py, mx + 20.0f * s, py + 4.0f * s, 0xFF000000u);
        if (fr > 0.0f) Box(c, mx - 20.0f * s, py, mx - 20.0f * s + 40.0f * s * fr, py + 4.0f * s, 0xFF40E060u);
    }
}

/* split-screen OFF: draw the marker into the game's own buckets (on top of everything) */
static int ScreenSize(float *W, float *H);
static void DrawMarkerSingleView(void)
{
    static Chain c3;
    float W, H;
    Rect r;
    u32 n;
    int guard = 0;
    if (!g_p2 || !P2_FROZEN() || !g_pool) return;
    if (rd32(A_GAME_STATE) != 0 || rd32(A_SW_RENDER) != 0 || !rd32(A_D3D_DEVICE) || !ScreenSize(&W, &H)) return;
    memset(&c3, 0, sizeof(c3));
    r.x0 = 0.0f; r.y0 = 0.0f; r.x1 = W; r.y1 = H;
    DrawDownMarker(&c3, &r, ProjectP2(W, H), W, H, 1);
    if (!c3.head[0]) return;
    n = rd32(A_OT_HEADS);
    if (!n) { wr32(A_OT_HEADS, c3.head[0]); return; }
    while (rd32(n) && guard++ < 60000) n = rd32(n);
    wr32(n, c3.head[0]);
}

/* ------------------------------------------------------------------ */
/*  v0.10: in-game co-op menu (F6) + coop.ini                          */
/*  Drawn with a built-in 5x7 font (one quad per run of lit pixels).   */
/*  While it is open the world is paused (UpdateWorld is not called).  */
/* ------------------------------------------------------------------ */
static const char kFontChars[] = " !%()+-./0123456789:<>ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const u8 kFont5x7[48][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    {0x00,0x00,0x00,0x0E,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, {0x00,0x04,0x04,0x00,0x04,0x04,0x00}, {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};

#define MENU_KEY      VK_F6
#define MENU_ITEMS    14
static char g_iniPath[MAX_PATH];
static int  g_menuSel = 0;
static const char *kLayoutNames[4] = { "TOP / BOTTOM", "LEFT / RIGHT", "LEFT / RIGHT WIDE", "TOP / BOTTOM WIDE" };

static void SpawnPlayer2(void);
static void RemovePlayer2(void);
static int g_stBValid;                                /* defined with the camera state below */

/* v0.12 LAN co-op settings (coop.ini [lan]); the code is in the "LAN CO-OP" section near the end */
#define LAN_PORT_DEF 27016
static int  g_lanMode = 0;                            /* 0 off, 1 host, 2 join */
static char g_lanIp[32] = "26.0.0.1";                 /* join: host address (LAN 192.168.x.x or Radmin VPN 26.x.x.x) */
static int  g_lanPort = LAN_PORT_DEF;
static int  g_lanSyncWorld = 1;                       /* enemies killed + trigger zones */
static int  g_lanSyncLevel = 1;                       /* level end / restart / mission failed for both */
static char g_nick[16] = "PLAYER";
static volatile int g_lanActive = 0;                  /* socket open */
static int  g_ipEdit = 0;                             /* HOST IP line: typing mode */
static void Lan_Start(void);
static void Lan_Stop(void);
static void Lan_StatusText(char *out);
static void Lan_TravelToPartner(void);
static void Lan_PreWorld(void);
static void Lan_PostWorld(int menuOpen);
static void Lan_OnDestroyAll(void);

static void CfgLoad(void)
{
    if (!g_iniPath[0]) return;
    g_split        = GetPrivateProfileIntA("coop", "split",        g_split,        g_iniPath) ? 1 : 0;
    g_layout       = GetPrivateProfileIntA("coop", "layout",       g_layout,       g_iniPath) & 3;
    g_cfgHudGame   = GetPrivateProfileIntA("coop", "p2_hud_game",  g_cfgHudGame,   g_iniPath) ? 1 : 0;
    g_cfgDownSecs  = GetPrivateProfileIntA("coop", "revive_secs",  g_cfgDownSecs,  g_iniPath);
    g_cfgDeathMode = GetPrivateProfileIntA("coop", "death_mode",   g_cfgDeathMode, g_iniPath) ? 1 : 0;
    g_cfgAutoSpawn = GetPrivateProfileIntA("coop", "auto_spawn",   g_cfgAutoSpawn, g_iniPath) ? 1 : 0;
    g_cfgP2Pad     = GetPrivateProfileIntA("coop", "p2_gamepad",   g_cfgP2Pad,     g_iniPath) ? 1 : 0;
    if (g_cfgDownSecs < 10 || g_cfgDownSecs > 600) g_cfgDownSecs = P2_DOWN_SECONDS;
    g_lanMode      = GetPrivateProfileIntA("lan", "mode",        g_lanMode,      g_iniPath) % 3;
    g_lanPort      = GetPrivateProfileIntA("lan", "port",        g_lanPort,      g_iniPath);
    g_lanSyncWorld = GetPrivateProfileIntA("lan", "sync_world",  g_lanSyncWorld, g_iniPath) ? 1 : 0;
    g_lanSyncLevel = GetPrivateProfileIntA("lan", "sync_level",  g_lanSyncLevel, g_iniPath) ? 1 : 0;
    {   char def[32];
        strcpy(def, g_lanIp); GetPrivateProfileStringA("lan", "host_ip", def, g_lanIp, sizeof(g_lanIp), g_iniPath);
        strcpy(def, g_nick);  GetPrivateProfileStringA("lan", "nick",    def, g_nick,  sizeof(g_nick),  g_iniPath); }
    if (g_lanPort < 1024 || g_lanPort > 65535) g_lanPort = LAN_PORT_DEF;
    {   /* the menu font has capitals, digits and a few signs only */
        char *c;
        for (c = g_nick; *c; c++) { if (*c >= 'a' && *c <= 'z') *c -= 32; if (!strchr(kFontChars, *c)) *c = '-'; }
        if (!g_nick[0]) strcpy(g_nick, "PLAYER");
    }
}

static void CfgSaveInt(const char *key, int v)
{
    char b[16];
    if (!g_iniPath[0]) return;
    wsprintfA(b, "%d", v);
    WritePrivateProfileStringA("coop", key, b, g_iniPath);
}

static void CfgSave(void)
{
    CfgSaveInt("split", g_split);           CfgSaveInt("layout", g_layout);
    CfgSaveInt("p2_hud_game", g_cfgHudGame); CfgSaveInt("revive_secs", g_cfgDownSecs);
    CfgSaveInt("death_mode", g_cfgDeathMode); CfgSaveInt("auto_spawn", g_cfgAutoSpawn);
    CfgSaveInt("p2_gamepad", g_cfgP2Pad);
    if (g_iniPath[0]) {
        char b[16];
        wsprintfA(b, "%d", g_lanMode);      WritePrivateProfileStringA("lan", "mode", b, g_iniPath);
        wsprintfA(b, "%d", g_lanPort);      WritePrivateProfileStringA("lan", "port", b, g_iniPath);
        wsprintfA(b, "%d", g_lanSyncWorld); WritePrivateProfileStringA("lan", "sync_world", b, g_iniPath);
        wsprintfA(b, "%d", g_lanSyncLevel); WritePrivateProfileStringA("lan", "sync_level", b, g_iniPath);
        WritePrivateProfileStringA("lan", "host_ip", g_lanIp, g_iniPath);
        WritePrivateProfileStringA("lan", "nick", g_nick, g_iniPath);
    }
}

/* one line of text; px = size of one font pixel */
static void DrawText5x7(Chain *c, float x, float y, float px, const char *str, u32 col)
{
    for (; *str; str++, x += 6.0f * px) {
        const char *f = strchr(kFontChars, (*str >= 'a' && *str <= 'z') ? *str - 32 : *str);
        int r;
        if (!f || *str == ' ') continue;
        for (r = 0; r < 7; r++) {
            u8 bits = kFont5x7[f - kFontChars][r];
            int b = 4;
            while (b >= 0) {
                int s0;
                if (!(bits & (1 << b))) { b--; continue; }
                s0 = b;
                while (b >= 0 && (bits & (1 << b))) b--;
                Box(c, x + (float)(4 - s0) * px, y + (float)r * px, x + (float)(4 - b) * px, y + (float)(r + 1) * px, col);
            }
        }
    }
}

static float TextW(const char *s, float px) { return (float)strlen(s) * 6.0f * px - px; }

static void MenuItemText(int i, char *label, char *value)
{
    label[0] = value[0] = 0;
    switch (i) {
    case 0: strcpy(label, "PLAYER 2");       strcpy(value, g_lanMode ? "(OFF IN LAN MODE)" : (g_p2 && InPlayerList(g_p2)) ? "< REMOVE >" : "< SPAWN >"); break;
    case 1: strcpy(label, "SPLIT SCREEN");   strcpy(value, g_split ? "< ON >" : "< OFF >"); break;
    case 2: strcpy(label, "LAYOUT");         wsprintfA(value, "< %s >", kLayoutNames[g_layout & 3]); break;
    case 3: strcpy(label, "P2 HUD");         strcpy(value, g_cfgHudGame ? "< GAME STYLE >" : "< SIMPLE >"); break;
    case 4: strcpy(label, "REVIVE TIME");    wsprintfA(value, "< %d S >", g_cfgDownSecs); break;
    case 5: strcpy(label, "P2 BLEEDS OUT");  strcpy(value, g_cfgDeathMode ? "< MISSION FAILED >" : "< RESPAWN AT P1 >"); break;
    case 6: strcpy(label, "AUTO SPAWN P2");  strcpy(value, g_cfgAutoSpawn ? "< ON >" : "< OFF >"); break;
    case 7: strcpy(label, "P2 GAMEPAD");     strcpy(value, g_cfgP2Pad ? "< XINPUT PAD 1 >" : "< OFF (KEYBOARD) >"); break;
    case 8: strcpy(label, "LAN CO-OP");      strcpy(value, g_lanMode == 1 ? "< HOST >" : g_lanMode == 2 ? "< JOIN >" : "< OFF >"); break;
    case 9: strcpy(label, "HOST IP");
            if (g_ipEdit) wsprintfA(value, "%s%s", g_lanIp, (GetTickCount() / 300) & 1 ? "-" : " ");
            else wsprintfA(value, "%s  (ENTER: EDIT)", g_lanIp);
            break;
    case 10: strcpy(label, "PORT");          wsprintfA(value, "< %d >", g_lanPort); break;
    case 11: strcpy(label, "SYNC ENEMIES");  strcpy(value, g_lanSyncWorld ? "< ON >" : "< OFF >"); break;
    case 12: strcpy(label, "GO TO PARTNER"); strcpy(value, g_lanActive ? "(ENTER: LOAD HIS LEVEL)" : "-"); break;
    default: strcpy(label, "CLOSE MENU");    break;
    }
}

/* dir: -1 left, +1 right / enter */
static void MenuChange(int i, int dir)
{
    switch (i) {
    case 0:
        if (g_lanMode) break;
        if (g_p2 && InPlayerList(g_p2)) RemovePlayer2();
        else { g_userRemoved = 0; if (g_cutscene) { g_spawnPending = 1; Log("menu: P2 will be spawned after the cut-scene"); } else SpawnPlayer2(); }
        break;
    case 1: g_split = !g_split; g_stBValid = 0; break;
    case 2: g_layout = (g_layout + (dir < 0 ? 3 : 1)) & 3; break;
    case 3: g_cfgHudGame = !g_cfgHudGame; break;
    case 4: g_cfgDownSecs += dir < 0 ? -30 : 30; if (g_cfgDownSecs < 30) g_cfgDownSecs = 120; if (g_cfgDownSecs > 120) g_cfgDownSecs = 30; break;
    case 5: g_cfgDeathMode = !g_cfgDeathMode; break;
    case 6: g_cfgAutoSpawn = !g_cfgAutoSpawn; break;
    case 7: g_cfgP2Pad = !g_cfgP2Pad; break;
    case 8: g_lanMode = (g_lanMode + 3 + (dir < 0 ? -1 : 1)) % 3; CfgSave(); Lan_Start(); break;   /* Lan_Start(OFF) = stop */
    case 9: if (dir > 0) { g_ipEdit = !g_ipEdit; if (!g_ipEdit) { CfgSave(); if (g_lanMode) Lan_Start(); } } break;
    case 10: g_lanPort += dir < 0 ? -1 : 1; if (g_lanPort < 1024) g_lanPort = 1024; if (g_lanPort > 65535) g_lanPort = 65535; CfgSave(); if (g_lanMode) Lan_Start(); break;
    case 11: g_lanSyncWorld = !g_lanSyncWorld; CfgSave(); break;
    case 12: if (dir > 0) Lan_TravelToPartner(); break;
    default: g_menuOpen = 0; break;
    }
    if (i >= 1 && i <= 7) { CfgSave(); Log("menu: setting %d changed (split=%d layout=%d hud=%d revive=%d death=%d auto=%d)", i,
                                         g_split, g_layout, g_cfgHudGame, g_cfgDownSecs, g_cfgDeathMode, g_cfgAutoSpawn); }
}

/* called every frame from Hook_UpdateWorld; returns 1 while the menu is open (world paused) */
static int MenuInput(void)
{
    static int kM, kU, kD, kL, kR, kE, kB;
    int focus = GameHasFocus();
    if (focus && HotKey(MENU_KEY, &kM)) {
        if (g_ipEdit) MenuChange(9, +1);          /* v0.12: closing while typing the IP = done */
        g_menuOpen = !g_menuOpen;
        Log("menu %s", g_menuOpen ? "opened (world paused)" : "closed");
        kU = KeyDown(VK_UP); kD = KeyDown(VK_DOWN); kL = KeyDown(VK_LEFT); kR = KeyDown(VK_RIGHT);
        kE = KeyDown(VK_RETURN); kB = KeyDown(VK_BACK);
        return g_menuOpen;
    }
    if (!g_menuOpen) return 0;
    if (rd32(A_GAME_STATE) != 0) { g_menuOpen = 0; return 0; }
    if (!focus) return 1;
    if (g_ipEdit && g_menuSel == 9) {             /* v0.12: typing the host IP: digits, '.', Backspace, Enter = done */
        static int kDig[24];
        int i, n = (int)strlen(g_lanIp);
        for (i = 0; i < 10; i++) {
            if (HotKey('0' + i, &kDig[i]) | HotKey(0x60 + i, &kDig[10 + i])) { if (n < 15) { g_lanIp[n++] = (char)('0' + i); g_lanIp[n] = 0; } }
        }
        if (HotKey(0xBE, &kDig[20]) | HotKey(0x6E, &kDig[21])) { if (n < 15) { g_lanIp[n++] = '.'; g_lanIp[n] = 0; } }
        if (HotKey(VK_BACK, &kB)) { if (n) g_lanIp[--n] = 0; }
        if (HotKey(VK_RETURN, &kE)) MenuChange(9, +1);
        kU = KeyDown(VK_UP); kD = KeyDown(VK_DOWN);
        return 1;
    }
    if (HotKey(VK_UP, &kU))    g_menuSel = (g_menuSel + MENU_ITEMS - 1) % MENU_ITEMS;
    if (HotKey(VK_DOWN, &kD))  g_menuSel = (g_menuSel + 1) % MENU_ITEMS;
    if (HotKey(VK_LEFT, &kL))  MenuChange(g_menuSel, -1);
    if (HotKey(VK_RIGHT, &kR)) MenuChange(g_menuSel, +1);
    if (HotKey(VK_RETURN, &kE)) MenuChange(g_menuSel, +1);
    if (HotKey(VK_BACK, &kB))  g_menuOpen = 0;
    return g_menuOpen;
}

static void AppendToOT0(const Chain *c)
{
    u32 n = rd32(A_OT_HEADS);
    int guard = 0;
    if (!c->head[0]) return;
    if (!n) { wr32(A_OT_HEADS, c->head[0]); return; }
    while (rd32(n) && guard++ < 60000) n = rd32(n);
    wr32(n, c->head[0]);
}

static void DrawCoopMenu(void)
{
    static Chain cm;
    float W, H, px, pw, ph, x0, y0, lh, vx;
    char label[48], value[48], st[64];
    int i;
    if (!g_menuOpen || !g_pool || rd32(A_GAME_STATE) != 0 || rd32(A_SW_RENDER) != 0 || !rd32(A_D3D_DEVICE) || !ScreenSize(&W, &H)) return;
    memset(&cm, 0, sizeof(cm));
    px = H / 240.0f; if (W / 320.0f < px) px = W / 320.0f; if (px < 1.0f) px = 1.0f;
    lh = 11.0f * px;
    pw = 280.0f * px; ph = (float)(MENU_ITEMS + 6) * lh + 10.0f * px;
    x0 = (W - pw) * 0.5f; y0 = (H - ph) * 0.5f;
    Box(&cm, x0 - 3.0f * px, y0 - 3.0f * px, x0 + pw + 3.0f * px, y0 + ph + 3.0f * px, 0xFFB01818u);   /* red frame (game colours) */
    Box(&cm, x0, y0, x0 + pw, y0 + ph, 0xFF0C1030u);                                                    /* dark blue panel          */
    Box(&cm, x0, y0, x0 + pw, y0 + lh + 4.0f * px, 0xFF1A2C78u);                                        /* title bar                */
    DrawText5x7(&cm, x0 + 8.0f * px, y0 + 3.0f * px, px, "SPIDER-MAN CO-OP  V0.12", 0xFFFFFFFFu);
    if (g_lanMode) { char ls[48]; Lan_StatusText(ls); wsprintfA(st, "LAN %s: %s", g_lanMode == 1 ? "HOST" : "JOIN", ls); }
    else if (!g_p2 || !InPlayerList(g_p2)) strcpy(st, "PLAYER 2: NOT IN GAME");
    else if (P2_FROZEN()) wsprintfA(st, "PLAYER 2: DOWN  %d:%02d", ((g_p2DownMs + 999) / 1000) / 60, ((g_p2DownMs + 999) / 1000) % 60);
    else wsprintfA(st, "PLAYER 2: %d / %d HP", g_p2VHp, g_p2MaxHp);
    DrawText5x7(&cm, x0 + 8.0f * px, y0 + lh + 8.0f * px, px, st, 0xFFFFD040u);
    vx = x0 + 120.0f * px;
    for (i = 0; i < MENU_ITEMS; i++) {
        float y = y0 + (float)(i + 2) * lh + 12.0f * px;
        int sel = (i == g_menuSel);
        MenuItemText(i, label, value);
        if (sel) Box(&cm, x0 + 4.0f * px, y - 2.0f * px, x0 + pw - 4.0f * px, y + 9.0f * px, 0xFFB01818u);
        DrawText5x7(&cm, x0 + 10.0f * px, y, px, label, sel ? 0xFFFFFFFFu : 0xFFA8B4E0u);
        if (value[0]) DrawText5x7(&cm, vx, y, px, value, sel ? 0xFFFFFFFFu : 0xFFE0E0E0u);
    }
    DrawText5x7(&cm, x0 + 8.0f * px, y0 + ph - 2.0f * lh - 2.0f * px, px, g_lanMode ? "NICK AND HOST IP: COOP.INI OR THIS MENU" : "PVP MODE: IN DEVELOPMENT", 0xFF7080B0u);
    DrawText5x7(&cm, x0 + 8.0f * px, y0 + ph - lh, px, "UP/DOWN  LEFT/RIGHT  ENTER   F6 CLOSE", 0xFF7080B0u);
    (void)TextW;
    AppendToOT0(&cm);
}

/* style 1: small extra elements next to the game's HUD in P2's view, placed in the HUD's own 512x240 virtual
 * screen (+ the HUD offset DAT_0060F76C) and scaled into P2's rectangle like the game's HUD (non-uniform). */
static void DrawP2HudExtras(Chain *c, const Rect *r)
{
    float vw = (float)SaneInt(A_PSX_W, 64, 4096, 512), vh = (float)SaneInt(A_PSX_H, 64, 4096, 240);
    float kx = (r->x1 - r->x0) / vw, ky = (r->y1 - r->y0) / vh, lb = (float)g_p2HudLb;
    float x, y, dw, dh;
    DWORD now = GetTickCount();
#define HX(v) (r->x0 + (float)(v) * kx)
#define HY(v) (r->y0 + ((float)(v) + lb) * ky)

    if (!g_p2 || !rd32(A_HUD_ENABLED)) return;                       /* the game hides its HUD (cut-scene...) */
    if (rd32(g_p2 + OFF_HUD_OFF1) || rd8(g_p2 + OFF_HUD_OFF2)) return; /* DrawHud skipped P2's part as well   */
    dw = 6.0f * kx; dh = 9.0f * ky;

#if P2_HUD_TAG
    x = (float)P2_TAG_VX; y = (float)P2_TAG_VY;
    Box(c, HX(x) - 1.0f, HY(y) - 1.0f, HX(x + 24.0f) + 1.0f, HY(y + 13.0f) + 1.0f, 0xFF000000u);
    Box(c, HX(x), HY(y), HX(x + 24.0f), HY(y + 13.0f), 0xFF2060E0u);
    Glyph(c, HX(x + 4.0f),  HY(y + 2.0f), dw, dh, SEG_P,    0xFFFFFFFFu);
    Glyph(c, HX(x + 14.0f), HY(y + 2.0f), dw, dh, kSeg7[2], 0xFFFFFFFFu);
#endif

    /* row: deaths counted (red cross + count), then the downed badge (! + m:ss) */
    x = (float)P2_EXTRA_VX; y = (float)P2_EXTRA_VY;
    if (g_p2Deaths > 0) {
        Box(c, HX(x - 1.0f), HY(y + 3.0f), HX(x + 11.0f), HY(y + 6.0f), 0xFF000000u);
        Box(c, HX(x),        HY(y + 3.5f), HX(x + 10.0f), HY(y + 5.5f), 0xFFE03030u);
        Box(c, HX(x + 3.5f), HY(y - 0.5f), HX(x + 6.5f),  HY(y + 9.5f), 0xFF000000u);
        Box(c, HX(x + 4.0f), HY(y),        HX(x + 6.0f),  HY(y + 9.0f), 0xFFE03030u);
        Number2(c, HX(x + 14.0f), HY(y), dw, dh, g_p2Deaths, 0xFFFF8080u);
        x += 36.0f;
    }
    if (g_p2Down) {
        int secs = (g_p2DownMs + 999) / 1000;
        u32 badge = (g_p2Down == 2) ? 0xFF6A6A6Au
                  : LerpCol(0xFF901010u, 0xFFFF3020u, 0.5f + 0.5f * (float)sin((double)now * 0.0105));
        if (secs < 0) secs = 0;
        Box(c, HX(x) - 1.0f, HY(y - 1.0f) - 1.0f, HX(x + 12.0f) + 1.0f, HY(y + 10.0f) + 1.0f, 0xFF000000u);
        Box(c, HX(x), HY(y - 1.0f), HX(x + 12.0f), HY(y + 10.0f), badge);
        Box(c, HX(x + 5.0f), HY(y + 0.5f), HX(x + 7.0f), HY(y + 6.0f), 0xFFFFFFFFu);   /* "!" */
        Box(c, HX(x + 5.0f), HY(y + 7.0f), HX(x + 7.0f), HY(y + 8.5f), 0xFFFFFFFFu);
        if (g_p2Down == 1) {                                                        /* lying: countdown */
            u32 col = (secs <= 10 && ((now / 180u) & 1u)) ? 0xFFFF6060u : 0xFFFFFFFFu;
            x += 16.0f;
            Glyph(c, HX(x), HY(y), dw, dh, kSeg7[(secs / 60) % 10], col);
            Box(c, HX(x + 7.0f), HY(y + 2.0f), HX(x + 8.5f), HY(y + 3.5f), col);
            Box(c, HX(x + 7.0f), HY(y + 6.0f), HX(x + 8.5f), HY(y + 7.5f), col);
            Number2(c, HX(x + 10.0f), HY(y), dw, dh, secs % 60, col);
        }
    }
#undef HX
#undef HY
}

/* ---- camera state: the camera object + the camera's static globals ---- */
#define CAMG3_ADDR 0x0056EFF8u   /* yaw tween rate, written by FUN_00417AE0 (outside the g1 block) */
#define CAMG3_SIZE 0x4u
#define CAMX_COUNT 7             /* aim-mode globals (FUN_004C3580 / FUN_004C38A0 / FUN_004C0510): pitch, prev heading, smoothed pitch, YAW, smoothed yaw,
                                  * v0.8: + aim bar height (DAT_00660F80, drawn by FUN_004737F0) + its HUD copy (DAT_0060F76C) */
#define CAMX_LB    5             /* index of A_LETTERBOX in kCamX  */
#define CAMX_LB2   6             /* index of A_LETTERBOX2 in kCamX */
static const u32 kCamX[CAMX_COUNT] = { 0x006A818Cu, 0x006A8D44u, 0x006A82B4u, 0x006A7FFCu, 0x006A8D54u, A_LETTERBOX, A_LETTERBOX2 };
typedef struct { u8 cam[CAM_SIZE]; u8 g1[0x400]; u8 g2[0x20]; u8 g3[CAMG3_SIZE]; u32 gx[CAMX_COUNT]; } CamState;
#define CAMG1_ADDR 0x0056F000u
#define CAMG1_SIZE 0x400u
#define CAMG2_ADDR 0x00548850u
#define CAMG2_SIZE 0x20u
static CamState g_stA, g_stB;
static int g_stBValid = 0;
static u32 g_bCam = 0;

static void CaptureState(CamState *s, u32 cam)
{
    memcpy(s->cam, (const void *)(uintptr_t)cam, CAM_SIZE);
    memcpy(s->g1,  (const void *)(uintptr_t)CAMG1_ADDR, CAMG1_SIZE);
    memcpy(s->g2,  (const void *)(uintptr_t)CAMG2_ADDR, CAMG2_SIZE);
    memcpy(s->g3,  (const void *)(uintptr_t)CAMG3_ADDR, CAMG3_SIZE);
    { int i; for (i = 0; i < CAMX_COUNT; i++) s->gx[i] = rd32(kCamX[i]); }
}

/* the object header (vtable, flags, ids, list links at +0x20/+0x34) is NOT touched */
static void ApplyState(const CamState *s, u32 cam)
{
    memcpy((void *)(uintptr_t)(cam + 0x08), s->cam + 0x08, 0x1A - 0x08);
    memcpy((void *)(uintptr_t)(cam + 0x40), s->cam + 0x40, CAM_SIZE - 0x40);
    memcpy((void *)(uintptr_t)CAMG1_ADDR, s->g1, CAMG1_SIZE);
    memcpy((void *)(uintptr_t)CAMG2_ADDR, s->g2, CAMG2_SIZE);
    memcpy((void *)(uintptr_t)CAMG3_ADDR, s->g3, CAMG3_SIZE);
    { int i; for (i = 0; i < CAMX_COUNT; i++) wr32(kCamX[i], s->gx[i]); }
}

static void InitCamB(u32 cam, u32 p1, u32 p2)
{
    static const u32 vec[3] = { 0x08u, 0x104u, 0x110u };      /* camera position, tripod now, tripod target */
    s32 d[3];
    int i, k;
    CaptureState(&g_stB, cam);
    for (i = 0; i < 3; i++) d[i] = (s32)rd32(p2 + OFF_POS + 4u * (u32)i) - (s32)rd32(p1 + OFF_POS + 4u * (u32)i);
    for (k = 0; k < 3; k++)
        for (i = 0; i < 3; i++) {
            s32 *f = (s32 *)(g_stB.cam + vec[k] + 4u * (u32)i);
            *f += d[i];
        }
    *(u32 *)(g_stB.cam + CAM_OFF_FOCUS)  = p2;
    *(u32 *)(g_stB.cam + CAM_OFF_FOCUS2) = p2;
#if CUTSCENE_FIX
    {   /* v0.8c: never inherit P1's scripted (4/5/6) or aim (7) camera: nothing would ever switch B back
         * (scripts only write the live camera = context A). Force follow mode, clear bars, snap to P2. */
        u32 *mode = (u32 *)(g_stB.cam + CAM_OFF_MODE);
        int p2aim = rd8(p2 + OFF_AIMFLAG) != 0;
        if (*mode != 3 && !(*mode == 7 && p2aim)) {
            Log("split: camera B inherited mode %d from A -> forced to 3 (follow P2), bars cleared", (int)*mode);
            *mode = 3;
            *(u32 *)(g_stB.cam + 0x280) = 3;     /* saved mode (FUN_00416720/00416780) */
            *(u32 *)(g_stB.cam + 0x2BC) = 0;     /* fixed-camera transition frames     */
            g_bSnap = 1;
        }
        if (!p2aim) { g_stB.gx[CAMX_LB] = 0; g_stB.gx[CAMX_LB2] = 0; }
    }
#endif
    g_stBValid = 1;
    g_bCam = cam;
    Log("split: camera B created from camera A (cam=%08X, mode=%d), shifted by (%d,%d,%d)", cam,
        (int)rd32(cam + CAM_OFF_MODE), d[0], d[1], d[2]);
}

/* v0.8c: cut-scene signals. Call ONLY while context A is live (not inside P2's swapped Update). */
static int CutsceneSignals(void)
{
    u32 p1 = rd32(A_PLAYER_HEAD), cam = rd32(A_CAMERA);
    int s = 0;
    if (p1 && p1 != g_p2) {
        /* P2's aim bars land in the shared globals only when P2's Update runs without its own context */
        int p2AimShared = g_p2 && rd8(g_p2 + OFF_AIMFLAG) && !(g_split && g_stBValid && cam && g_bCam == cam);
        if ((s32)rd32(A_LETTERBOX) > 0 && !rd8(p1 + OFF_AIMFLAG) && !p2AimShared) s |= CS_SIG_LETTERBOX;
        if ((s32)rd32(p1 + OFF_IGNOREINPUT) >= CUTSCENE_E18_MIN)                    s |= CS_SIG_IGNOREINPUT;
        if (rd8(p1 + OFF_CUTSCENE))                                                 s |= CS_SIG_SCRIPT;
    }
    if (cam) { u32 m = rd32(cam + CAM_OFF_MODE); if (m == 4 || m == 5 || m == 6) s |= CS_SIG_FIXEDCAM; }
    return s;
}

/* put the list in the order [first, second] (head = first) */
static void LinkOrder(u32 first, u32 second)
{
    wr32(A_PLAYER_HEAD, first);
    wr32(first  + OFF_PREV, 0);
    wr32(first  + OFF_NEXT, second);
    wr32(second + OFF_PREV, first);
    wr32(second + OFF_NEXT, 0);
}

static const u32 kHudGates[4] = { 0x005FA958u, 0x006B559Cu, 0x0060CF94u, 0x006B55A4u };

static int ScreenSize(float *W, float *H)
{
    s32 w = (s32)rd32(A_SCREEN_W), h = (s32)rd32(A_SCREEN_H);
    if (w < 64 || w > 8192 || h < 64 || h > 8192) return 0;
    *W = (float)w; *H = (float)h;
    return 1;
}

/* returns NULL when split-screen can be used this frame, otherwise the reason (for the log) */
static const char *SplitCheck(u32 *camOut, u32 *p1Out, float *W, float *H)
{
    u32 cam, p1;
    if (!g_split)                 return "split-screen is switched off (F8)";
    if (!g_splitOk)               return "render hook not installed";
    if (!g_p2)                    return "player 2 does not exist";
#if CUTSCENE_FIX
    if (g_cutscene)               return "cut-scene / scripted camera active (P1 full screen, P2 waits)";
#endif
    if (rd32(A_GAME_STATE) != 0)  return "game state != 0 (not in play)";
    if (rd32(A_SW_RENDER) != 0)   return "software renderer is in use (only Direct3D is supported)";
    if (!rd32(A_D3D_DEVICE))      return "no Direct3D device";
    cam = rd32(A_CAMERA);
    p1  = rd32(A_PLAYER_HEAD);
    if (!cam)                     return "no camera object";
    if (!p1 || p1 == g_p2)        return "no player 1";
    if (rd32(p1 + OFF_NEXT) != g_p2 || rd32(g_p2 + OFF_NEXT) != 0) return "player list is not exactly [P1, P2]";
    if (!ScreenSize(W, H))        return "screen size globals look invalid";
    *camOut = cam; *p1Out = p1;
    return NULL;
}

static void __cdecl Hook_Render(void)
{
    static unsigned splitFrames = 0;
    PfnVoid render = (PfnVoid)(uintptr_t)A_RENDER;
    u32 cam, p1, saved[4], h;
    float W, H;
    Rect ra, rb;
    CollectStat s1, s2;
    u8 zsave;
    int i, lbMerged = 0;
    u32 lbOwn = 0, lbOwn2 = 0;

    {
        static const char *lastWhy = (const char *)1;
        const char *why = SplitCheck(&cam, &p1, &W, &H);
        if (why != lastWhy && (g_split && g_p2)) {
            if (why) Log("split-screen NOT active: %s   (screen globals: %d x %d, sw=%d, device=%08X, scene=%d, zbuf=%d)", why,
                         (int)rd32(A_SCREEN_W), (int)rd32(A_SCREEN_H), (int)rd32(A_SW_RENDER), rd32(A_D3D_DEVICE), rd8(A_SCENE_FLAG), rd8(A_ZBUF_FLAG));
            else     Log("split-screen ACTIVE");
            lastWhy = why;
        }
        if (why) { render(); g_poolUsed = 0; if (!g_cutscene) DrawMarkerSingleView(); DrawCoopMenu(); return; }
    }

    g_stage = 10;
    if (rd8(A_SCENE_FLAG) == 0) {         /* the game would do this on its first draw call of the frame */
        ((void (__cdecl *)(int, int))(uintptr_t)A_BEGIN_SCENE)(1, -1);
        if (rd8(A_SCENE_FLAG) == 0) { Log("split: BeginFrame did not start the scene -> frame drawn normally"); render(); return; }
    }
    splitFrames++;
    MakeViews(W, H, &ra, &rb);
    zsave = rd8(A_ZBUF_FLAG);
    wr8(A_ZBUF_FLAG, 0);              /* z-buffer mode would draw opaque polygons at once, past our buckets */
    g_poolUsed = 0;

    /* ---------------- pass 1: player 1, with the live camera ---------------- */
    g_stage = 11;
    render();
    g_stage = 12;
    g_markA = ProjectP2(W, H);        /* camera A's matrix is current right after P1's pass */
    CollectPass(&ra, W, H, &g_c1, &s1);
    DrawDownMarker(&g_c1, &ra, g_markA, W, H, 1);
    ClearOT();

    /* ---------------- pass 2: player 2, with camera B ---------------- */
    g_stage = 13;
    if (!g_stBValid || g_bCam != cam) InitCamB(cam, p1, g_p2);
    CaptureState(&g_stA, cam);
    ApplyState(&g_stB, cam);
    wr32(cam + CAM_OFF_FOCUS,  g_p2);
    wr32(cam + CAM_OFF_FOCUS2, g_p2);
#if CUTSCENE_FIX
    if (g_bSnap) {                    /* the game's own snap (FUN_00416AA0, also used at a cut-scene skip) */
        ((void (__fastcall *)(void *, void *))(uintptr_t)A_CAM_SNAP)((void *)(uintptr_t)cam, NULL);
        g_bSnap = 0;
    }
#endif
    /* aim bars are per player now (kCamX). Cut-scene bars are written by scripts in P1's context: when P1 is
     * NOT aiming, A's bar value is a cut-scene bar and P2's view shows it too (P2's own value is restored after) */
    lbOwn = rd32(A_LETTERBOX); lbOwn2 = rd32(A_LETTERBOX2); lbMerged = 0;
    if (!rd8(p1 + OFF_AIMFLAG) && (s32)g_stA.gx[CAMX_LB] > (s32)lbOwn) {
        wr32(A_LETTERBOX, g_stA.gx[CAMX_LB]); wr32(A_LETTERBOX2, g_stA.gx[CAMX_LB2]); lbMerged = 1;
    }
    LinkOrder(g_p2, p1);              /* "the player" (list head) is now P2 */
#if P2_HIDE_GAME_HUD
    for (i = 0; i < 4; i++) { saved[i] = rd32(kHudGates[i]); wr32(kHudGates[i], 0); }
#endif
    g_stage = 14;
#if P2_CUSTOM_HUD
    g_p2GameHudRan = 0;
# if P2_HUD_STYLE == 1
    g_p2GameHud = g_cfgHudGame && g_hudHooked && g_hudTimerHooked;   /* game-style HUD for P2 (menu: P2 HUD) */
    g_hideHud   = g_hudHooked && !g_p2GameHud;       /* else skipped, P2 gets the v0.8 quads (DrawP2Hud)    */
# else
    g_hideHud = g_hudHooked;          /* the game's HUD call is skipped while P2's view is rendered */
# endif
#endif
    render();
    g_hideHud = 0;
    g_p2GameHud = 0;
    g_p2HudLb = (s32)rd32(A_LETTERBOX2);  /* the HUD offset P2's HUD was just drawn with (before A's is restored) */
    g_stage = 15;
    g_markB = ProjectP2(W, H);        /* camera B's matrix is current right after P2's pass */
    if (lbMerged) { wr32(A_LETTERBOX, lbOwn); wr32(A_LETTERBOX2, lbOwn2); }
#if P2_HIDE_GAME_HUD
    for (i = 0; i < 4; i++) wr32(kHudGates[i], saved[i]);
#endif
    LinkOrder(p1, g_p2);              /* back to [P1, P2] */
    CaptureState(&g_stB, cam);
    ApplyState(&g_stA, cam);          /* camera A continues exactly where pass 1 left it */
    CollectPass(&rb, W, H, &g_c2, &s2);
#if P2_CUSTOM_HUD
    if (g_p2GameHudRan) DrawP2HudExtras(&g_c2, &rb);  /* game HUD already collected: only the small extras */
    else                DrawP2Hud(&g_c2, &rb);        /* fallback: v0.8 flat-quad HUD                     */
#endif
    DrawDownMarker(&g_c2, &rb, g_markB, W, H, 0);
    DrawDivider(&g_c2, W, H);
    ClearOT();

    /* ---------------- merge both views back into the game's buckets ---------------- */
    g_stage = 16;
    for (h = 0; h < OT_BUCKETS; h++) {
        u32 a = g_c1.head[h], b = g_c2.head[h], head;
        if (a) { head = a; if (b) wr32(g_c1.tail[h], b); } else head = b;
        wr32(A_OT_HEADS + 4u * h, head);
    }
    wr8(A_ZBUF_FLAG, zsave);
    DrawCoopMenu();

    if (splitFrames <= 3 || (splitFrames % 900) == 0) {
        Log("split frame %u: screen %.0fx%.0f layout=%d zbuf=%d | P1 view: nodes=%u kept=%u dropped=%u extents x[%.0f..%.0f] y[%.0f..%.0f]",
            splitFrames, W, H, g_layout, zsave, s1.nodes, s1.kept, s1.dropped, s1.minx, s1.maxx, s1.miny, s1.maxy);
        Log("             P2 view: nodes=%u kept=%u dropped=%u extents x[%.0f..%.0f] y[%.0f..%.0f] | pool %u of %u KB | camB mode=%d",
            s2.nodes, s2.kept, s2.dropped, s2.minx, s2.maxx, s2.miny, s2.maxy,
            (unsigned)(g_poolUsed / 1024u), (unsigned)(POOL_BYTES / 1024u), (int)(*(u32 *)(g_stB.cam + CAM_OFF_MODE)));
        Log("             P2 hud: own=%d style=%d game hud for P2=%d (hooks %d/%d) vhp=%d/%d max(+EF0)=%d | fluid=0x%X carts=%d | P1 fluid=0x%X carts=%d",
            P2_CUSTOM_HUD, P2_HUD_STYLE, g_p2GameHudRan, g_hudHooked, g_hudTimerHooked, g_p2VHp, g_p2MaxHp, (int)rd32(g_p2 + OFF_MAXHP),
            (unsigned)rd32(g_p2 + OFF_WEB_FLUID), (int)rd32(g_p2 + OFF_WEB_CARTS),
            (unsigned)rd32(p1 + OFF_WEB_FLUID), (int)rd32(p1 + OFF_WEB_CARTS));
    }
    g_stage = 17;
}


/* ------------------------------------------------------------------ */
/*  Player 2 runs inside its OWN camera context (v0.5)                 */
/*  The game keeps ONE global camera (A_CAMERA + a block of globals).   */
/*  CPlayer::Update (vtable slot +8, 0x4C65C0) converts pad input to    */
/*  world directions with the camera's yaw and pushes camera presets    */
/*  (FUN_00417AE0 ...) when the player starts to run/swing/land.        */
/*  P2 gets a private copy of the vtable; its Update slot swaps camera  */
/*  B in for the duration of P2's Update.                               */
/* ------------------------------------------------------------------ */
#define A_PLAYER_VTBL_UPDATE 0x004C65C0u
typedef void (__fastcall *PfnEntUpdate)(void *self, void *edx);
static PfnEntUpdate g_origP2Update = NULL;
static u32 g_p2Vtbl[256];
static int g_inP2Update = 0;
static int g_p2CamSwaps = 0;

static void __fastcall Hook_P2Update(void *self, void *edx)
{
    u32 cam = rd32(A_CAMERA);
    if (P2_FROZEN() && (u32)(uintptr_t)self == g_p2) return;   /* lying / dead: frozen (animation still runs) */
#if CUTSCENE_FIX
    /* cut-scene without an own camera context for P2: P2 must not touch the script camera -> frozen */
    if (g_cutscene && (u32)(uintptr_t)self == g_p2 && !(g_split && g_stBValid && cam && g_bCam == cam)) return;
#endif
    if (g_split && g_stBValid && g_p2 && (u32)(uintptr_t)self == g_p2 && cam && g_bCam == cam && !g_inP2Update) {
        g_inP2Update = 1;
        g_stage = 20;
        CaptureState(&g_stA, cam);          /* P1's camera, exactly as it is now          */
        ApplyState(&g_stB, cam);            /* P2's camera becomes "the" camera           */
        g_origP2Update(self, edx);
        CaptureState(&g_stB, cam);          /* keep whatever P2's Update did to its camera */
        {
            static u32 lastB = 0xFFFFFFFFu; u32 m = rd32(cam + CAM_OFF_MODE);
            if (m != lastB) { Log("P2 camera B mode %d -> %d (frame %u)", (int)lastB, (int)m, g_frame); lastB = m; }
        }
        ApplyState(&g_stA, cam);            /* P1's camera is back, untouched             */
        g_stage = 21;
        g_inP2Update = 0;
        if (++g_p2CamSwaps == 1) Log("P2 Update now runs in camera-B context (own camera for P2's input and swing logic)");
    } else {
        g_origP2Update(self, edx);
    }
}

static void InstallP2Vtable(u32 p2)
{
    u32 vt = rd32(p2);
    if (!vt || IsBadReadPtr((const void *)(uintptr_t)vt, sizeof(g_p2Vtbl))) { Log("P2 vtable not readable (%08X) - P2 keeps the shared camera", vt); return; }
    if (rd32(vt + 8) != A_PLAYER_VTBL_UPDATE) {
        Log("P2 vtable %08X slot+8 = %08X (expected %08X) - P2 keeps the shared camera", vt, rd32(vt + 8), A_PLAYER_VTBL_UPDATE);
        return;
    }
    memcpy(g_p2Vtbl, (const void *)(uintptr_t)vt, sizeof(g_p2Vtbl));
    g_origP2Update = (PfnEntUpdate)(uintptr_t)g_p2Vtbl[2];
    g_p2Vtbl[2] = (u32)(uintptr_t)Hook_P2Update;
    wr32(p2, (u32)(uintptr_t)g_p2Vtbl);
    g_p2CamSwaps = 0;
    Log("P2 private vtable installed (orig %08X, Update slot -> camera-context thunk)", vt);
}

/* v0.11: P2's state survives level changes. The constructor gives a new P2 full health and the game's carry-over
 * web values (= P1's), so at every level end P2's own values are saved and given back to the next P2.
 * A checkpoint restart (game state 2/9 at DestroyAll) keeps the values P2 had when the level started. */
typedef struct { int valid, vhp, fluid, carts, armor; u8 armorOn; } P2Carry;
static P2Carry g_carry;
#define OFF_ARMOR_ON 0x5E9u
#define OFF_ARMOR    0x5ECu

static void P2SaveCarry(const char *why)
{
    if (!g_p2) return;
    g_carry.vhp     = P2_FROZEN() || g_p2Down == 3 ? g_p2MaxHp * P2_REVIVE_HP_PCT / 100 : g_p2VHp;
    g_carry.fluid   = (s32)rd32(g_p2 + OFF_WEB_FLUID);
    g_carry.carts   = (s32)rd32(g_p2 + OFF_WEB_CARTS);
    g_carry.armorOn = rd8(g_p2 + OFF_ARMOR_ON);
    g_carry.armor   = (s32)rd32(g_p2 + OFF_ARMOR);
    g_carry.valid   = 1;
    Log("P2 state saved (%s): health %d/%d web 0x%X cartridges %d armour %d/%d", why, g_carry.vhp, g_p2MaxHp,
        (unsigned)g_carry.fluid, g_carry.carts, g_carry.armorOn, g_carry.armor);
}

static void P2ApplyCarry(void)
{
    if (!g_p2 || !g_carry.valid) return;
    g_p2VHp = g_carry.vhp;
    if (g_p2VHp > g_p2MaxHp) g_p2VHp = g_p2MaxHp;
    if (g_p2VHp < 1) g_p2VHp = 1;
    if (g_carry.fluid >= 0 && g_carry.fluid <= 0x1000) wr32(g_p2 + OFF_WEB_FLUID, (u32)g_carry.fluid);
    if (g_carry.carts >= 0 && g_carry.carts <= 99)     wr32(g_p2 + OFF_WEB_CARTS, (u32)g_carry.carts);
    wr8(g_p2 + OFF_ARMOR_ON, g_carry.armorOn);
    wr32(g_p2 + OFF_ARMOR, (u32)g_carry.armor);
    Log("P2 state restored: health %d/%d web 0x%X cartridges %d (P1 has web 0x%X cartridges %d)", g_p2VHp, g_p2MaxHp,
        (unsigned)rd32(g_p2 + OFF_WEB_FLUID), (int)rd32(g_p2 + OFF_WEB_CARTS),
        (unsigned)rd32(rd32(A_PLAYER_HEAD) + OFF_WEB_FLUID), (int)rd32(rd32(A_PLAYER_HEAD) + OFF_WEB_CARTS));
}

static void SpawnPlayer2(void)
{
    u32 p1, mem, p2, old, tail;
    int guard;

    p1 = rd32(A_PLAYER_HEAD);
    if (g_lanMode)                   { Log("F9/auto-spawn: no local player 2 in LAN mode"); g_spawnPending = 0; return; }
    if (!p1)                         { Log("F9: no player 1 yet (not in a level)"); return; }
    if (rd32(A_GAME_STATE) != 0)     { Log("F9: game state != 0 (not in play)");     return; }
    if (g_p2 && InPlayerList(g_p2))  { Log("F9: player 2 already exists (%08X)", g_p2); return; }
    g_p2 = 0;

    Log("F9: spawning player 2. P1=%08X pos=(%d,%d,%d) hp=%d", p1,
        (s32)rd32(p1 + OFF_POS), (s32)rd32(p1 + OFF_POS + 4), (s32)rd32(p1 + OFF_POS + 8), rd16(p1 + OFF_HEALTH));

    SnapshotRegions();
    mem = (u32)(uintptr_t)((PfnAlloc)(uintptr_t)A_ALLOC)((int)PLAYER_SIZE);
    if (!mem) { Log("alloc failed"); return; }
    p2 = (u32)(uintptr_t)((PfnCtor)(uintptr_t)A_PLAYER_CTOR)((void *)(uintptr_t)mem);
    RestoreRegions();
    if (!p2) { Log("ctor returned NULL"); return; }
    g_p2MaxHp = rd16(p2 + OFF_HEALTH);
    if (g_p2MaxHp < 1) g_p2MaxHp = 100;
    g_p2VHp = g_p2MaxHp;
    g_p2Pin = g_p2MaxHp * 6; if (g_p2Pin > 30000) g_p2Pin = 30000;
    wr16(p2 + OFF_HEALTH, (s16)g_p2Pin);            /* pin now, so the first frame does not count as "damage" */
    Log("ctor ok, P2=%08X, list head now %08X, P2 max health from ctor = %d", p2, rd32(A_PLAYER_HEAD), g_p2MaxHp);

    /* the ctor inserted P2 at the HEAD of the list; put P1 back in front */
    if (rd32(A_PLAYER_HEAD) == p2) {
        old = rd32(p2 + OFF_NEXT);
        if (old) {
            wr32(A_PLAYER_HEAD, old);
            wr32(old + OFF_PREV, 0);
            wr32(p2 + OFF_NEXT, 0);
            tail = old; guard = 0;
            while (rd32(tail + OFF_NEXT) && guard++ < 16) tail = rd32(tail + OFF_NEXT);
            wr32(tail + OFF_NEXT, p2);
            wr32(p2 + OFF_PREV, tail);
        }
    }

    /* own pad block + skip the first-update init (it touches the camera) */
    memset(g_pad2, 0, sizeof(g_pad2));
    wr32((u32)(uintptr_t)g_pad2 + 0x16C, 0x41);
    wr32(p2 + OFF_PAD, (u32)(uintptr_t)g_pad2);
    wr8 (p2 + OFF_INITDONE, 1);
    InstallP2Vtable(p2);
    g_p2Down = 0; g_p2Invuln = 0; g_p2ReviveHold = 0; g_p1Near = 0; P2DownTitle(-1);

    /* put it next to player 1 */
    wr32(p2 + OFF_POS,     rd32(p1 + OFF_POS) + P2_OFFSET_X);
    wr32(p2 + OFF_POS + 4, rd32(p1 + OFF_POS + 4));
    wr32(p2 + OFF_POS + 8, rd32(p1 + OFF_POS + 8));

    g_p2 = p2;
    P2ApplyCarry();                                     /* v0.11: P2 keeps his own health / web between levels */
    g_stBValid = 0;                                     /* camera B is (re)created at the first split frame */
    { u32 cam = rd32(A_CAMERA);
      Log("camera=%08X focus(+0xFC)=%08X mode(+0x2A0)=%d  (focus should equal P1=%08X) split=%d layout=%d", cam,
          cam ? rd32(cam + CAM_OFF_FOCUS) : 0, cam ? (int)rd32(cam + CAM_OFF_MODE) : -1, p1, g_split, g_layout); }
    Log("list: head=%08X head.next=%08X (P1=%08X P2=%08X)", rd32(A_PLAYER_HEAD),
        rd32(rd32(A_PLAYER_HEAD) + OFF_NEXT), p1, p2);
}

static void TeleportPlayer2(const char *why)
{
    u32 p1 = rd32(A_PLAYER_HEAD);
    if (!g_p2 || !p1 || p1 == g_p2) return;
    wr32(g_p2 + OFF_POS,     rd32(p1 + OFF_POS) + P2_OFFSET_X);
    wr32(g_p2 + OFF_POS + 4, rd32(p1 + OFF_POS + 4));
    wr32(g_p2 + OFF_POS + 8, rd32(p1 + OFF_POS + 8));
    if (why) Log("%s: P2 teleported next to P1", why);
}

static void RemovePlayer2(void)
{
    if (!g_p2) return;
    P2SaveCarry("removed by the user");
    g_stBValid = 0;
    if (InPlayerList(g_p2)) UnlinkPlayer(g_p2);   /* NOT destroyed: its destructor would overwrite P1's carry-over state */
    Log("P2 %08X removed by the user (unlinked, leaked on purpose); no auto-spawn until the next level", g_p2);
    g_userRemoved = 1;
    g_p2 = 0; g_p2Down = 0; P2DownTitle(-1);
}

/* ------------------------------------------------------------------ */
/*  Player 2 input: same pad layout as the game's own input poll       */
/*  (FUN_00505720): 16-byte button entries updated by PadEntry().      */
/* ------------------------------------------------------------------ */
static const struct { u32 off; u32 bit; } kEntries[] = {
    {0x00,0x80},{0x10,0x40},{0x20,0x20},{0x30,0x10},{0x40,0x100},{0x50,0x00},{0x60,0x200},{0x70,0x400},
    {0x80,0x04},{0x90,0x08},{0xA0,0x01},{0xB0,0x02},{0xC0,0x00},{0xD0,0x00},{0xE0,0x1000},{0xF0,0x00},
    {0x100,0x10},{0x110,0x80},{0x120,0x40},{0x130,0x20}
};

/* bit -> key. Bits are the game's action bits (default P1 bindings in brackets). */
static const struct { int vk; u32 bit; } kP2Keys[] = {
    {'W',0x01},{'S',0x02},{'A',0x04},{'D',0x08},          /* up/down/left/right (arrows)  */
    {'K',0x10},{'L',0x20},{'J',0x40},{'I',0x80},          /* jump/kick/punch/web (Num2/6/4/8) */
    {'U',0x100},{'P',0x200},{'O',0x400},{'H',0x1000}      /* Num7 / Num3 / Num9 / Enter    */
};

/* v0.11: optional XInput gamepad for P2 (menu: P2 GAMEPAD). Own structs, the DLL is loaded on demand. */
typedef struct { WORD wButtons; BYTE bLT, bRT; SHORT lx, ly, rx, ry; } XPad;
typedef struct { DWORD packet; XPad pad; } XPadState;
typedef DWORD (WINAPI *PfnXInputGetState)(DWORD, XPadState *);
static PfnXInputGetState g_xget = NULL;
static int g_xtried = 0;

static u32 ReadP2Gamepad(void)
{
    static const char *dlls[3] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    static const struct { WORD b; u32 bit; } map[] = {
        {0x0001,0x01},{0x0002,0x02},{0x0004,0x04},{0x0008,0x08},      /* d-pad  -> up/down/left/right   */
        {0x1000,0x10},{0x2000,0x20},{0x4000,0x40},{0x8000,0x80},      /* A jump, B kick, X punch, Y web */
        {0x0100,0x100},{0x0200,0x200},{0x0010,0x1000}                 /* LB, RB, START = Enter          */
    };
    XPadState st;
    u32 m = 0;
    size_t i;
    if (!g_xget && !g_xtried) {
        g_xtried = 1;
        for (i = 0; i < 3 && !g_xget; i++) {
            HMODULE h = LoadLibraryA(dlls[i]);
            if (h) g_xget = (PfnXInputGetState)(void *)GetProcAddress(h, "XInputGetState");
        }
        Log("P2 gamepad: XInput %s", g_xget ? "loaded" : "NOT found (install DirectX / use a newer Windows)");
    }
    if (!g_xget) return 0;
    memset(&st, 0, sizeof(st));
    if (g_xget(0, &st) != 0) return 0;                          /* no controller 1 */
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) if (st.pad.wButtons & map[i].b) m |= map[i].bit;
    if (st.pad.ly >  12000) m |= 0x01;                          /* left stick */
    if (st.pad.ly < -12000) m |= 0x02;
    if (st.pad.lx < -12000) m |= 0x04;
    if (st.pad.lx >  12000) m |= 0x08;
    if (st.pad.bRT > 100)   m |= 0x400;                         /* RT = the third extra action (O) */
    return m;
}

static void UpdatePlayer2Pad(void)
{
    u32 mask = 0;
    size_t i;
    PfnPadEntry fn = (PfnPadEntry)(uintptr_t)A_PAD_ENTRY;

    if (GameHasFocus())
        for (i = 0; i < sizeof(kP2Keys) / sizeof(kP2Keys[0]); i++)
            if (KeyDown(kP2Keys[i].vk)) mask |= kP2Keys[i].bit;
    if (g_cfgP2Pad && GameHasFocus()) mask |= ReadP2Gamepad();

    if (g_p2Down || g_cutscene) mask = 0;   /* falling / lying / dead / cut-scene: no input */
    wr32((u32)(uintptr_t)g_pad2 + 0x16C, 0x41);
    for (i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); i++)
        fn((char *)(g_pad2 + kEntries[i].off), (int)(mask & kEntries[i].bit));
}

/* ------------------------------------------------------------------ */
/*  Hooks                                                              */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/*  Enemies react to both players (v0.6)                               */
/* ------------------------------------------------------------------ */
#define ENEMY_TARGET_NEAREST   1        /* 0 = enemies only know player 1 (v0.5 behaviour)                    */
#define ENEMY_RETARGET_FRAMES  90       /* minimum frames between two target changes of one enemy             */
#define ENEMY_RETARGET_PCT_SQ  56       /* switch only if other_dist^2 < cur_dist^2 * 56% (other < ~75% as far) */
#define ENEMY_LOCK_DIST        0x3000   /* within 3.0 units (12-bit fixed) of its target an enemy never switches */
#define ENEMY_ONLY_SLEEPY      0        /* 1 = only retarget entities that the game may put to sleep (flag +0x46 bit 1); 0 = every entity except players/camera */
#define A_LIST_UPDATE          0x00460FC0u  /* void __cdecl UpdateList(void *listHeadPtr, int)                */
#define A_DIST                 0x004E6220u  /* int __cdecl Dist(const int *a, const int *b) (fast, >>12)      */
#define A_DIST_CALL_ACTIVE     0x0046102Eu  /* call Dist inside UpdateList: awake entity vs "the player"      */
#define A_DIST_CALL_WAKE       0x00461187u  /* call Dist inside the sleeping-entity check                      */
#define A_PRE_UPDATE           0x00460ED0u  /* void __thiscall PreUpdate(entity)                              */
#define A_PRE_CALL_1           0x00461111u
#define A_PRE_CALL_2           0x00461126u

typedef int  (__cdecl   *PfnDist)(const int *, const int *);
typedef void (__cdecl   *PfnListUpd)(void *, int);
typedef void (__fastcall *PfnPre)(void *, void *);

static u32 g_realHead = 0;              /* the real list head (P1) while P2 exists                          */
static int g_headSwapped = 0;           /* DAT_006A9038 currently points to P2 for one entity's update      */
static int g_inWorldUpdate = 0;         /* inside the game's UpdateWorld (and P2 exists)                    */
static u32 g_curEnt = 0, g_curTarget = 0;
static int g_enemyHooks = 0;
static unsigned g_enemySwitches = 0;

typedef struct { u32 ent, target; unsigned since, seen; int sleepy; } TgtEntry;
#define TGT_N 1024
static TgtEntry g_tgt[TGT_N];

static double Dist2(u32 a, u32 b)
{
    double dx = (double)(s32)rd32(a + OFF_POS)     - (double)(s32)rd32(b + OFF_POS);
    double dy = (double)(s32)rd32(a + OFF_POS + 4) - (double)(s32)rd32(b + OFF_POS + 4);
    double dz = (double)(s32)rd32(a + OFF_POS + 8) - (double)(s32)rd32(b + OFF_POS + 8);
    return dx * dx + dy * dy + dz * dz;
}

static void RestoreHead(void)
{
    if (g_headSwapped) { wr32(A_PLAYER_HEAD, g_realHead); g_headSwapped = 0; }
}

/* returns the player this entity should regard as "the player", or 0 = not an entity we retarget */
static u32 PickTarget(u32 ent)
{
    u32 p1 = g_realHead, p2 = g_p2, cur, pref;
    TgtEntry *e = NULL;
    double d1, d2, dcur, doth;
    int i, k, p1dead;

    if (!p1 || !p2 || g_p2Down || ent == p1 || ent == p2 || ent == rd32(A_CAMERA)) return 0;
#if ENEMY_ONLY_SLEEPY
    if (!(rd8(ent + 0x46) & 2)) return 0;
#endif
    p1dead = rd16(p1 + OFF_HEALTH) <= 0;
    d1 = Dist2(ent, p1); d2 = Dist2(ent, p2);

    k = (int)((ent >> 4) & (TGT_N - 1));
    for (i = 0; i < 8; i++) {
        TgtEntry *t = &g_tgt[(k + i) & (TGT_N - 1)];
        if (t->ent == ent) { e = t; break; }
        if (!e && (t->ent == 0 || g_frame - t->seen > 300)) e = t;
    }
    if (!e) e = &g_tgt[k];
    if (e->ent != ent || g_frame - e->seen > 300) {         /* new (or long unseen) entity: nearest player */
        e->ent = ent; e->target = (d2 < d1 || p1dead) ? p2 : p1; e->since = g_frame;
    }
    e->seen = g_frame;
    e->sleepy = (rd8(ent + 0x46) & 2) != 0;

    cur  = e->target;
    pref = (d2 < d1 || p1dead) ? p2 : p1;
    if (cur == p1 && p1dead) { e->target = p2; e->since = g_frame; g_enemySwitches++; }
    else if (pref != cur) {
        dcur = (cur == p1) ? d1 : d2;
        doth = (cur == p1) ? d2 : d1;
        if ((g_frame - e->since) >= ENEMY_RETARGET_FRAMES &&
            dcur > (double)ENEMY_LOCK_DIST * (double)ENEMY_LOCK_DIST &&
            doth * 100.0 < dcur * (double)ENEMY_RETARGET_PCT_SQ) {
            e->target = pref; e->since = g_frame; g_enemySwitches++;
        }
    }
    return e->target;
}

/* awake entity: distance to ITS target (decides "too far -> go to sleep") */
static int __cdecl Hook_DistActive(const int *a, const int *b)
{
    PfnDist dist = (PfnDist)(uintptr_t)A_DIST;
    u32 ent = (u32)(uintptr_t)b - 8, t = 0;
    (void)a;
    RestoreHead();                       /* the previous entity may have left the head swapped */
    g_curEnt = 0; g_curTarget = 0;
    if (g_inWorldUpdate) t = PickTarget(ent);
    if (t) { g_curEnt = ent; g_curTarget = t; return dist((const int *)(uintptr_t)(t + OFF_POS), b); }
    return dist((const int *)(uintptr_t)(rd32(A_PLAYER_HEAD) + OFF_POS), b);
}

/* sleeping entity: wake up when EITHER player is near */
static int __cdecl Hook_DistWake(const int *a, const int *b)
{
    PfnDist dist = (PfnDist)(uintptr_t)A_DIST;
    (void)a;
    RestoreHead();
    if (g_inWorldUpdate && g_p2 && !g_p2Down && g_realHead) {
        int d1 = dist((const int *)(uintptr_t)(g_realHead + OFF_POS), b);
        int d2 = dist((const int *)(uintptr_t)(g_p2       + OFF_POS), b);
        return d1 < d2 ? d1 : d2;
    }
    return dist((const int *)(uintptr_t)(rd32(A_PLAYER_HEAD) + OFF_POS), b);
}

/* right before the entity's Update: make its chosen target "the player" */
static void __fastcall Hook_Pre(void *ent, void *edx)
{
    if (g_curTarget && (u32)(uintptr_t)ent == g_curEnt && g_curTarget != g_realHead) {
        wr32(A_PLAYER_HEAD, g_curTarget);
        g_headSwapped = 1;
    }
    ((PfnPre)(uintptr_t)A_PRE_UPDATE)(ent, edx);
}

/* after each list pass the real head must be back */
static void __cdecl Hook_ListUpdate(void *list, int arg2)
{
    ((PfnListUpd)(uintptr_t)A_LIST_UPDATE)(list, arg2);
    RestoreHead();
    g_curEnt = 0; g_curTarget = 0;
}

static int PatchCallTo(u32 site, u32 expectTarget, const void *newTarget)
{
    u32 rel;
    if (rd8(site) != 0xE8 || (u32)(site + 5 + rd32(site + 1)) != expectTarget) return 0;
    rel = (u32)(uintptr_t)newTarget - (site + 5);
    return PatchBytes(site + 1, &rel, 4);
}

static void InstallEnemyHooks(void)
{
    u32 a; int n = 0, ok = 1;
#if ENEMY_TARGET_NEAREST
    ok &= PatchCallTo(A_DIST_CALL_ACTIVE, A_DIST, (const void *)Hook_DistActive);
    ok &= PatchCallTo(A_DIST_CALL_WAKE,   A_DIST, (const void *)Hook_DistWake);
    ok &= PatchCallTo(A_PRE_CALL_1, A_PRE_UPDATE, (const void *)Hook_Pre);
    ok &= PatchCallTo(A_PRE_CALL_2, A_PRE_UPDATE, (const void *)Hook_Pre);
    if (!ok) { Log("enemy targeting: a call site did not match -> enemies keep targeting P1 only (some sites may already be patched, restart the game)"); return; }
    for (a = A_UPDATEWORLD; a < A_RENDER + 0x100; a++)
        if (rd8(a) == 0xE8 && (u32)(a + 5 + rd32(a + 1)) == A_LIST_UPDATE && PatchCallTo(a, A_LIST_UPDATE, (const void *)Hook_ListUpdate)) n++;
    if (n < 8) { Log("enemy targeting: only %d list-update call sites found (expected 9) -> hooks stay installed but the head restore may be incomplete", n); }
    g_enemyHooks = 1;
    Log("enemy targeting installed: nearest player, retarget >=%d frames and <%d%% (squared) distance, lock %d; %d list-update call sites wrapped",
        ENEMY_RETARGET_FRAMES, ENEMY_RETARGET_PCT_SQ, ENEMY_LOCK_DIST, n);
#else
    (void)a; (void)n; (void)ok;
#endif
}

/* ------------------------------------------------------------------ */
/*  Downed state of player 2 (v0.7)                                    */
/* ------------------------------------------------------------------ */
typedef void (__fastcall *PfnThis)(void *self, void *edx);
typedef void (__fastcall *PfnPlayerAnim)(void *self, void *edx, int anim, int first, int last);
typedef void (__fastcall *PfnDie)(void *self, void *edx, int outcome);
typedef int  (__cdecl   *PfnSfx)(int id, int *pos, int flags);

/* run a CPlayer method on P2 inside P2's camera context (the aim code writes the global camera + aim bars) */
static void CallOnP2InCamB(u32 fn)
{
    u32 cam = rd32(A_CAMERA);
    int swap = g_split && g_stBValid && cam && g_bCam == cam && !g_inP2Update;
    if (swap) { CaptureState(&g_stA, cam); ApplyState(&g_stB, cam); }
    ((PfnThis)(uintptr_t)fn)((void *)(uintptr_t)g_p2, NULL);
    if (swap) { CaptureState(&g_stB, cam); ApplyState(&g_stA, cam); }
}

static void P2PlayAnim(u32 anim)
{
    ((PfnPlayerAnim)(uintptr_t)A_PLAYER_ANIM)((void *)(uintptr_t)g_p2, NULL, (int)anim, 0, -1);
}

/* bar empty: let him finish the fall first (no input), see P2UpdateDown */
static void P2EnterDown(void)
{
    g_p2Down = 3;
    g_p2FallMs = 0;
    g_p2ReviveHold = 0;
    g_p1Near = 0; g_reviveKey = 0;
    g_p2LastTick = GetTickCount();
    Log("P2 bar empty at (%d,%d,%d): input off, waiting for the landing (on ground=%d)",
        (s32)rd32(g_p2 + OFF_POS), (s32)rd32(g_p2 + OFF_POS + 4), (s32)rd32(g_p2 + OFF_POS + 8), (rd8(g_p2 + OFF_GROUNDF) & 2) != 0);
}

/* on the ground: the game's death animation + sound, then he lies there (Update skipped, anim holds its last frame) */
static void P2StartLying(void)
{
    u32 p = g_p2;
    if (rd8(p + OFF_AIMFLAG)) CallOnP2InCamB(A_AIM_EXIT);                  /* as Die() does: leave aim mode   */
    if (rd32(p + OFF_WEBLINE)) {                                            /* ... and let go of the web line  */
        ((PfnThis)(uintptr_t)A_WEBLINE_FREE)((void *)(uintptr_t)rd32(p + OFF_WEBLINE), NULL);
        wr32(p + OFF_WEBLINE, 0);
    }
    wr32(p + OFF_VEL, 0); wr32(p + OFF_VEL + 4, 0); wr32(p + OFF_VEL + 8, 0);
    P2PlayAnim(ANIM_DEATH);
    ((PfnSfx)(uintptr_t)A_PLAY_SFX)(SFX_DEATH, (int *)(uintptr_t)(p + OFF_POS), 0);
    g_p2Down = 1;
    g_p2DownMs = g_cfgDownSecs * 1000;
    g_p2ReviveHold = 0;
    g_p1Near = 0; g_reviveKey = 0;
    g_p2LastTick = GetTickCount();
    Log("P2 DOWN at (%d,%d,%d): death animation 0x%X, lying; P1 has %d s to come within %d units and hold Num0 for %d frames",
        (s32)rd32(p + OFF_POS), (s32)rd32(p + OFF_POS + 4), (s32)rd32(p + OFF_POS + 8), ANIM_DEATH,
        g_cfgDownSecs, P2_REVIVE_RADIUS_U, P2_REVIVE_HOLD_FRAMES);
}

static void P2Revive(const char *why, int pct)
{
    int wasLying = P2_FROZEN();
    u32 p = g_p2;
    g_p2Down = 0;
    g_p2ReviveHold = 0;
    g_p1Near = 0; g_reviveKey = 0;
    g_p2DeathFired = 0;
    g_p2VHp = g_p2MaxHp * pct / 100;
    if (g_p2VHp < 1) g_p2VHp = 1;
    g_p2Invuln = P2_REVIVE_INVULN_FRAMES;
    if (p) wr16(p + OFF_HEALTH, (s16)g_p2Pin);
    if (p && wasLying) {
        /* the game's own knock-down -> get-up sequence: state 0x800000 with the get-up flag, anim 0xB1.
         * When 0xB1 ends, CPlayer::Update (0x4B5EFD) clears the flag and returns him to normal play. */
        wr32(p + OFF_VEL, 0); wr32(p + OFF_VEL + 4, 0); wr32(p + OFF_VEL + 8, 0);
        wr32(p + OFF_STATE, STATE_KNOCKDOWN);
        wr8 (p + OFF_GETUPFLAG, 1);
        P2PlayAnim(ANIM_GETUP);
    }
    P2DownTitle(-1);
    Log("P2 revived (%s): bar %d/%d%s", why, g_p2VHp, g_p2MaxHp, wasLying ? ", get-up animation" : "");
}

static void P2DeathOutcome(void)
{
    u32 p1 = rd32(A_PLAYER_HEAD);
    if (g_cfgDeathMode == 1 && p1 && p1 != g_p2 && rd32(A_GAME_STATE) == 0) {
        Log("P2 death counted (%d so far): the game's own death outcome (Die(1): mission failed -> checkpoint)", g_p2Deaths);
        ((PfnDie)(uintptr_t)A_DIE)((void *)(uintptr_t)p1, NULL, 1);
        return;
    }
    Log("P2 death counted (%d so far): P2 gets up next to P1 with full health", g_p2Deaths);
    TeleportPlayer2(NULL);
    P2Revive("respawn after death", 100);
}

static void P2UpdateDown(void)
{
    u32 p1 = rd32(A_PLAYER_HEAD);
    DWORD now = GetTickCount();
    int dt = (int)(now - g_p2LastTick);
    double r = (double)P2_REVIVE_RADIUS_U * 4096.0;     /* Dist2 works on raw coordinates (units << 12) */
    g_p2LastTick = now;
    if (dt < 0 || dt > 100) dt = 33;                    /* pause / long hitch: do not burn the countdown */
    if (g_cutscene && g_p2Down != 3) return;            /* v0.8c: countdown / death outcome wait for the cut-scene */

    if (g_p2Down == 3) {                                /* falling */
        g_p2FallMs += dt;
        if (rd8(g_p2 + OFF_GROUNDF) & 2) P2StartLying();
        else if (g_p2FallMs >= P2_FALL_WAIT_MS) { Log("P2 did not land within %d ms - lying down where he is", P2_FALL_WAIT_MS); P2StartLying(); }
        return;
    }
    if (g_p2Down == 2) {                                /* dead: grey marker, then the outcome */
        g_p2DeadMs -= dt;
        if (g_p2DeadMs <= 0 && !g_p2DeathFired) { g_p2DeathFired = 1; P2DeathOutcome(); }
        return;
    }
    if (g_p2Down != 1) return;

    g_p2DownMs -= dt;
    g_p1Near = (p1 && p1 != g_p2 && rd16(p1 + OFF_HEALTH) > 0 && Dist2(p1, g_p2) <= r * r);
    g_reviveKey = g_p1Near && GameHasFocus() && (KeyDown(P2_REVIVE_KEY1) || KeyDown(P2_REVIVE_KEY2));
    {
        static int lastNear = -1;
        if (g_p1Near != lastNear) {
            Log("P1 %s downed P2 (distance %d units, revive radius %d)", g_p1Near ? "reached" : "is away from",
                p1 ? (int)(sqrt(Dist2(p1, g_p2)) / 4096.0) : -1, P2_REVIVE_RADIUS_U);
            lastNear = g_p1Near;
        }
    }
    if (g_reviveKey) {
        if (++g_p2ReviveHold >= P2_REVIVE_HOLD_FRAMES) { P2Revive("P1 held the revive key", P2_REVIVE_HP_PCT); return; }
    } else g_p2ReviveHold = 0;
    P2DownTitle((g_p2DownMs + 999) / 1000);
    if (g_p2DownMs <= 0) {
        g_p2DownMs = 0;
        g_p2Down = 2;
        g_p2DeadMs = P2_DEATH_DELAY_MS;
        g_p2DeathFired = 0;
        g_p2Deaths++;
        g_p1Near = 0; g_reviveKey = 0; g_p2ReviveHold = 0;
        P2DownTitle(-1);
        Log("P2 countdown expired: death counted (%d). Outcome in %d ms (mode %d)", g_p2Deaths, P2_DEATH_DELAY_MS, g_cfgDeathMode);
    }
}

static void SpawnPlayer2(void);

/* v0.8c: called after the game's UpdateWorld (scripts have run, context A is live, head = P1) */
static void CutsceneUpdate(void)
{
#if CUTSCENE_FIX
    int sig = CutsceneSignals();
    u32 cam = rd32(A_CAMERA), p1 = rd32(A_PLAYER_HEAD);
    if (rd32(A_GAME_STATE) != 0) sig = 0;
    if (sig & CUTSCENE_SIGNALS) {
        if (!g_cutscene) {
            g_cutscene = 1; g_csFrames = 0; g_csSeen = 0;
            Log("cut-scene START (signals 0x%X: bars=%d ignoreInput=%d script=%d camMode=%d): split off, P2 input off",
                sig, (int)rd32(A_LETTERBOX), p1 ? (int)rd32(p1 + OFF_IGNOREINPUT) : -1, p1 ? rd8(p1 + OFF_CUTSCENE) : -1,
                cam ? (int)rd32(cam + CAM_OFF_MODE) : -1);
            if (g_p2 && InPlayerList(g_p2) && !g_p2Down && rd8(g_p2 + OFF_AIMFLAG)) CallOnP2InCamB(A_AIM_EXIT);
        }
        g_csHold = CUTSCENE_HOLD_FRAMES;
    }
    if (!g_cutscene) return;
    g_csSeen |= sig; g_csFrames++;
    if (sig & CUTSCENE_SIGNALS) return;
    if (--g_csHold > 0) return;

    g_cutscene = 0;
    Log("cut-scene END after %u frames (signals seen 0x%X, camera A mode %d)", g_csFrames, g_csSeen,
        cam ? (int)rd32(cam + CAM_OFF_MODE) : -1);
    if (g_p2 && InPlayerList(g_p2) && p1 && p1 != g_p2) {
        int moved = 0;
        double r = (double)CUTSCENE_TP_DIST_U * 4096.0;
        if (!g_p2Down && Dist2(p1, g_p2) > r * r) { TeleportPlayer2("cut-scene end"); moved = 1; }
        if (g_split && cam && (moved || !g_stBValid || g_bCam != cam)) InitCamB(cam, p1, g_p2);  /* fresh camera B from A */
    }
    if (g_spawnPending) { g_spawnPending = 0; Log("cut-scene over: deferred F9"); SpawnPlayer2(); }
#endif
}

static void __cdecl Hook_UpdateWorld(void)
{
    static int kF7, kF8, kF9, kF10, kF11;
    static u32 lastHead = 0;
    static unsigned headFrames = 0, autoTry = 0;

    g_stage = 1;
    if (MenuInput() || rd32(A_GAME_STATE) != 0) {   /* v0.10: co-op menu open -> world paused (v0.12: or the menu just changed the level) */
        Lan_PreWorld(); Lan_PostWorld(1);    /* v0.12: the network keeps running (partner stays visible) */
        return;
    }

    {   /* v0.10: auto-spawn P2 shortly after a level (or checkpoint restart) starts */
        u32 head = rd32(A_PLAYER_HEAD);
        if (head != lastHead && head != g_p2) { lastHead = head; headFrames = 0; g_userRemoved = 0; }
        else headFrames++;
        if (g_cfgAutoSpawn && !g_lanMode && !g_userRemoved && head && !(g_p2 && InPlayerList(g_p2)) && rd32(A_GAME_STATE) == 0 &&
            !g_cutscene && !g_spawnPending && headFrames > 45 && headFrames - autoTry > 120) {
            autoTry = headFrames;
            Log("auto-spawn: level running for %u frames, creating P2", headFrames);
            SpawnPlayer2();
        }
        if (headFrames < autoTry) autoTry = 0;
    }

    if (GameHasFocus()) {
        if (HotKey(VK_F9,  &kF9)) {
            if (g_lanMode) Log("F9: no local player 2 in LAN mode");
            else if (g_cutscene && !(g_p2 && InPlayerList(g_p2))) { g_spawnPending = 1; Log("F9 during a cut-scene: P2 will be spawned when it ends"); }
            else SpawnPlayer2();
        }
        if (HotKey(VK_F10, &kF10)) { if (g_p2 && g_p2Down) P2Revive("F10", P2_REVIVE_HP_PCT); TeleportPlayer2("F10"); }
        if (HotKey(VK_F11, &kF11)) RemovePlayer2();
        if (HotKey(VK_F8,  &kF8))  { g_split = !g_split; g_stBValid = 0; Log("F8: split-screen = %d", g_split); }
        if (HotKey(VK_F7,  &kF7))  { g_layout = (g_layout + 1) & 3; Log("F7: split layout = %d", g_layout); }
    }

    g_stage = 2;
    if (g_p2) {
        if (!InPlayerList(g_p2)) {
            Log("P2 %08X is no longer in the player list (level change?)", g_p2);
            g_stBValid = 0;
            g_p2 = 0; g_p2Down = 0; g_p1Near = 0; g_p2DeathFired = 0; P2DownTitle(-1);
        } else {
            s16 cur;
            UpdatePlayer2Pad();
            /* P2 cannot die for the game: damage is booked on P2's own health bar, the real value is pinned high.
             * An empty bar puts P2 into the DOWNED state (see P2EnterDown). */
            cur = rd16(g_p2 + OFF_HEALTH);
            if (!g_p2Down) {
                if (g_p2Invuln) g_p2Invuln--;
                else if (cur < g_p2Pin && !g_cutscene) g_p2VHp -= (g_p2Pin - cur);
                if ((g_frame % 20) == 0 && g_p2VHp < g_p2MaxHp) g_p2VHp++;
                if (g_p2VHp <= 0) { g_p2VHp = 0; P2EnterDown(); }
            } else {
                P2UpdateDown();
            }
            wr16(g_p2 + OFF_HEALTH, (s16)g_p2Pin);
            g_stage = 3;
            LeashPlayer2();
        }
    }

    g_stage = 5;
    Lan_PreWorld();
    g_realHead = rd32(A_PLAYER_HEAD);
    g_inWorldUpdate = (g_p2 != 0 && g_enemyHooks && !g_p2Down && !g_cutscene);   /* cut-scene: game behaves single-player */
    ((PfnVoid)(uintptr_t)A_UPDATEWORLD)();
    g_inWorldUpdate = 0;
    RestoreHead();
    CutsceneUpdate();
    Lan_PostWorld(0);
    g_stage = 6;

    /* diagnostics: camera modes / aim flags of both players */
    if (g_p2) {
        u32 camp = rd32(A_CAMERA), p1 = rd32(A_PLAYER_HEAD);
        static u32 lastA = 0xFFFFFFFFu; static int lastAim1 = -1, lastAim2 = -1;
        if (camp) {
            u32 m = rd32(camp + CAM_OFF_MODE);
            if (m != lastA) { Log("P1 camera A mode %d -> %d (frame %u)", (int)lastA, (int)m, g_frame); lastA = m; }
        }
        if (p1) {
            int a1 = rd8(p1 + 0x8EA), a2 = rd8(g_p2 + 0x8EA);
            if (a1 != lastAim1) { Log("P1 aim flag (+0x8EA) %d -> %d (frame %u)", lastAim1, a1, g_frame); lastAim1 = a1; }
            if (a2 != lastAim2) { Log("P2 aim flag (+0x8EA) %d -> %d (frame %u)", lastAim2, a2, g_frame); lastAim2 = a2; }
            {
                static unsigned lastAimLog = 0;
                if ((a1 || a2) && g_frame - lastAimLog >= 15) {
                    const u32 *b = g_stBValid ? g_stB.gx : NULL;
                    lastAimLog = g_frame;
                    Log("aim dbg (frame %u): P1[flag=%d pitch=%d pitchSm=%d yaw=%d yawSm=%d prevHead=%d tgtHead=%d] P2[flag=%d pitch=%d pitchSm=%d yaw=%d yawSm=%d prevHead=%d tgtHead=%d]",
                        g_frame,
                        a1, (s16)rd32(kCamX[0]), (s16)rd32(kCamX[2]), (s16)rd32(kCamX[3]), (s16)rd32(kCamX[4]), (s16)rd32(kCamX[1]), (int)rd32(p1 + 0xDF0),
                        a2, b ? (s16)b[0] : 0, b ? (s16)b[2] : 0, b ? (s16)b[3] : 0, b ? (s16)b[4] : 0, b ? (s16)b[1] : 0, (int)rd32(g_p2 + 0xDF0));
                }
            }
        }
    }

    if (g_p2 && (++g_frame % 300) == 0) {
        u32 p1 = rd32(A_PLAYER_HEAD);
        Log("frame %u: P1=%08X (%d,%d,%d) hp=%d | P2=%08X (%d,%d,%d) bar=%d/%d web=0x%X carts=%d split=%d",
            g_frame, p1, (s32)rd32(p1 + OFF_POS), (s32)rd32(p1 + OFF_POS + 4), (s32)rd32(p1 + OFF_POS + 8), rd16(p1 + OFF_HEALTH),
            g_p2, (s32)rd32(g_p2 + OFF_POS), (s32)rd32(g_p2 + OFF_POS + 4), (s32)rd32(g_p2 + OFF_POS + 8), g_p2VHp, g_p2MaxHp,
            (unsigned)rd32(g_p2 + OFF_WEB_FLUID), (int)rd32(g_p2 + OFF_WEB_CARTS), g_split);
        if (g_enemyHooks) {
            int i, n1 = 0, n2 = 0, ns = 0;
            for (i = 0; i < TGT_N; i++)
                if (g_tgt[i].ent && g_frame - g_tgt[i].seen < 60) { if (g_tgt[i].target == g_p2) n2++; else n1++; ns += g_tgt[i].sleepy; }
            Log("             enemies: entities targeting P1=%d P2=%d (of them sleep-capable: %d) | target switches so far=%u", n1, n2, ns, g_enemySwitches);
        }
    }
}

static void __cdecl Hook_DestroyAll(void)
{
    /* Level unload: take P2 out of the list first, so the game's sweep never runs
     * P2's destructor (it would overwrite the carry-over globals with P2's values). */
    g_stage = 7;
    Log("DestroyAll called (level unload), P2=%08X game state=%d", g_p2, (int)rd32(A_GAME_STATE));
    if (g_p2 && InPlayerList(g_p2)) {
        int st = (int)rd32(A_GAME_STATE);
        if (st == 2 || st == 9) Log("checkpoint restart: P2 gets the state he had when the level started");
        else P2SaveCarry("level end");
    }
    g_stBValid = 0;
    g_p2Down = 0; g_p1Near = 0; g_reviveKey = 0; g_p2DeathFired = 0; P2DownTitle(-1);
    g_cutscene = 0; g_csHold = 0; g_spawnPending = 0; g_bSnap = 0;
    RestoreHead();
    memset(g_tgt, 0, sizeof(g_tgt));
    g_enemySwitches = 0;
    Lan_OnDestroyAll();
    if (g_p2) {
        if (InPlayerList(g_p2)) UnlinkPlayer(g_p2);
        Log("DestroyAll: P2 %08X unlinked before level unload", g_p2);
        g_p2 = 0;
    }
    g_origDestroyAll();
    Log("DestroyAll finished");
    g_stage = 8;
}

/* v0.10: health pickups. AddHealth (FUN_004BD750, thiscall, ret 4) only heals when +0xE2 < max (+0xEF0); P2's real
 * health is pinned at 6 x max, so pickups never healed him. For P2 the heal now goes to his virtual health bar. */
#define A_ADDHEALTH        0x004BD750u
#define A_ADDHEALTH_CALL1  0x0046908Fu
#define A_ADDHEALTH_CALL2  0x0046B8A4u
#define A_FRAME_COUNTER    0x006B4CA8u
typedef int (__fastcall *PfnAddHealth)(void *self, void *edx, int amount);
static int __fastcall Hook_AddHealth(void *self, void *edx, int amount)
{
    if (g_p2 && (u32)(uintptr_t)self == g_p2) {
        if (g_p2Down || amount <= 0 || g_p2VHp >= g_p2MaxHp) return 0;   /* full / down: the pickup stays    */
        g_p2VHp += amount;
        if (g_p2VHp > g_p2MaxHp) g_p2VHp = g_p2MaxHp;
        wr32(g_p2 + 0x5E0u, rd32(A_FRAME_COUNTER));                      /* the HUD's white "healed" flash   */
        wr32(g_p2 + 0x5D0u, rd32(g_p2 + 0x5D0u) + 1u);                   /* what the game itself also counts */
        Log("P2 picked up health +%d -> %d/%d", amount, g_p2VHp, g_p2MaxHp);
        return 1;
    }
    return ((PfnAddHealth)(uintptr_t)A_ADDHEALTH)(self, edx, amount);
}

/* ================================================================== */
/*  v0.12: LAN CO-OP, phase 1 (each player runs the game on his own PC) */
/*                                                                      */
/*  The partner is a PUPPET: a second CPlayer (same ctor as P2) whose   */
/*  Update (vtable +8) is replaced by network-driven position, rotation */
/*  and animation; its Hit (vtable +0xC) is swallowed.                  */
/*  World sync - both PCs simulate their own copy of the level, only    */
/*  EVENTS are exchanged (each carries the level tag):                  */
/*    EV_KILL      an enemy died (found on the other PC by node +0xDE)  */
/*    EV_CMDPOINT  a trigger zone fired (CommandPoint 0x4DE8D0, once)   */
/*    EV_LEVEL     level end / restart / mission failed: both PCs load  */
/*  UDP, port 27016 by default; works over LAN and Radmin VPN.          */
/* ================================================================== */
#define A_ENT_BODYUPD    0x00460E80u  /* void thiscall Ent::UpdateBody(desc): hook points + matrix (end of CPlayer::Update) */
#define A_UPDMATRIX      0x00475F50u  /* void cdecl UpdateMatrix(ent): angles +0x14 -> matrix +0x164, pos >> 12 -> +0x178     */
#define A_HOOKPOINTS     0x00454450u  /* void cdecl CalcHookPoints(ent) (needs +0x188, allocated by A_ENT_BODYUPD)               */
#define A_PLAYER_COLL    0x005564E4u  /* collision descriptor CPlayer passes to A_ENT_BODYUPD                                 */
#define A_SETANIM        0x00460C10u  /* void thiscall SetAnim(u16 anim, int first, int last)                                 */
#define A_CYCLEANIM      0x00460D00u  /* void thiscall CycleAnim(u16 anim, s8 dir)                                             */
#define A_PLAYER_HITFN   0x004BD890u  /* CPlayer vtable slot +0xC: int thiscall Hit(msg*)                                      */
#define A_MODEL_SLOTS    0x006B2440u  /* 40 model slots, stride 0x44; +0x1C = animation table (dword 0 = count)               */
#define A_BADDIES        0x0056E990u  /* list head: enemies                                                                    */
#define A_SLEEPERS       0x0060DAB4u  /* list head: sleeping entities                                                          */
#define A_CMDPOINT       0x004DE8D0u  /* CommandPoint_Trigger(key): fires a trigger zone once (+5 = fired)                     */
#define A_LEVEL_NAME     0x0068285Cu  /* level file name, 9 bytes ("l1a2_t")                                                   */
#define A_RESTART_NAME   0x00682865u  /* restart point name, up to 50 chars                                                    */
#define A_DIFFICULTY     0x0054D474u
#define E_FLAGS46        0x46u        /* u16: 0x40 = delete pending                                                            */
#define E_NODE           0xDEu        /* u16: trigger node that spawned the entity (0xFFFF none)                               */
#define E_MODEL          0x1Fu
#define E_ANG            0x14u
#define E_FRAME          0x128u
#define E_ANIMMODE       0x140u
#define E_ANIMDIR        0x141u
#define E_ANIMEND        0x144u
#define E_ANIMSPEED      0x14Cu
static const u32 kCmdPointCalls[3] = { 0x004273F3u, 0x00430042u, 0x00452C19u };

#define LAN_MAGIC        0x4E4C5053u  /* 'SPLN' */
#define LAN_PROTO        2
#define LAN_INTERP_MS    100          /* the partner is shown this far in the past (smooth movement) */
#define LAN_TIMEOUT_MS   5000
#define LAN_MAX_EVENTS   64
#define LAN_EV_PER_DGRAM 12
#define LAN_ABS(x)       ((x) < 0 ? -(x) : (x))

enum { PK_HELLO = 1, PK_STATE = 2, PK_PING = 3 };
enum { EV_KILL = 1, EV_CMDPOINT = 2, EV_LEVEL = 3 };

#pragma pack(push, 1)
typedef struct {            /* 20 bytes, starts every datagram */
    u32 magic; u8 proto, type; u16 seq;
    u16 evAck, sess;        /* evAck: last in-order event applied from the peer; sess: sender's network session id */
    u32 timeMs;
    u8  nEvents, flags; u16 pad1;   /* flags: 1 in a level, 2 loading/menu, 4 co-op menu open, 8 cut-scene */
} LanHdr;
typedef struct {            /* 72 bytes: one player snapshot (PK_STATE body) */
    u32 levelTag;
    s32 pos[3], vel[3];
    s16 ang[3];
    u16 anim; s16 frame, animEnd; u16 animFrac; s8 animDir; u8 animMode;
    s32 animSpeed;
    u32 stateBits;
    s16 hp, hpMax;
    u8  costume, web;
    s16 pad;
    s32 reserved[3];
} LanPlayer;
typedef struct { u16 seq; u8 type, len; u8 data[60]; } LanEvent;     /* 64 bytes */
typedef struct { char nick[16]; char level[12]; u8 proto, difficulty, costume, inLevel; } LanHello;   /* 32 bytes */
#pragma pack(pop)
typedef char lan_hdr_size[(sizeof(LanHdr) == 20) ? 1 : -1];
typedef char lan_player_size[(sizeof(LanPlayer) == 72) ? 1 : -1];
typedef char lan_event_size[(sizeof(LanEvent) == 64) ? 1 : -1];
typedef char lan_hello_size[(sizeof(LanHello) == 32) ? 1 : -1];

typedef int  (__fastcall *PfnLanHit)(void *self, void *edx, void *msg);
typedef void (__fastcall *PfnLanSetAnim)(void *self, void *edx, int anim, int first, int last);
typedef void (__fastcall *PfnLanCycleAnim)(void *self, void *edx, int anim, int dir);
typedef void (__fastcall *PfnLanBody)(void *self, void *edx, u32 desc);
typedef void (__cdecl   *PfnLanEnt)(u32 ent);
typedef u32  (__cdecl   *PfnCmdPoint)(u32 key, u32 arg);

/* ---- network state (g_netCs guards: g_peer, g_rxq, g_outEv, g_txSeq, g_myHello) ---- */
static SOCKET g_sock = INVALID_SOCKET;
static struct sockaddr_in g_peer;
static volatile int   g_peerKnown = 0;
static volatile DWORD g_lastRx = 0, g_lastGameFrame = 0;
static CRITICAL_SECTION g_netCs;
static int  g_netInit = 0, g_lanErr = 0;
static u16  g_txSeq = 0, g_rxSeq = 0;
static LanEvent g_outEv[LAN_MAX_EVENTS]; static int g_outN = 0; static u16 g_outSeq = 0;
static volatile u16 g_inEvSeq = 0;
#define RXQ 64
static struct { int len; DWORD t; u8 buf[1200]; } g_rxq[RXQ]; static int g_rxHead = 0, g_rxTail = 0;   /* t = arrival time */
static LanHello g_myHello, g_peerHello; static volatile int g_peerHelloOk = 0;
static u16  g_mySess = 0;                    /* my session id (new on every Lan_Start)                    */
static u16  g_netPeerSess = 0;               /* net thread: session of the adopted partner                */
static u16  g_peerSess = 0;                  /* game thread: partner session the counters belong to       */

/* ---- game-thread state ---- */
typedef struct { DWORD t; LanPlayer s; } Snap;
static Snap g_snap[32]; static int g_snapN = 0;
static u32  g_puppet = 0, g_puppetSpawnTry = 0; static int g_puppetBroken = 0;
static u32  g_puppetVtbl[256];
static COOP_ALIGN16 u8 g_puppetPad[0x400];
static u32  g_levelTag = 0;
static int  g_lanRemote = 0;                 /* 1 while applying a partner's event (no echo) */
static u16  g_levelGen = 0;
static int  g_lanForced = 0;                 /* the coming level unload was ordered by the partner / travel */
static int  g_forceState = 0; static char g_forceLevel[10];
#define RK_N 1024
static u8   g_rkCnt[RK_N]; static DWORD g_rkTime[RK_N];  /* enemies killed on the partner's order, per node (no echo) */
#define KILL_N 512
static struct { u32 ent, seen; u8 alive, sent; } g_kill[KILL_N];
static u32  g_lanFrames = 0;

static u32 LevelTag(void)
{
    const u8 *s = (const u8 *)(uintptr_t)A_LEVEL_NAME; u32 h = 2166136261u; int i;
    for (i = 0; i < 9 && s[i]; i++) h = (h ^ (u8)(s[i] | 0x20)) * 16777619u;
    return h;
}

static int LevelNameOk(const char *n)        /* "l1a2_t": a level file name, nothing a packet could abuse */
{
    int i;
    if ((n[0] | 0x20) != 'l') return 0;
    for (i = 1; i < 9 && n[i]; i++)
        if (!((n[i] >= '0' && n[i] <= '9') || ((n[i] | 0x20) >= 'a' && (n[i] | 0x20) <= 'z') || n[i] == '_')) return 0;
    return i >= 4;
}

static void LevelNameCopy(char *dst, const char *src, int max)   /* max includes the NUL */
{
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

/* ------------------------------------------------------------------ */
/*  Sockets                                                            */
/* ------------------------------------------------------------------ */
static void Lan_SendRaw(u8 type, const void *body, int blen, u8 flags)
{
    u8 buf[1200]; LanHdr *h = (LanHdr *)buf; int off = sizeof(LanHdr), i;
    struct sockaddr_in to; SOCKET s = g_sock;
    if (s == INVALID_SOCKET || !g_peerKnown) return;
    memset(h, 0, sizeof(*h));
    EnterCriticalSection(&g_netCs);
    to = g_peer;
    h->magic = LAN_MAGIC; h->proto = LAN_PROTO; h->type = type; h->seq = ++g_txSeq;
    h->evAck = g_inEvSeq; h->sess = g_mySess; h->timeMs = GetTickCount(); h->flags = flags;
    if (body && blen > 0 && off + blen <= (int)sizeof(buf)) { memcpy(buf + off, body, (size_t)blen); off += blen; }
    for (i = 0; i < g_outN && i < LAN_EV_PER_DGRAM && off + (int)sizeof(LanEvent) <= (int)sizeof(buf); i++) {
        memcpy(buf + off, &g_outEv[i], sizeof(LanEvent)); off += sizeof(LanEvent); h->nEvents++;
    }
    LeaveCriticalSection(&g_netCs);
    sendto(s, (const char *)buf, off, 0, (struct sockaddr *)&to, sizeof(to));
}

static DWORD WINAPI NetThread(LPVOID unused)
{
    DWORD lastKeep = 0;
    (void)unused;
    for (;;) {
        SOCKET s = g_sock;
        fd_set rs; struct timeval tv; struct sockaddr_in from; int fl, n, r;
        u8 buf[1200];
        if (s == INVALID_SOCKET) { Sleep(50); continue; }
        FD_ZERO(&rs); FD_SET(s, &rs); tv.tv_sec = 0; tv.tv_usec = 20000;
        r = select(0, &rs, NULL, NULL, &tv);
        if (r < 0) { Sleep(20); continue; }                          /* socket closed by Lan_Stop */
        if (r > 0) {
            fl = sizeof(from);
            n = recvfrom(s, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&from, &fl);
            if (n >= (int)sizeof(LanHdr) && ((LanHdr *)buf)->magic == LAN_MAGIC) {
                const LanHdr *h = (const LanHdr *)buf;
                DWORD now = GetTickCount();
                int accept = 1;
                EnterCriticalSection(&g_netCs);
                if (g_lanMode == 1) {          /* host: adopt the first client; another sender only after the old one went silent */
                    int same = g_peerKnown && g_peer.sin_addr.s_addr == from.sin_addr.s_addr && g_peer.sin_port == from.sin_port;
                    if (!g_peerKnown || same || h->sess == g_netPeerSess || now - g_lastRx > LAN_TIMEOUT_MS) {
                        g_peer = from; g_peerKnown = 1; g_netPeerSess = h->sess;
                    } else accept = 0;
                }
                if (accept && h->type == PK_HELLO && n >= (int)(sizeof(LanHdr) + sizeof(LanHello))) {
                    LanHello hi;
                    memcpy(&hi, buf + sizeof(LanHdr), sizeof(hi));
                    hi.nick[15] = 0; hi.level[11] = 0;
                    g_peerHello = hi; g_peerHelloOk = 1;
                }
                if (accept && h->proto == LAN_PROTO) {
                    if (((g_rxHead + 1) % RXQ) == g_rxTail) g_rxTail = (g_rxTail + 1) % RXQ;   /* full: drop the OLDEST */
                    g_rxq[g_rxHead].len = n; g_rxq[g_rxHead].t = now;
                    memcpy(g_rxq[g_rxHead].buf, buf, (size_t)n); g_rxHead = (g_rxHead + 1) % RXQ;
                }
                LeaveCriticalSection(&g_netCs);
                if (accept) g_lastRx = now;
            }
        }
        /* outside a level (menus, loading, Bink movies) the game thread sends nothing: keep the link alive here */
        if (g_peerKnown && GetTickCount() - g_lastGameFrame > 300 && GetTickCount() - lastKeep > 250) {
            LanHello hi;
            lastKeep = GetTickCount();
            EnterCriticalSection(&g_netCs); hi = g_myHello; LeaveCriticalSection(&g_netCs);
            hi.inLevel = 0;
            Lan_SendRaw(PK_HELLO, &hi, sizeof(hi), 2);
        }
    }
    return 0;
}

static void Lan_RemovePuppet(const char *why);
static void Lan_UpdateHello(int inLevel);

static void Lan_Stop(void)
{
    SOCKET s = g_sock;
    if (g_puppet) Lan_RemovePuppet("network stopped");
    g_lanActive = 0;
    g_sock = INVALID_SOCKET;
    if (s != INVALID_SOCKET) { closesocket(s); Log("lan: network stopped"); }
    g_peerKnown = 0; g_peerHelloOk = 0; g_snapN = 0;
}

static void Lan_Start(void)
{
    WSADATA wd; struct sockaddr_in me; u_long nb = 1; BOOL off = FALSE; DWORD got = 0; SOCKET s;
    Lan_Stop();
    g_lanErr = 0;
    if (!g_lanMode) return;
    if (!g_netInit) {
        if (WSAStartup(MAKEWORD(2, 2), &wd)) { Log("lan: WSAStartup failed"); g_lanErr = 1; return; }
        InitializeCriticalSection(&g_netCs);
        g_netInit = 1;
        CreateThread(NULL, 0, NetThread, NULL, 0, NULL);
    }
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { Log("lan: socket() failed %d", WSAGetLastError()); g_lanErr = 1; return; }
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), NULL, 0, &got, NULL, NULL);   /* ICMP "port unreachable" must not break recv */
    memset(&me, 0, sizeof(me)); me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port = htons((u_short)(g_lanMode == 1 ? g_lanPort : 0));                     /* client: any free port */
    if (bind(s, (struct sockaddr *)&me, sizeof(me))) {
        Log("lan: bind to UDP %d failed (%d) - port busy? another copy of the game running?", g_lanPort, WSAGetLastError());
        closesocket(s); g_lanErr = 2; return;
    }
    ioctlsocket(s, FIONBIO, &nb);
    EnterCriticalSection(&g_netCs);
    memset(&g_peer, 0, sizeof(g_peer)); g_peerKnown = 0;
    if (g_lanMode == 2) {
        g_peer.sin_family = AF_INET; g_peer.sin_port = htons((u_short)g_lanPort); g_peer.sin_addr.s_addr = inet_addr(g_lanIp);
        if (g_peer.sin_addr.s_addr == INADDR_NONE || g_peer.sin_addr.s_addr == 0) { Log("lan: bad host IP '%s'", g_lanIp); g_lanErr = 3; }
        else g_peerKnown = 1;
    }
    g_outN = 0; g_outSeq = 0; g_inEvSeq = 0; g_rxHead = g_rxTail = 0; g_rxSeq = 0; g_txSeq = 0;
    g_mySess = (u16)((GetTickCount() ^ GetCurrentProcessId() ^ (GetTickCount() >> 16)) | 1u);
    g_netPeerSess = 0; g_peerSess = 0;
    LeaveCriticalSection(&g_netCs);
    g_peerHelloOk = 0; g_lastRx = 0; g_levelGen = 0; g_forceState = 0; g_lanForced = 0;
    g_sock = s;
    g_lanActive = 1;
    Lan_UpdateHello(0);
    if (g_p2) RemovePlayer2();                                       /* LAN mode: no local player 2 / split-screen */
    g_spawnPending = 0;
    Log("lan: %s on UDP %d, partner %s (nick %s)", g_lanMode == 1 ? "HOST" : "JOIN", g_lanPort,
        g_lanMode == 2 ? g_lanIp : "= first player who connects", g_nick);
}

/* ------------------------------------------------------------------ */
/*  Reliable events (resent in every datagram until acknowledged)      */
/* ------------------------------------------------------------------ */
static void Lan_QueueEvent(u8 type, const void *data, int len)
{
    LanEvent *e;
    if (!g_lanActive) return;
    EnterCriticalSection(&g_netCs);
    if (g_outN >= LAN_MAX_EVENTS) { LeaveCriticalSection(&g_netCs); Log("lan: event queue full, dropping type %d", type); return; }
    e = &g_outEv[g_outN++]; e->seq = ++g_outSeq; e->type = type; e->len = (u8)(len > 60 ? 60 : len);
    memset(e->data, 0, sizeof(e->data)); if (data) memcpy(e->data, data, e->len);
    LeaveCriticalSection(&g_netCs);
}

static void Lan_AckEvents(u16 ack)
{
    int i, k = 0;
    EnterCriticalSection(&g_netCs);
    for (i = 0; i < g_outN; i++) if ((s16)(g_outEv[i].seq - ack) > 0) g_outEv[k++] = g_outEv[i];
    g_outN = k;
    LeaveCriticalSection(&g_netCs);
}

/* ------------------------------------------------------------------ */
/*  World events                                                       */
/* ------------------------------------------------------------------ */
/* the partner is connected and playing the same level (world events make sense only then) */
static int Lan_PeerHere(void)
{
    return g_peerHelloOk && g_lastRx && GetTickCount() - g_lastRx < LAN_TIMEOUT_MS &&
           g_snapN && g_snap[g_snapN - 1].s.levelTag == g_levelTag;
}

static int KillSlot(u32 e)                   /* open addressing; slots not seen for 3 scans are reused */
{
    int k = (int)((e >> 4) & (KILL_N - 1)), i, freeK = -1;
    for (i = 0; i < 16; i++, k = (k + 1) & (KILL_N - 1)) {
        if (g_kill[k].ent == e) return k;
        if (freeK < 0 && (!g_kill[k].ent || g_lanFrames - g_kill[k].seen > 2)) freeK = k;
    }
    if (freeK >= 0) { g_kill[freeK].ent = e; g_kill[freeK].alive = 0; g_kill[freeK].sent = 0; }
    return freeK;
}

static void Lan_ScanKills(void)              /* an enemy died here -> EV_KILL(level, node) once */
{
    u32 lists[2] = { A_BADDIES, A_SLEEPERS }; int l, guard, here = Lan_PeerHere(); DWORD now = GetTickCount();
    for (l = 0; l < 2; l++) {
        u32 e = rd32(lists[l]); guard = 0;
        while (e && guard++ < 2000) {
            int k = KillSlot(e);
            if (k >= 0) {
                u16 node = (u16)rd16(e + E_NODE);
                s16 hp = rd16(e + OFF_HEALTH);
                g_kill[k].seen = g_lanFrames;
                if (hp > 0 && !(rd16(e + E_FLAGS46) & 0x40)) { g_kill[k].alive = 1; g_kill[k].sent = 0; }   /* (a new entity may reuse the address) */
                else if (hp <= 0 && g_kill[k].alive && !g_kill[k].sent) {   /* only a real death; despawns / scripted removals are not kills */
                    u32 r = node & (RK_N - 1);
                    g_kill[k].sent = 1;
                    if (node != 0xFFFF) {
                        if (g_rkCnt[r] && now - g_rkTime[r] < 5000) g_rkCnt[r]--;   /* we killed it on the partner's order */
                        else if (here) { u32 d[2]; g_rkCnt[r] = 0; d[0] = g_levelTag; d[1] = node; Lan_QueueEvent(EV_KILL, d, 8); }
                    }
                }
            }
            e = rd32(e + OFF_NEXT);
        }
    }
}

static void Lan_ApplyKill(u16 node)          /* lethal hit through the enemy's own Hit (vtable +0xC): its own death + trigger pulse */
{
    u32 lists[2] = { A_BADDIES, A_SLEEPERS }; int l, guard;
    for (l = 0; l < 2; l++) {
        u32 e = rd32(lists[l]); guard = 0;
        while (e && guard++ < 2000) {
            if ((u16)rd16(e + E_NODE) == node && rd16(e + OFF_HEALTH) > 0 && !(rd16(e + E_FLAGS46) & 0x40)) {
                u8 msg[0x1C]; u32 vt = rd32(e), fn = vt ? rd32(vt + 0xC) : 0;
                if (fn < 0x00401000u || fn > 0x00530000u) { Log("lan: enemy %08X (node %d) has no usable Hit", e, node); return; }
                memset(msg, 0, sizeof(msg));
                msg[0] = 4; *(u16 *)(msg + 8) = (u16)(rd16(e + OFF_HEALTH) + 50);
                g_rkCnt[node & (RK_N - 1)]++; g_rkTime[node & (RK_N - 1)] = GetTickCount();
                g_lanRemote = 1;
                ((PfnLanHit)(uintptr_t)fn)((void *)(uintptr_t)e, NULL, msg);
                g_lanRemote = 0;
                if (rd16(e + OFF_HEALTH) > 0 && !(rd16(e + E_FLAGS46) & 0x40)) {
                    if (g_rkCnt[node & (RK_N - 1)]) g_rkCnt[node & (RK_N - 1)]--;
                    Log("lan: partner killed enemy node %d, but this enemy class (vtable %08X) ignores hits - it stays alive here", node, vt);
                } else Log("lan: partner killed enemy node %d -> killed here too (%08X)", node, e);
                return;
            }
            e = rd32(e + OFF_NEXT);
        }
    }
}

/* trigger zones: the 3 call sites of CommandPoint_Trigger. A zone fires only once, so a double call is harmless. */
static u32 __cdecl Hook_CmdPoint(u32 key, u32 arg)
{
    u32 r = ((PfnCmdPoint)(uintptr_t)A_CMDPOINT)(key, arg);
    if (r && !g_lanRemote && g_lanActive && g_lanSyncWorld && Lan_PeerHere()) {
        u32 d[3]; d[0] = g_levelTag; d[1] = key; d[2] = arg;
        Lan_QueueEvent(EV_CMDPOINT, d, 12);
        Log("lan: trigger zone %08X fired here -> sent to the partner", key);
    }
    return r;
}

static void Lan_ApplyEvent(const LanEvent *e)
{
    const u32 *d = (const u32 *)e->data;
    switch (e->type) {
    case EV_KILL:
        if (g_lanSyncWorld && d[0] == g_levelTag) Lan_ApplyKill((u16)d[1]);
        break;
    case EV_CMDPOINT:
        if (g_lanSyncWorld && d[0] == g_levelTag && rd32(A_GAME_STATE) == 0) {
            u32 r;
            g_lanRemote = 1; r = ((PfnCmdPoint)(uintptr_t)A_CMDPOINT)(d[1], d[2]); g_lanRemote = 0;
            Log("lan: partner fired trigger zone %08X -> %s here", d[1], r ? "fired" : "already fired");
        }
        break;
    case EV_LEVEL: {
        u16 gen = (u16)(e->data[1] | (e->data[2] << 8));
        u32 tag; memcpy(&tag, e->data + 12, 4);
        if (!g_lanSyncLevel) break;
        if (tag != g_levelTag) { Log("lan: partner level event (state %d) was for another level - ignored", e->data[0]); break; }
        if ((s16)(gen - g_levelGen) < 0 || (gen == g_levelGen && g_lanMode == 1)) {   /* tie: the host's order wins */
            Log("lan: partner level event gen %d ignored (mine %d)", gen, g_levelGen); break;
        }
        if (e->data[0] != 1 && e->data[0] != 2 && e->data[0] != 3 && e->data[0] != 8 && e->data[0] != 9) break;
        LevelNameCopy(g_forceLevel, (const char *)e->data + 3, 10);
        if ((e->data[0] == 3 || e->data[0] == 9) && !LevelNameOk(g_forceLevel)) { Log("lan: bad level name in a partner event - ignored"); break; }
        g_levelGen = gen;
        g_forceState = e->data[0];
        Log("lan: partner -> game state %d, level '%s' (gen %d)", g_forceState, g_forceLevel, gen);
        break; }
    }
}

static void Lan_HandleDatagram(const u8 *buf, int len, DWORD arrived)
{
    const LanHdr *h = (const LanHdr *)buf; int off = sizeof(LanHdr), i;
    if (h->sess != g_peerSess) {            /* the partner (re)started his network: new numbering on both sides */
        Log("lan: partner session %04X (was %04X) - counters reset", h->sess, g_peerSess);
        g_peerSess = h->sess; g_inEvSeq = 0; g_rxSeq = (u16)(h->seq - 1); g_snapN = 0; g_levelGen = 0;
        EnterCriticalSection(&g_netCs); g_outN = 0; g_outSeq = 0; LeaveCriticalSection(&g_netCs);
    }
    Lan_AckEvents(h->evAck);
    if (h->type == PK_STATE && off + (int)sizeof(LanPlayer) <= len) {
        if ((s16)(h->seq - g_rxSeq) > 0 || g_snapN == 0) {
            g_rxSeq = h->seq;
            if (g_snapN == 32) { memmove(g_snap, g_snap + 1, sizeof(Snap) * 31); g_snapN = 31; }
            g_snap[g_snapN].t = arrived; memcpy(&g_snap[g_snapN].s, buf + off, sizeof(LanPlayer)); g_snapN++;
        }
        off += sizeof(LanPlayer);
    } else if (h->type == PK_HELLO) {
        off += sizeof(LanHello);
        if (h->flags & 2) {                 /* partner is in a menu / loading: his Spider-Man leaves this level */
            g_snapN = 0;
            if (g_puppet) Lan_RemovePuppet("partner left the level");
        }
    }
    for (i = 0; i < h->nEvents && off + (int)sizeof(LanEvent) <= len; i++, off += sizeof(LanEvent)) {
        LanEvent ev;
        memcpy(&ev, buf + off, sizeof(ev));
        if ((s16)(ev.seq - g_inEvSeq) <= 0) continue;             /* already applied */
        if (ev.seq != (u16)(g_inEvSeq + 1)) break;               /* gap: wait for the resend */
        g_inEvSeq = ev.seq;
        Lan_ApplyEvent(&ev);
    }
}

static void Lan_Poll(void)
{
    for (;;) {
        int n; DWORD t; u8 buf[1200];
        EnterCriticalSection(&g_netCs);
        if (g_rxTail == g_rxHead) { LeaveCriticalSection(&g_netCs); break; }
        n = g_rxq[g_rxTail].len; t = g_rxq[g_rxTail].t; memcpy(buf, g_rxq[g_rxTail].buf, (size_t)n); g_rxTail = (g_rxTail + 1) % RXQ;
        LeaveCriticalSection(&g_netCs);
        Lan_HandleDatagram(buf, n, t);
    }
}

/* ------------------------------------------------------------------ */
/*  Puppet (the partner's Spider-Man on this PC)                       */
/* ------------------------------------------------------------------ */
static int __fastcall Puppet_Hit(void *self, void *edx, void *msg)
{
    (void)self; (void)edx; (void)msg;            /* enemies here cannot hurt the partner: his own PC decides */
    return 0;
}

static s16 LerpAng(s16 a, s16 b, int t256) { int d = ((b - a + 2048) & 4095) - 2048; return (s16)((a + d * t256 / 256) & 4095); }

static void Puppet_Body(u32 p)                  /* what CPlayer::Update does at its end: hook points + render matrix */
{
    ((PfnLanBody)(uintptr_t)A_ENT_BODYUPD)((void *)(uintptr_t)p, NULL, A_PLAYER_COLL);
    if (!(rd8(p + 4) & 4)) {
        ((PfnLanEnt)(uintptr_t)A_UPDMATRIX)(p);
        if (rd32(p + 0x188)) ((PfnLanEnt)(uintptr_t)A_HOOKPOINTS)(p);
    }
}

/* runs inside UpdateList(players) after PreUpdate + AnimTick, instead of CPlayer::Update */
static void __fastcall Puppet_Update(void *self, void *edx)
{
    u32 p = (u32)(uintptr_t)self, p1 = rd32(A_PLAYER_HEAD);
    DWORD rt = GetTickCount() - LAN_INTERP_MS; int i, ia, ib, t;
    const LanPlayer *a, *b;
    (void)edx;
    if (p != g_puppet) return;
    wr16(p + OFF_HEALTH, 30000);
    if (!g_snapN || g_snap[g_snapN - 1].s.levelTag != g_levelTag) {     /* partner elsewhere: park under the floor */
        if (p1) for (i = 0; i < 3; i++) wr32(p + OFF_POS + 4u * (u32)i, rd32(p1 + OFF_POS + 4u * (u32)i) + (i == 1 ? (2000u << 12) : 0u));
        Puppet_Body(p);
        return;
    }
    for (ib = g_snapN - 1; ib > 0 && g_snap[ib - 1].t > rt; ib--) ;
    ia = ib > 0 ? ib - 1 : ib;
    a = &g_snap[ia].s; b = &g_snap[ib].s;
    if (ia != ib && g_snap[ib].t > g_snap[ia].t && rt > g_snap[ia].t) t = (int)((rt - g_snap[ia].t) * 256u / (g_snap[ib].t - g_snap[ia].t));
    else t = 256;
    if (t > 256) t = 256;
    if (a->levelTag != b->levelTag) a = b;
    for (i = 0; i < 3; i++) {
        wr32(p + OFF_POS + 4u * (u32)i, (u32)(a->pos[i] + (s32)(((long long)(b->pos[i] - a->pos[i]) * t) >> 8)));
        wr32(p + OFF_VEL + 4u * (u32)i, (u32)b->vel[i]);
        wr16(p + E_ANG + 2u * (u32)i, LerpAng(a->ang[i], b->ang[i], t));
    }
    if ((u16)rd16(p + OFF_ANIM) != b->anim) {
        u32 slot = rd8(p + E_MODEL), tab = slot < 40 ? rd32(A_MODEL_SLOTS + slot * 0x44u + 0x1C) : 0;
        if (tab && b->anim < rd32(tab)) {
            if (b->animMode == 1) ((PfnLanCycleAnim)(uintptr_t)A_CYCLEANIM)(self, NULL, b->anim, b->animDir);
            else ((PfnLanSetAnim)(uintptr_t)A_SETANIM)(self, NULL, b->anim, b->frame, b->animEnd);
        }
    }
    if (LAN_ABS(rd16(p + E_FRAME) - b->frame) > 3) wr16(p + E_FRAME, b->frame);
    wr32(p + E_ANIMSPEED, (u32)b->animSpeed);
    Puppet_Body(p);
}

static void Lan_RemovePuppet(const char *why)
{
    if (!g_puppet) return;
    if (InPlayerList(g_puppet)) UnlinkPlayer(g_puppet);   /* never destroyed: the destructor writes the carry-over globals */
    Log("lan: partner's Spider-Man %08X removed (%s)", g_puppet, why);
    g_puppet = 0;
}

static void Lan_SpawnPuppet(void)
{
    u32 p1 = rd32(A_PLAYER_HEAD), mem, p, vt, old, tail; int guard = 0, i;
    const LanPlayer *s = &g_snap[g_snapN - 1].s;
    if (!p1 || rd32(A_GAME_STATE) != 0 || g_puppet || g_cutscene || !g_snapN || s->levelTag != g_levelTag) return;
    SnapshotRegions();
    mem = (u32)(uintptr_t)((PfnAlloc)(uintptr_t)A_ALLOC)((int)PLAYER_SIZE);
    p = mem ? (u32)(uintptr_t)((PfnCtor)(uintptr_t)A_PLAYER_CTOR)((void *)(uintptr_t)mem) : 0;
    RestoreRegions();
    if (!p) { Log("lan: puppet ctor failed"); return; }
    if (rd32(A_PLAYER_HEAD) == p && (old = rd32(p + OFF_NEXT)) != 0) {   /* the ctor put it at the head: local player back in front */
        wr32(A_PLAYER_HEAD, old); wr32(old + OFF_PREV, 0); wr32(p + OFF_NEXT, 0);
        tail = old; while (rd32(tail + OFF_NEXT) && guard++ < 16) tail = rd32(tail + OFF_NEXT);
        wr32(tail + OFF_NEXT, p); wr32(p + OFF_PREV, tail);
    }
    memset(g_puppetPad, 0, sizeof(g_puppetPad));
    wr32((u32)(uintptr_t)g_puppetPad + 0x16C, 0x41);
    wr32(p + OFF_PAD, (u32)(uintptr_t)g_puppetPad);
    wr8(p + OFF_INITDONE, 1);
    wr16(p + OFF_HEALTH, 30000);
    vt = rd32(p);
    if (!vt || IsBadReadPtr((const void *)(uintptr_t)vt, sizeof(g_puppetVtbl)) || rd32(vt + 8) != A_PLAYER_VTBL_UPDATE) {
        Log("lan: puppet vtable %08X unexpected - puppet not created (no retry on this level)", vt);
        if (InPlayerList(p)) UnlinkPlayer(p);
        g_puppetBroken = 1;
        return;
    }
    memcpy(g_puppetVtbl, (const void *)(uintptr_t)vt, sizeof(g_puppetVtbl));
    g_puppetVtbl[2] = (u32)(uintptr_t)Puppet_Update;
    if (g_puppetVtbl[3] == A_PLAYER_HITFN) g_puppetVtbl[3] = (u32)(uintptr_t)Puppet_Hit;
    else Log("lan: puppet Hit slot = %08X (expected %08X), left as is", g_puppetVtbl[3], A_PLAYER_HITFN);
    wr32(p, (u32)(uintptr_t)g_puppetVtbl);
    for (i = 0; i < 3; i++) wr32(p + OFF_POS + 4u * (u32)i, (u32)s->pos[i]);
    g_puppet = p;
    Log("lan: partner's Spider-Man spawned %08X at (%d,%d,%d), flags +4=%02X +5=%02X", p, s->pos[0], s->pos[1], s->pos[2], rd8(p + 4), rd8(p + 5));
}

static void Lan_CaptureLocal(LanPlayer *s)
{
    u32 p = rd32(A_PLAYER_HEAD); int i;
    memset(s, 0, sizeof(*s));
    if (!p) return;
    s->levelTag = g_levelTag;
    for (i = 0; i < 3; i++) {
        s->pos[i] = (s32)rd32(p + OFF_POS + 4u * (u32)i); s->vel[i] = (s32)rd32(p + OFF_VEL + 4u * (u32)i);
        s->ang[i] = rd16(p + E_ANG + 2u * (u32)i);
    }
    s->anim = (u16)rd16(p + OFF_ANIM); s->frame = rd16(p + E_FRAME); s->animEnd = rd16(p + E_ANIMEND);
    s->animFrac = (u16)rd16(p + 0x146); s->animDir = (s8)rd8(p + E_ANIMDIR); s->animMode = rd8(p + E_ANIMMODE);
    s->animSpeed = (s32)rd32(p + E_ANIMSPEED); s->stateBits = rd32(p + OFF_STATE);
    s->hp = rd16(p + OFF_HEALTH); s->hpMax = (s16)rd32(p + OFF_MAXHP);
    s->web = rd32(p + OFF_WEBLINE) != 0;
}

/* ------------------------------------------------------------------ */
/*  Entry points from the mod's hooks                                  */
/* ------------------------------------------------------------------ */
static void Lan_UpdateHello(int inLevel)
{
    LanHello h; memset(&h, 0, sizeof(h));
    LevelNameCopy(h.nick, g_nick, 16);
    LevelNameCopy(h.level, (const char *)(uintptr_t)A_LEVEL_NAME, 10);
    h.proto = LAN_PROTO; h.difficulty = (u8)rd32(A_DIFFICULTY); h.inLevel = (u8)inLevel;
    EnterCriticalSection(&g_netCs); g_myHello = h; LeaveCriticalSection(&g_netCs);
}

static void Lan_PreWorld(void)                /* before UpdateWorld (also while the co-op menu pauses the world) */
{
    if (!g_lanActive) return;
    g_lastGameFrame = GetTickCount();
    g_levelTag = LevelTag();
    Lan_Poll();
    if (g_puppet && !InPlayerList(g_puppet)) { Log("lan: puppet %08X left the player list", g_puppet); g_puppet = 0; }
    if (g_puppet && GetTickCount() - g_lastRx > LAN_TIMEOUT_MS) { Lan_RemovePuppet("partner silent for 5 s"); g_snapN = 0; }
    if (!g_puppet && !g_puppetBroken && g_snapN && GetTickCount() - g_lastRx < LAN_TIMEOUT_MS && g_lanFrames - g_puppetSpawnTry > 60) {
        g_puppetSpawnTry = g_lanFrames;
        Lan_SpawnPuppet();
    }
}

static void Lan_PostWorld(int menuOpen)       /* after UpdateWorld, BEFORE GameLoop looks at the game state */
{
    LanPlayer s;
    if (!g_lanActive) return;
    g_lanFrames++;
    if (g_lanSyncWorld && !menuOpen) Lan_ScanKills();
    if (g_forceState && (rd32(A_GAME_STATE) == 0 || g_lanMode == 2)) {   /* partner finished / restarted / failed; same frame: the client yields */
        Log("lan: following the partner: state %d level '%s' (my state was %d)", g_forceState, g_forceLevel, (int)rd32(A_GAME_STATE));
        if (g_forceState != 2 && g_forceLevel[0]) memcpy((void *)(uintptr_t)A_LEVEL_NAME, g_forceLevel, 9);
        g_lanForced = 1;
        wr32(A_GAME_STATE, (u32)g_forceState);
        g_forceState = 0;
        g_menuOpen = 0;
    }
    if ((g_lanFrames % 60) == 1) { Lan_UpdateHello(1); { LanHello h; EnterCriticalSection(&g_netCs); h = g_myHello; LeaveCriticalSection(&g_netCs);
                                                        Lan_SendRaw(PK_HELLO, &h, sizeof(h), 1); } }
    Lan_CaptureLocal(&s);
    Lan_SendRaw(PK_STATE, &s, sizeof(s), (u8)(1 | (menuOpen ? 4 : 0) | (g_cutscene ? 8 : 0)));
}

static void Lan_OnDestroyAll(void)            /* level unload: tell the partner, drop the puppet */
{
    u32 st = rd32(A_GAME_STATE);
    if (g_lanActive) {
        if (g_lanForced) { g_lanForced = 0; Log("lan: level unload ordered by the partner (state %d)", (int)st); }
        else if (g_lanSyncLevel && Lan_PeerHere() && (st == 1 || st == 2 || st == 3 || st == 8 || st == 9)) {
            u8 d[16]; memset(d, 0, sizeof(d));
            g_levelGen++;
            d[0] = (u8)st; d[1] = (u8)g_levelGen; d[2] = (u8)(g_levelGen >> 8);
            LevelNameCopy((char *)d + 3, (const char *)(uintptr_t)A_LEVEL_NAME, 10);   /* where to go (state 3/9)     */
            memcpy(d + 12, &g_levelTag, 4);                                           /* the level being left        */
            Lan_QueueEvent(EV_LEVEL, d, 16);
            Lan_SendRaw(PK_PING, NULL, 0, 2); Lan_SendRaw(PK_PING, NULL, 0, 2);   /* the next send comes only after the load */
            Log("lan: level unload (state %d, level '%s') -> partner follows (gen %d)", (int)st, (const char *)d + 3, g_levelGen);
        }
        g_forceState = 0;                     /* a pending order is superseded by this unload */
    }
    if (g_puppet) Lan_RemovePuppet("level unload");
    g_snapN = 0; g_puppetBroken = 0; memset(g_kill, 0, sizeof(g_kill)); memset(g_rkCnt, 0, sizeof(g_rkCnt));
    g_lastGameFrame = 0;
}

static int AsciiIcmp(const char *a, const char *b)
{
    for (;; a++, b++) { int x = (u8)*a | 0x20, y = (u8)*b | 0x20; if (!*a || !*b) return *a != *b; if (x != y) return 1; }
}

/* menu: load the level the partner is playing (uses the game's "next level" path, state 3, so the level counts as reached) */
static void Lan_TravelToPartner(void)
{
    char lvl[12];
    if (!g_peerHelloOk || rd32(A_GAME_STATE) != 0) return;
    EnterCriticalSection(&g_netCs); LevelNameCopy(lvl, g_peerHello.level, 10); LeaveCriticalSection(&g_netCs);
    if (!LevelNameOk(lvl)) { Log("lan: partner level '%s' cannot be loaded", lvl); return; }
    if (!g_peerHello.inLevel) { Log("lan: partner is not in a level"); return; }
    { char mine[12]; LevelNameCopy(mine, (const char *)(uintptr_t)A_LEVEL_NAME, 10);
      if (!AsciiIcmp(mine, lvl)) { Log("lan: already on the partner's level '%s'", lvl); return; } }
    memcpy((void *)(uintptr_t)A_LEVEL_NAME, lvl, 9);
    wr8(A_RESTART_NAME, 0);
    g_lanForced = 1;
    wr32(A_GAME_STATE, 3);
    g_menuOpen = 0;
    Log("lan: travelling to the partner's level '%s'", lvl);
}

static void Lan_StatusText(char *out)
{
    DWORD now = GetTickCount();
    if (!g_lanMode)                         { strcpy(out, "OFF"); return; }
    if (g_lanErr == 2)                      { wsprintfA(out, "PORT %d BUSY", g_lanPort); return; }
    if (g_lanErr == 3)                      { strcpy(out, "BAD HOST IP"); return; }
    if (g_lanErr || !g_lanActive)           { strcpy(out, "NETWORK ERROR"); return; }
    if (!g_lastRx || now - g_lastRx > LAN_TIMEOUT_MS) { strcpy(out, g_lanMode == 1 ? "WAITING FOR PLAYER" : "CONNECTING..."); return; }
    if (g_peerHelloOk && g_netInit) {
        char lvl[12]; int i; LanHello ph;
        EnterCriticalSection(&g_netCs); ph = g_peerHello; LeaveCriticalSection(&g_netCs);
        ph.nick[15] = 0;
        if (ph.proto != LAN_PROTO) { strcpy(out, "OTHER MOD VERSION"); return; }
        LevelNameCopy(lvl, ph.level, 10);
        for (i = 0; lvl[i]; i++) { if (lvl[i] == '_') lvl[i] = '-'; else if (lvl[i] >= 'a' && lvl[i] <= 'z') lvl[i] -= 32; }
        if (!ph.inLevel)                                  wsprintfA(out, "%s IN MENU", ph.nick);
        else if (g_snapN && g_snap[g_snapN - 1].s.levelTag != g_levelTag) wsprintfA(out, "%s ON %s", ph.nick, lvl);
        else                                              wsprintfA(out, "OK: %s", ph.nick);
        return;
    }
    strcpy(out, "CONNECTED");
}

static void Lan_Install(void)
{
    int i, n = 0;
    for (i = 0; i < 3; i++) n += PatchCallTo(kCmdPointCalls[i], A_CMDPOINT, (const void *)Hook_CmdPoint);
    Log("lan: %d of 3 trigger-zone call sites hooked", n);
    if (g_lanMode) Lan_Start();
}

static void InstallHooks(void)
{
    static int done = 0;
    static const u8 sigCall[5]    = { 0xE8, 0x70, 0xF9, 0xFF, 0xFF };
    static const u8 sigDestroy[8] = { 0x8B, 0x0D, 0x38, 0x90, 0x6A, 0x00, 0x53, 0x85 };
    static const u8 sigAlloc[8]   = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x6A, 0x01, 0x6A };
    static const u8 sigCtor[8]    = { 0x6A, 0xFF, 0x68, 0x68, 0x98, 0x53, 0x00, 0x64 };
    static const u8 sigPad[8]     = { 0x8B, 0x44, 0x24, 0x04, 0x53, 0x33, 0xD2, 0x56 };
    u8 *tramp;
    u8 jmp[6];
    u32 rel, a, renderCall = 0;
    int found = 0;

    if (done) return;
    done = 1;

    if (!SigMatch(A_UPDATEWORLD_CALL, sigCall, 5) || !SigMatch(A_DESTROYALL, sigDestroy, 8) ||
        !SigMatch(A_ALLOC, sigAlloc, 8) || !SigMatch(A_PLAYER_CTOR, sigCtor, 8) ||
        !SigMatch(A_PAD_ENTRY, sigPad, 8)) {
        Log("SIGNATURE MISMATCH: this is not SpideyPC.exe v1.3 (build 2001-08-15). Hooks NOT installed.");
        return;
    }

    /* trampoline for DestroyAll: original first instruction (6 bytes) + jmp back */
    tramp = (u8 *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) { Log("VirtualAlloc for trampoline failed"); return; }
    memcpy(tramp, (void *)(uintptr_t)A_DESTROYALL, 6);
    tramp[6] = 0xE9;
    rel = (A_DESTROYALL + 6) - ((u32)(uintptr_t)tramp + 11);
    memcpy(tramp + 7, &rel, 4);
    g_origDestroyAll = (PfnVoid)(uintptr_t)tramp;

    jmp[0] = 0xE9;
    rel = (u32)(uintptr_t)Hook_DestroyAll - (A_DESTROYALL + 5);
    memcpy(jmp + 1, &rel, 4);
    jmp[5] = 0x90;
    if (!PatchBytes(A_DESTROYALL, jmp, 6)) { Log("patch DestroyAll failed"); return; }

    /* redirect the single call to UpdateWorld in GameLoop */
    rel = (u32)(uintptr_t)Hook_UpdateWorld - (A_UPDATEWORLD_CALL + 5);
    if (!PatchBytes(A_UPDATEWORLD_CALL + 1, &rel, 4)) { Log("patch UpdateWorld call failed"); return; }

    Log("hooks installed (UpdateWorld call @%08X, DestroyAll @%08X). F9 spawn / F10 teleport+revive / F11 remove / P1 holds Num0 next to a downed P2 = revive.",
        A_UPDATEWORLD_CALL, A_DESTROYALL);
    InstallEnemyHooks();
    {
        int ok1 = PatchCallTo(A_ADDHEALTH_CALL1, A_ADDHEALTH, (const void *)Hook_AddHealth);
        int ok2 = PatchCallTo(A_ADDHEALTH_CALL2, A_ADDHEALTH, (const void *)Hook_AddHealth);
        Log("health pickups for P2: %d of 2 AddHealth call sites hooked", ok1 + ok2);
    }
    Lan_Install();
    Log("co-op menu: press F6 in a level (settings in %s)", g_iniPath);

    /* split-screen: find the single "call RenderFrame" in GameLoop by scanning for E8 rel32 -> 0x4555A0 */
    for (a = A_MAINLOOP_LO; a < A_MAINLOOP_HI; a++) {
        if (rd8(a) == 0xE8 && (u32)(a + 5 + rd32(a + 1)) == A_RENDER) { renderCall = a; found++; }
    }
    if (found != 1) { Log("split-screen: %d call sites of RenderFrame found in GameLoop (need exactly 1) -> split-screen disabled", found); return; }
    g_pool = (u8 *)VirtualAlloc(NULL, POOL_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_pool) { Log("split-screen: VirtualAlloc for the polygon pool failed -> disabled"); return; }
    rel = (u32)(uintptr_t)Hook_Render - (renderCall + 5);
    if (!PatchBytes(renderCall + 1, &rel, 4)) { Log("split-screen: patching the RenderFrame call failed -> disabled"); return; }
    g_splitOk = 1;
    Log("split-screen hook installed (RenderFrame call @%08X). F8 on/off, F7 layout.", renderCall);

    /* P2 HUD: redirect the single "call DrawHud" inside RenderFrame so P2's pass can skip it */
    if (rd8(A_HUD_CALL) == 0xE8 && (u32)(A_HUD_CALL + 5 + rd32(A_HUD_CALL + 1)) == A_HUD_DRAW) {
        rel = (u32)(uintptr_t)Hook_Hud - (A_HUD_CALL + 5);
        if (PatchBytes(A_HUD_CALL + 1, &rel, 4)) { g_hudHooked = 1; Log("HUD hook installed (DrawHud call @%08X): P2 view gets its own HUD.", A_HUD_CALL); }
        else Log("HUD hook: patching the DrawHud call failed -> the game's HUD stays visible in P2's view");
    } else {
        Log("HUD hook: call site @%08X does not look like 'call DrawHud' -> the game's HUD stays visible in P2's view", A_HUD_CALL);
    }
#if P2_CUSTOM_HUD && P2_HUD_STYLE == 1
    /* style 1: the timer call inside DrawHud, so P2's call can skip the level-script HUD callbacks */
    if (g_hudHooked) {
        if (PatchCallTo(A_HUD_TIMER_CALL, A_HUD_TIMER, (const void *)Hook_HudTimer)) {
            g_hudTimerHooked = 1;
            Log("HUD: timer call @%08X hooked -> P2 view gets the game's own HUD (timer=%d boss bar=%d compass=%d)",
                A_HUD_TIMER_CALL, P2_HUD_TIMER, P2_HUD_BOSSBAR, P2_HUD_COMPASS);
        } else {
            Log("HUD: timer call @%08X does not match -> P2 view keeps the v0.8 quad HUD", A_HUD_TIMER_CALL);
        }
    }
#endif
}

/* ------------------------------------------------------------------ */
/*  Crash logging (always; stage 10..17 = inside the split-screen code)  */
/* ------------------------------------------------------------------ */
static LONG CALLBACK CrashLogger(PEXCEPTION_POINTERS ep)
{
    static volatile LONG count = 0;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_IN_PAGE_ERROR) {
        PCONTEXT c = ep->ContextRecord;
        void *addr = ep->ExceptionRecord->ExceptionAddress;
        HMODULE mod = NULL;
        char modName[MAX_PATH];
        DWORD *sp = (DWORD *)(uintptr_t)c->Esp;
        if (InterlockedIncrement(&count) > 12) return EXCEPTION_CONTINUE_SEARCH;   /* do not flood the log */
        modName[0] = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &mod) && mod)
            GetModuleFileNameA(mod, modName, MAX_PATH);
        Log("EXCEPTION %08lX at %p [module %s +%X] (access addr %p)  P2=%08X frame=%u stage=%d split=%d",
            code, addr, modName[0] ? modName : "?", (unsigned)((uintptr_t)addr - (uintptr_t)mod),
            (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters > 1) ?
                (void *)ep->ExceptionRecord->ExceptionInformation[1] : NULL,
            g_p2, g_frame, (int)g_stage, g_split);
        Log("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        if (!IsBadReadPtr(sp, 16 * sizeof(DWORD))) {
            Log("  stack: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX", sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
            Log("         %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX", sp[8], sp[9], sp[10], sp[11], sp[12], sp[13], sp[14], sp[15]);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ------------------------------------------------------------------ */
/*  dinput8.dll proxy                                                  */
/* ------------------------------------------------------------------ */
typedef HRESULT (WINAPI *PfnDI8Create)(HINSTANCE, DWORD, const void *, void **, void *);
static HMODULE g_realDI = NULL;
static volatile int g_diCalled = 0;                   /* did the game ever call DirectInput8Create? */

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD ver, const void *riid,
                                                        void **out, void *outer)
{
    PfnDI8Create fn;
    g_diCalled = 1;
    InstallHooks();           /* first call happens during game init, outside loader lock */
    if (!g_realDI) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);   /* SysWOW64 for a 32-bit process on 64-bit Windows */
        strcat(path, "\\dinput8.dll");
        g_realDI = LoadLibraryA(path);
        if (!g_realDI) { Log("cannot load real dinput8: %s", path); return E_FAIL; }
    }
    fn = (PfnDI8Create)GetProcAddress(g_realDI, "DirectInput8Create");
    if (!fn) return E_FAIL;
    return fn(hinst, ver, riid, out, outer);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        DisableThreadLibraryCalls(h);
        GetModuleFileNameA(NULL, path, MAX_PATH);
        { char *s = strrchr(path, '\\'); if (s) s[1] = 0; else path[0] = 0; }
        {   /* keep the previous run's log: coop_log.txt -> coop_log_prev.txt */
            char prev[MAX_PATH];
            strcpy(prev, path);
            strcat(path, "coop_log.txt");
            strcat(prev, "coop_log_prev.txt");
            MoveFileExA(path, prev, MOVEFILE_REPLACE_EXISTING);
        }
        g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        strcpy(g_iniPath, path);                                 /* .../coop_log.txt -> .../coop.ini */
        { char *s2 = strrchr(g_iniPath, '\\'); if (s2) strcpy(s2 + 1, "coop.ini"); else strcpy(g_iniPath, "coop.ini"); }
        CfgLoad();
        Log("spidey coop proxy v0.12 (LAN co-op phase 1, P2 state kept between levels, P2 gamepad, in-game menu F6, coop.ini, auto-spawn, P2 health pickups, split-screen, game-style P2 HUD, cut-scene safe, P2 own camera, enemies see both players, downed state + revive, per-player aim bars) loaded");
        { char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH); Log("loaded into process: %s", exe); }
        AddVectoredExceptionHandler(1, CrashLogger);
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("process detach (%s): P2=%08X frame=%u stage=%d, DirectInput8Create called=%d", reserved ? "process is exiting" : "DLL unloaded", g_p2, g_frame, (int)g_stage, (int)g_diCalled);
    }
    return TRUE;
}
