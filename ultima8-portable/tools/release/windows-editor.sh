#!/usr/bin/env bash
# The editor for Windows: built in MSYS2's UCRT64 terminal, with Qt's DLLs and the run-time
# DLLs beside it, as a .zip in OUTDIR (default dist/).
#   tools/release/windows-editor.sh 0.1.0
# Needs, in UCRT64: pacman -S --needed zip mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-tools}
source "$(dirname "$0")/common.sh"

cmake -S "$PORTABLE/editor" -B "$WORK/windows-editor" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/windows-editor" --target u8edit
pkg="u8edit-$VERSION-windows-x64"
dir="$WORK/$pkg"
rm -rf "$dir" && mkdir -p "$dir"
cp "$WORK/windows-editor/u8edit.exe" "$dir/"
strip "$dir/u8edit.exe"
# Qt's DLLs and plug-ins; then everything else they need from UCRT64 (the compiler's run-time,
# ICU, FreeType, HarfBuzz, zlib, ...), which windeployqt leaves out for MSYS2 builds.
windeployqt6 --no-translations --no-compiler-runtime "$dir/u8edit.exe" \
	|| windeployqt --no-translations --no-compiler-runtime "$dir/u8edit.exe"
for _ in 1 2; do
	ldd "$dir"/u8edit.exe "$dir"/*.dll "$dir"/*/*.dll 2>/dev/null | grep -i '/ucrt64/' \
		| awk '{print $3}' | sort -u | while read -r dll; do
		[ -e "$dir/$(basename "$dll")" ] || cp "$dll" "$dir/"
	done
done
sed 's/$/\r/' "$PORTABLE/tools/release/README-editor.txt" > "$dir/README.txt"
add_licenses "$dir" qt
rm -f "$OUT/$pkg.zip"
(cd "$WORK" && zip -qr "$OUT/$pkg.zip" "$pkg")
ls -la "$OUT/$pkg.zip"
