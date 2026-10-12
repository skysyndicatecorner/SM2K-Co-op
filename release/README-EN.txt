SM2K Co-op — co-op mod for Spider-Man (2000, PC)
================================================

Unofficial fan mod. You need your own copy of the game, version 1.3.
Built with the help of AI (Claude by Anthropic), tested in the game by a human.
Source and help: {{REPO_URL}}

INSTALL
  1. Copy dinput8.dll into the game folder, next to SpideyPC.exe.
     You can copy coop.ini there too (nick and network settings).
  2. Start the game, load a level, press F6 for the mod menu.
  Uninstall: delete dinput8.dll.

TWO PLAYERS, ONE PC
  Player 2 joins automatically when a level starts.
  Keyboard: W A S D move, K jump, J punch, L kick, I web, U / P / O extra, H = Enter.
  Gamepad: F6, P2 GAMEPAD. F8 split-screen on/off, F7 layout.
  To revive a downed player 2: player 1 walks up and holds Num0.

NETWORK (LAN / Radmin VPN)
  Host:   F6, LAN CO-OP = HOST. Tell your friend your IP (Radmin: 26.x.x.x).
  Friend: F6, HOST IP, Enter, type the IP, Enter. Then LAN CO-OP = JOIN.
  The status line should read "OK: <NICK>". On different levels the friend picks GO TO PARTNER.
  Firewall: allow SpideyPC.exe, or run as administrator
    netsh advfirewall firewall add rule name="SM2K Co-op" dir=in action=allow protocol=UDP localport=27016

ANTIVIRUS WARNINGS
  Proxy DLLs that patch a game in memory are sometimes flagged. Check the hash:
    certutil -hashfile dinput8.dll SHA256
  It must match SHA256SUMS.txt. Then report a false positive:
  https://www.microsoft.com/wdsi/filesubmission

PROBLEMS
  Attach coop_log.txt from the game folder to a GitHub issue (for LAN: from both PCs).

Spider-Man is a trademark of Marvel. This mod is not affiliated with or endorsed by Marvel or Activision.
Code license: MIT (LICENSE.txt).
