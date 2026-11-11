#!/bin/bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
version="$(sed -nE 's/^project\(Smoosher VERSION ([0-9]+\.[0-9]+\.[0-9]+)\)$/\1/p' "$repo_dir/CMakeLists.txt")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Cannot read the release version from CMakeLists.txt" >&2
    exit 1
fi

if [[ $# -gt 0 && "$1" != "v$version" ]]; then
    echo "Release tag must be v$version to match CMakeLists.txt (got $1)" >&2
    exit 1
fi
printf '%s\n' "$version"
