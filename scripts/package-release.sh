#!/usr/bin/env bash
# Package a release:  scripts/package-release.sh v0.12 [owner/repo]
# Needs build/dinput8.dll (scripts/build-linux-clang.sh). Writes dist/sm2k-coop-<ver>-win32.zip + SHA256SUMS.txt
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="${1:?version, e.g. v0.12}"
REPO="${2:-${GITHUB_REPOSITORY:-}}"
if [ -n "$REPO" ]; then REPO_URL="https://github.com/$REPO"; else REPO_URL="GitHub: sm2k-coop"; REPO="OWNER/sm2k-coop"; fi
NAME="sm2k-coop-$VER-win32"
OUT="$ROOT/dist/$NAME"
rm -rf "$OUT" "$ROOT/dist/$NAME.zip"; mkdir -p "$OUT"
cp "$ROOT/build/dinput8.dll" "$OUT/"
cp "$ROOT/config/coop.ini.example" "$OUT/coop.ini"
for f in README-RU.txt README-EN.txt; do sed -e "s#{{REPO_URL}}#$REPO_URL#g" "$ROOT/release/$f" > "$OUT/$f"; done
cp "$ROOT/LICENSE" "$OUT/LICENSE.txt"
cp "$ROOT/docs/LEGAL.md" "$OUT/LEGAL.md"
# Windows line endings for the text files people open in Notepad
for f in "$OUT"/*.txt "$OUT"/*.ini "$OUT"/*.md; do sed -i 's/\r$//; s/$/\r/' "$f"; done
( cd "$OUT" && sha256sum dinput8.dll > SHA256SUMS.txt )
# fixed timestamps and order -> the same zip every time
find "$OUT" -exec env TZ=UTC touch -d '2001-08-15 00:00:00' {} +
( cd "$ROOT/dist" && find "$NAME" | LC_ALL=C sort | TZ=UTC zip -X -q "$NAME.zip" -@ )
( cd "$ROOT/dist" && sha256sum "$NAME.zip" "$NAME/dinput8.dll" | sed "s#$NAME/##" > "$NAME.sha256.txt" )
sed "s#{{REPO}}#$REPO#g" "$ROOT/docs/releases/$VER.md" > "$ROOT/dist/release-notes.md" 2>/dev/null || true
echo "OK: dist/$NAME.zip"; cat "$ROOT/dist/$NAME.sha256.txt"
