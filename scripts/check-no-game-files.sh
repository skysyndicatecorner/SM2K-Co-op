#!/usr/bin/env bash
# Fails if the repository contains game files or decompiler output. Run by CI on every push and pull request.
set -euo pipefail
cd "$(dirname "$0")/.."
bad=0
files="$(git ls-files 2>/dev/null || find . -type f -not -path './.git/*' -not -path './build/*' -not -path './dist/*')"
while IFS= read -r f; do
  [ -z "$f" ] && continue
  case "${f,,}" in
    *.exe|*.pkr|*.bik|*.trg|*.psx|*.gzf|*.gpr|*.rep|*.idb|*.i64|*.dmp)
      echo "::error file=$f::game or reverse-engineering file type must not be committed"; bad=1;;
  esac
  case "$f" in *.c|*.h|*.txt|*.md|*.asm|*.s)
    # Ghidra / IDA output markers: a few mentions in comments are fine, a listing is not
    n=$(grep -c -E 'undefined[1248]|LAB_00[0-9a-f]{6}|\bthunk_FUN_|__thiscall FUN_' "$f" 2>/dev/null || true)
    if [ "${n:-0}" -gt 10 ]; then echo "::error file=$f::looks like decompiler output ($n markers)"; bad=1; fi
  esac
  sz=$(stat -c %s "$f" 2>/dev/null || echo 0)
  if [ "$sz" -gt 2000000 ]; then echo "::error file=$f::file larger than 2 MB, game data?"; bad=1; fi
done <<< "$files"
[ "$bad" -eq 0 ] && echo "OK: no game files or decompiler output"
exit $bad
