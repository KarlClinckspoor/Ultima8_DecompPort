# Shared by the release scripts: paths, the version, the license files.
set -euo pipefail
PORTABLE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REPO="$(cd "$PORTABLE/.." && pwd)"
VERSION="${1:?usage: $(basename "$0") VERSION [OUTDIR]  (VERSION like 0.1.0)}"
VERSION="${VERSION#v}"
OUT="$(mkdir -p "${2:-$REPO/dist}" && cd "${2:-$REPO/dist}" && pwd)"
WORK="$REPO/build/release"
mkdir -p "$WORK"

# The licenses of what a package contains, and where its source is.
add_licenses() {	# add_licenses DIR [qt]
	local dir="$1/LICENSES"
	mkdir -p "$dir"
	cp "$PORTABLE/third_party/libmatoya/LICENSE" "$dir/libmatoya-MIT.txt"
	cp "$PORTABLE/third_party/munt/mt32emu/COPYING.LESSER.txt" "$dir/mt32emu-LGPL-2.1.txt"
	cp "$PORTABLE/third_party/nuked-opl3/LICENSE" "$dir/nuked-opl3-LGPL-2.1.txt"
	if [ "${2:-}" = qt ]; then
		rm "$dir"/libmatoya-MIT.txt "$dir"/mt32emu-LGPL-2.1.txt "$dir"/nuked-opl3-LGPL-2.1.txt
		cp "$PORTABLE/tools/release/QT-LGPL.txt" "$dir/Qt-LGPL.txt"
	fi
	cat > "$dir/SOURCE.txt" <<NOTE
Built from https://github.com/KarlClinckspoor/Ultima8_DecompPort at tag v$VERSION.
The complete source of this program and of the libraries it contains is there; the
libraries' own versions are listed in ultima8-portable/third_party/README.md.
$( [ "${2:-}" = qt ] && echo "Qt is used under the LGPL v3, unmodified, as the shared libraries beside the program;
its source is at https://download.qt.io/official_releases/qt/ ." )
NOTE
}
