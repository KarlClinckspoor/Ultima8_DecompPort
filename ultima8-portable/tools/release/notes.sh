#!/usr/bin/env bash
# Prints the CHANGELOG.md section of a version: the release notes.
#   tools/release/notes.sh 0.1.0
set -euo pipefail
v="${1#v}"
awk -v v="$v" '
	/^## / { show = ($2 == "v" v || $2 == v) ; next }
	show' "$(dirname "$0")/../../../CHANGELOG.md"
