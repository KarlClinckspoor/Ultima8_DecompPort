#!/usr/bin/env bash
# Linux release packages: the port and the editor, as .tar.gz in OUTDIR (default dist/).
#   tools/release/linux.sh 0.1.0
# Needs clang, cmake, ninja, glslangValidator and Qt 6 (qt6-base-dev, qt6-wayland).
source "$(dirname "$0")/common.sh"

# The port: one program, which opens X11 or Wayland, Vulkan or OpenGL and ALSA at run time.
cmake -S "$PORTABLE" -B "$WORK/linux-port" -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build "$WORK/linux-port" --target u8
pkg="ultima8-$VERSION-linux-x64"
rm -rf "$WORK/$pkg" && mkdir -p "$WORK/$pkg"
cp "$WORK/linux-port/Ultima8" "$WORK/$pkg/"
strip "$WORK/$pkg/Ultima8"
cp "$PORTABLE/tools/release/README-port.txt" "$WORK/$pkg/README.txt"
add_licenses "$WORK/$pkg"
tar -C "$WORK" -czf "$OUT/$pkg.tar.gz" "$pkg"

# The editor, with the Qt libraries and plug-ins it needs in lib/ and plugins/. The libraries
# every desktop has (the C and C++ run-times, OpenGL, X11's core, fontconfig, GLib, D-Bus,
# Wayland) stay the system's, as AppImages leave them.
cmake -S "$PORTABLE/editor" -B "$WORK/linux-editor" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/linux-editor" --target u8edit
pkg="u8edit-$VERSION-linux-x64"
dir="$WORK/$pkg"
rm -rf "$dir" && mkdir -p "$dir/bin" "$dir/lib" "$dir/plugins"
cp "$WORK/linux-editor/u8edit" "$dir/bin/"
strip "$dir/bin/u8edit"
qtplugins="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || qmake6 -query QT_INSTALL_PLUGINS)"
for p in platforms/libqxcb.so platforms/libqwayland-generic.so platforms/libqwayland-egl.so \
	platforms/libqoffscreen.so xcbglintegrations wayland-shell-integration \
	wayland-decoration-client wayland-graphics-integration-client imageformats \
	platforminputcontexts; do
	[ -e "$qtplugins/$p" ] || continue
	mkdir -p "$dir/plugins/$(dirname "$p")"
	cp -a "$qtplugins/$p" "$dir/plugins/$(dirname "$p")/"
done
system='^(linux-vdso|ld-linux|libc|libm|libdl|libpthread|librt|libresolv|libutil|libstdc\+\+|libgcc_s|libGL|libGLX|libGLdispatch|libEGL|libOpenGL|libgbm|libdrm|libX11|libX11-xcb|libxcb|libxcb-glx|libxcb-dri[23]|libxcb-present|libxcb-sync|libXau|libXdmcp|libXext|libXrender|libfontconfig|libfreetype|libexpat|libz|libglib-2\.0|libgobject-2\.0|libgthread-2\.0|libgio-2\.0|libgmodule-2\.0|libdbus-1|libsystemd|libudev|libasound|libwayland-client|libwayland-cursor|libwayland-egl|libffi|libselinux|libmount|libblkid|libpcre2-8|libcap|libgcrypt|libgpg-error|liblzma|liblz4|libbsd|libmd)\.so'
deps() { ldd "$@" 2>/dev/null | awk '/=> \//{print $3}' | sort -u; }
files=("$dir/bin/u8edit" $(find "$dir/plugins" -name '*.so'))
for lib in $(deps "${files[@]}"); do
	base="$(basename "$lib")"
	[[ "$base" =~ $system ]] && continue
	cp -L "$lib" "$dir/lib/"
done
# the libraries the copied ones need in turn
for _ in 1 2 3; do
	for lib in $(deps "$dir"/lib/*.so*); do
		base="$(basename "$lib")"
		[[ "$base" =~ $system ]] || [ -e "$dir/lib/$base" ] || cp -L "$lib" "$dir/lib/"
	done
done
printf '[Paths]\nPrefix=..\nPlugins=plugins\n' > "$dir/bin/qt.conf"
cat > "$dir/u8edit" <<'LAUNCH'
#!/bin/sh
# Starts the editor with the Qt libraries in this folder.
here="$(dirname "$(readlink -f "$0")")"
LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "$here/bin/u8edit" "$@"
LAUNCH
chmod +x "$dir/u8edit"
cp "$PORTABLE/tools/release/README-editor.txt" "$dir/README.txt"
add_licenses "$dir" qt
tar -C "$WORK" -czf "$OUT/$pkg.tar.gz" "$pkg"
ls -la "$OUT"
