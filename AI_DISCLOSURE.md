# AI disclosure

This project was made with substantial help from an AI assistant: **Claude, by Anthropic**.
We say so up front, so that users and contributors know how the code came about and how far to trust it.

## Who did what

| Part | Done by |
|---|---|
| Idea, goals, feature decisions, priorities | the human author |
| Testing in the game, logs, bug reports | the human author |
| Reverse-engineering notes (what an address or function does) | the AI, working from analysis the author ran on his own copy of the game; checked against in-game logs |
| C source of the mod (`src/dinput8.c`), build scripts, CI | the AI, revised after each test round |
| Documentation, this repository's texts and website | the AI, reviewed by the author |
| Code review of the network code (v0.12) | a second, independent AI pass; its findings were fixed before release |

## What this means for you

- **The code can contain mistakes.** Some addresses and behaviours were inferred from analysis and have only been
  confirmed where the logs show them working. The source comments say which parts are verified and which are
  assumptions.
- **No game code was copied.** The AI was told to write original code and to keep only the facts needed for
  interoperability, such as addresses, structure offsets and calling conventions. See [docs/LEGAL.md](docs/LEGAL.md).
- **Copyright note.** In some jurisdictions, purely AI-generated material is not protected by copyright. The
  project is released under the MIT license anyway, so everyone has the same broad permission to use and change it
  whatever its exact copyright status.
- **Contributions** written by humans, with AI help, or by AI are all welcome. Say in the pull request how the
  change was made, and test it in the game.
