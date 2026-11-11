#!/bin/bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: bash scripts/verify-macos.sh <directory-containing-AU-and-VST3>" >&2
    exit 1
fi
script_dir="$(cd "$(dirname "$0")" && pwd)"
version="$(bash "$script_dir/release-version.sh")"

for relative_path in AU/Smoosher.component VST3/Smoosher.vst3; do
    bundle="$1/$relative_path"
    /usr/bin/lipo "$bundle/Contents/MacOS/Smoosher" -verify_arch arm64
    /usr/bin/lipo "$bundle/Contents/MacOS/Smoosher" -verify_arch x86_64
    bundle_version="$(/usr/bin/plutil -extract CFBundleShortVersionString raw -o - "$bundle/Contents/Info.plist")"
    if [[ "$bundle_version" != "$version" ]]; then
        echo "$bundle has version $bundle_version; expected $version" >&2
        exit 1
    fi
done
