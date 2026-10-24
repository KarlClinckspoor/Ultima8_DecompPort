#!/bin/bash
# Compiles game sources with -fsyntax-only and lists their errors.
#   tools/syntax.sh                 every file under game/
#   tools/syntax.sh game/u8/x.cpp   just these
# Logs go to $OUT (default /tmp/u8syntax): one per file, summary.txt, unique.txt.
cd "$(dirname "$0")/.."
OUT=${OUT:-/tmp/u8syntax}
INC=$(find game platform -type d | sed 's/^/-I/' | tr '\n' ' ')
rm -rf "$OUT"; mkdir -p "$OUT"
FILES="$@"
[ -z "$FILES" ] && FILES=$(find game -name '*.cpp' | sort)
CXX=${CXX:-clang++}
printf '%s\n' $FILES | xargs -P 8 -I{} sh -c "$CXX -std=gnu++20 -fsyntax-only -fsigned-char -fno-delete-null-pointer-checks -ferror-limit=0 -Wno-everything $INC {} > \"$OUT/\$(echo {} | tr / _).log\" 2>&1"
cat "$OUT"/*.log | grep "error:" | sort -u > "$OUT/unique.txt"
for f in $FILES; do n=$(grep -c "error:" "$OUT/$(echo $f | tr / _).log"); echo "$n $f"; done | sort -rn > "$OUT/summary.txt"
echo "errors: $(awk '{s+=$1} END {print s+0}' "$OUT/summary.txt"), unique: $(wc -l < "$OUT/unique.txt"), clean files: $(awk '$1==0' "$OUT/summary.txt" | wc -l) of $(wc -l < "$OUT/summary.txt")"
