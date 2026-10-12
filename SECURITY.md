# Security and antivirus notes

## Verify a release

Every release has a `SHA256SUMS.txt` and a GitHub build-provenance attestation.

```powershell
certutil -hashfile dinput8.dll SHA256
```

Compare the result with `SHA256SUMS.txt`. If you have the GitHub CLI, you can also verify where the file came from:

```sh
gh attestation verify dinput8.dll --repo <owner>/sm2k-coop
```

The release DLL is built by `scripts/build-linux-clang.sh` with a pinned toolchain, so it is **reproducible**.
On Ubuntu 24.04 with clang 18.1.3, running the script on the tagged source gives a byte-identical file.

## Why antivirus software may flag the mod

`dinput8.dll` is a proxy DLL: it is loaded in place of a system DLL and patches the game's code in memory. LAN mode
also opens a UDP socket. Malware does the same things, so heuristic detections such as "Trojan:Win32/Wacatac" can
happen. To reduce the risk, the DLL is:

- built in public CI from the source in this repository;
- not packed and not obfuscated;
- free of downloads and process launches. It talks to the network only when you turn on LAN mode, and only to the
  address you enter.

If your antivirus flags a release, check the hash first, then report a false positive
(Microsoft: https://www.microsoft.com/wdsi/filesubmission). Don't turn your antivirus off.

## What the DLL does on your PC

| Action | Where |
|---|---|
| Patches call sites inside `SpideyPC.exe` (in memory only, never on disk) | game process |
| Writes `coop.ini`, `coop_log.txt`, `coop_log_prev.txt` | game folder |
| Loads `xinput1_4.dll` / `xinput1_3.dll` / `xinput9_1_0.dll` when the player 2 gamepad option is on | system |
| Opens UDP port 27016 (host) or a random port (join) when LAN mode is on | network |

It doesn't change the registry, write outside the game folder, or start other programs.

## Reporting a vulnerability

The LAN code parses packets from the network. If you find a way for a malicious packet to crash the game or do
anything worse, please open a private security advisory (GitHub → Security → Report a vulnerability) instead of a
public issue.
