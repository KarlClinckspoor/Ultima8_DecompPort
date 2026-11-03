# Releasing

How the portable Ultima VIII (`ultima8-portable/`) and its editor (`ultima8-portable/editor/`)
are released for Windows and Linux.

## What a release is

One version for both programs, `MAJOR.MINOR.PATCH` (no `v`: the first release is `0.1.0`), as
a git tag on `master` and a GitHub release with these packages:

| Package | What | Built on |
| --- | --- | --- |
| `ultima8-VERSION-windows-x64.zip` | `Ultima8.exe`, one file | Linux, cross-compiled with mingw-w64 |
| `ultima8-VERSION-linux-x64.tar.gz` | `Ultima8`, one file | Linux (clang) |
| `u8edit-VERSION-windows-x64.zip` | `u8edit.exe` with Qt's DLLs | Windows, MSYS2 UCRT64 |
| `u8edit-VERSION-linux-x64.tar.gz` | `u8edit` with Qt in `lib/` and `plugins/`, started by `./u8edit` | Linux |

Each holds a `README.txt` (from `ultima8-portable/tools/release/README-*.txt`) and a
`LICENSES/` folder: the licenses of the libraries inside (libmatoya MIT; Munt's mt32emu and
Nuked OPL3, LGPL 2.1; Qt, LGPL 3, as separate libraries) and where the source is. No package
holds anything of the game: players need their own copy.

While the version is 0.x, the minor number goes up for new features and the patch number for
fixes. A recording made by one version replays in another only if the game's behaviour did
not change; say so in the notes when it did.

### Compatibility

- **Linux:** the packages are built on Ubuntu 24.04 and need glibc 2.38 and libstdc++ from
  GCC 12 or newer. The editor carries its Qt (and ICU, HarfBuzz, xcb utilities, ...) but uses
  the system's C and C++ run-times, OpenGL, X11, fontconfig, GLib and D-Bus, as AppImages do.
  To reach older systems, build on an older distribution.
- **Windows:** 10 or 11, 64-bit. The port's mingw build draws with Vulkan only (libmatoya's
  Direct3D renderers need Microsoft's shader compiler). The editor runs on the Qt that MSYS2
  ships.

## Making one

A release is published by GitHub Actions (`.github/workflows/release.yml`) when a push to
`master` changes `VERSION`. Pushing a branch is all it takes, so whoever can push to `master`
can release; a tag or a release made by hand is not needed.

1. **Test** the commit to release:
   - the port: a scripted new game on Linux and, under Wine or on Windows, on the Windows
     build (`U8_TEST_INPUT`, see `ultima8-portable/README.md`), and a recording replayed by the
     new build;
   - the editor: `u8edit-tests` (`-DU8EDIT_TESTS=ON`, `U8_DATA=...`), and opening a map in each
     packaged build.
2. **Write the notes**: a `## VERSION` section at the top of `CHANGELOG.md`, for players: what
   is new, what changed, what is needed.
3. **Put the number in `VERSION`**, commit both, and push to `master`.

The workflow then:

- reads `VERSION`; if that version has a release already, it stops (so pushes that do not
  change the version, or a re-run, publish nothing). It fails if `CHANGELOG.md` has no section
  for it;
- builds the four packages, the Linux ones and the Windows port on Ubuntu, the Windows
  editor on Windows in MSYS2;
- creates the tag and the release on the pushed commit, with the changelog's section as its
  text and the packages attached.

It can be run again by hand (Actions > Release > Run workflow), which does the same for the
version in `VERSION`. A failed run publishes nothing: fix it and push again, or run it again.

4. **Check the release**: download each package on a clean machine (or a fresh folder),
   unpack it, and start it on a game folder.

**By hand**, without the workflow: the same scripts make the packages, into `dist/`,

```sh
ultima8-portable/tools/release/linux.sh VERSION          # Linux: the port and the editor
ultima8-portable/tools/release/windows-port.sh VERSION   # Linux, with mingw-w64: the port for Windows
ultima8-portable/tools/release/windows-editor.sh VERSION # Windows, in MSYS2 UCRT64: the editor
```

then create the release on GitHub (tag `VERSION` on `master`), with
`ultima8-portable/tools/release/notes.sh VERSION` as its text, and attach `dist/*`.

## Scripts

`ultima8-portable/tools/release/`:

| Script | Does |
| --- | --- |
| `common.sh` | the paths, the version and the `LICENSES/` folder, for the others |
| `linux.sh` | builds the port and the editor and packs them; bundles Qt for the editor |
| `windows-port.sh` | cross-compiles the port with `cmake/mingw-w64.cmake` and zips it |
| `windows-editor.sh` | builds the editor in MSYS2, runs `windeployqt6`, adds the DLLs it leaves out, zips it |
| `notes.sh` | prints a version's section of `CHANGELOG.md` |

Builds go to `build/release/` and packages to `dist/`, both ignored by git.
