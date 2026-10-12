# Legal notes and copyright assessment

*Not legal advice. This is the project's own assessment of what it publishes and why. If you have a real legal
question, ask a lawyer in your country.*

## The short version

- The repository contains **only original code and documentation**, under the MIT license.
- It contains **no** game files: no executable, art, models, sound, levels, scripts or text from the game.
- It contains **no** decompiled or disassembled game code. Only *facts needed for interoperability* are present:
  memory addresses, structure offsets, calling conventions and short descriptions of what a function does.
- It does **not** circumvent copy protection. The mod never touches a CD check, DRM or licence check, and it
  never will (see below).
- Players need their own copy of the game.
- "Spider-Man" is used only to say which game the mod works with (nominative use). There are no logos, character
  art or screenshots of protected characters in the repository or on the website.

## Who owns what

| Thing | Owner | In this repository? |
|---|---|---|
| Spider-Man character, name, logos | Marvel | name only, as a reference to the game |
| Spider-Man (2000) game, PC port, `SpideyPC.exe`, data files | the game's publisher and rights holders | no |
| SM2K Co-op source and documentation | SM2K Co-op contributors | yes, MIT |

## Why the published material is believed to be lawful

### 1. Mods that do not include the original game

A mod that ships only its own code, and runs on top of a copy the player already owns, does not copy the game.
This is the common model for PC game mods and has been tolerated for decades. Our release archive contains only
`dinput8.dll`, an example config and text files.

### 2. Reverse engineering for interoperability

Writing the mod required studying how the game works. Many legal systems allow this when the goal is
interoperability with an independently created program, and the result does not reproduce the original:

- **EU:** Directive 2009/24/EC, Article 6 (decompilation for interoperability).
- **Russia:** Civil Code, Article 1280, paragraphs 2–3. A lawful user may study and decompile a program to make an
  independently created program work with it, within the limits stated there.
- **USA:** case law on intermediate copying for interoperability (*Sega v. Accolade*, 1992; *Sony v. Connectix*,
  2000) and 17 U.S.C. §1201(f) (the interoperability exemption).

A recurring limit across these laws: the information obtained may be shared only as far as interoperability needs
it, and must not be used to build a program substantially similar to the original. That is why the repository has
addresses and short notes but **never the decompiled listing itself**, and why the mod is a different program, a
co-op layer, not a copy of the game.

### 3. No circumvention of protection measures

Anti-circumvention laws (US DMCA §1201, EU InfoSoc Directive Article 6, and Russian Civil Code Article 1299) are
the most common reason game-related repositories get taken down. SM2K Co-op does not touch any copy protection.
**Pull requests that add a no-CD patch, a crack, or anything that disables a protection check will be rejected.**

### 4. Trademarks

The project's name, SM2K Co-op, does not use the trademark as a brand. The full game title appears only in
descriptions, to identify compatibility. The website uses only original typography and graphics.

## AI-generated material

Most of the code and documentation was written by an AI assistant under human direction (see
[AI_DISCLOSURE.md](../AI_DISCLOSURE.md)). Under the US Copyright Office's current guidance, material generated
entirely by AI may not be protected by copyright, while human selection, arrangement and modification can be. This
does not create any obligation to the game's rights holders. It only means that the project's own copyright may be
thin. The MIT license is used anyway so that the permissions are clear.

## If a rights holder complains

1. Read the notice carefully and identify exactly which file it is about.
2. If the notice is correct (for example, someone committed a game file), remove that file *and* purge it from the
   history, then reply.
3. If you believe the notice is wrong, GitHub's DMCA policy allows a counter-notice. Consider getting legal advice
   first, because a counter-notice has legal consequences.
4. Do not "route around" a valid takedown by re-uploading the same material elsewhere.

## Rules for contributors (summary)

- Never commit: the game's exe or data files, extracted assets, decompiled or disassembled listings, Ghidra or IDA
  databases, or memory dumps that contain game code.
- Do commit: your own code, addresses and offsets with a one-line description, and logs written by the mod.
- Screenshots: gameplay screenshots in issues are fine, as is common for bug reports. Don't add game art to the
  repository or the website.
