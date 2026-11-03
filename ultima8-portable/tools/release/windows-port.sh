#!/usr/bin/env bash
# The port for Windows, cross-compiled on Linux with mingw-w64: a .zip in OUTDIR (default dist/).
#   tools/release/windows-port.sh 0.1.0
# Needs the mingw-w64 package (x86_64-w64-mingw32-gcc), cmake, ninja and zip.
source "$(dirname "$0")/common.sh"

cmake -S "$PORTABLE" -B "$WORK/windows-port" -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE="$PORTABLE/cmake/mingw-w64.cmake"
cmake --build "$WORK/windows-port" --target u8
pkg="ultima8-$VERSION-windows-x64"
rm -rf "$WORK/$pkg" && mkdir -p "$WORK/$pkg"
cp "$WORK/windows-port/Ultima8.exe" "$WORK/$pkg/"
sed 's/$/\r/' "$PORTABLE/tools/release/README-port.txt" > "$WORK/$pkg/README.txt"
add_licenses "$WORK/$pkg"
rm -f "$OUT/$pkg.zip"
(cd "$WORK" && zip -qr "$OUT/$pkg.zip" "$pkg")
ls -la "$OUT/$pkg.zip"
