#!/bin/bash
# Package an already-built universal AU/VST3 pair. Signing is mandatory.
set -euo pipefail
# Payload directories must be readable by all users, even when the caller uses
# a restrictive umask to handle private keys.
umask 022

if [[ $# -ne 2 ]]; then
    echo "Usage: bash scripts/package-macos.sh <directory-containing-AU-and-VST3> <output.pkg>" >&2
    exit 1
fi
: "${MACOS_APPLICATION_IDENTITY:?Set the Developer ID Application identity}"
: "${MACOS_INSTALLER_IDENTITY:?Set the Developer ID Installer identity}"
: "${MACOS_KEYCHAIN:?Set the signing keychain path}"

script_dir="$(cd "$(dirname "$0")" && pwd)"
version="$(bash "$script_dir/release-version.sh")"
bash "$script_dir/verify-macos.sh" "$1"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/smoosher-package.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT
root="$work_dir/root"
plugins="$root/Library/Audio/Plug-Ins"
mkdir -p "$plugins/Components" "$plugins/VST3" "$(dirname "$2")"
/usr/bin/ditto "$1/AU/Smoosher.component" "$plugins/Components/Smoosher.component"
/usr/bin/ditto "$1/VST3/Smoosher.vst3" "$plugins/VST3/Smoosher.vst3"

# These JUCE bundles contain one executable each and no nested code.
# Sign only the staged copies, after all bundle modifications are complete.
for bundle in "$plugins/Components/Smoosher.component" "$plugins/VST3/Smoosher.vst3"; do
    /usr/bin/codesign --force --timestamp --options runtime \
        --keychain "$MACOS_KEYCHAIN" --sign "$MACOS_APPLICATION_IDENTITY" "$bundle"
    /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle"
done

# Disable relocation so an old copy elsewhere cannot redirect an update.
/usr/bin/pkgbuild --analyze --root "$root" "$work_dir/components.plist"
python3 - "$work_dir/components.plist" <<'PY'
import plistlib
import sys
path = sys.argv[1]
with open(path, "rb") as source:
    components = plistlib.load(source)
for component in components:
    component["BundleIsRelocatable"] = False
    component["BundleOverwriteAction"] = "upgrade"
with open(path, "wb") as destination:
    plistlib.dump(components, destination)
PY
/usr/bin/pkgbuild --root "$root" --component-plist "$work_dir/components.plist" \
    --identifier com.helvetesound.smoosher.pkg --version "$version" \
    --install-location / --ownership recommended "$work_dir/payload.pkg"

# The distribution enforces the same minimum OS and architectures as the build.
cat > "$work_dir/requirements.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>os</key><array><string>11.0</string></array>
<key>arch</key><array><string>arm64</string><string>x86_64</string></array>
</dict></plist>
PLIST
/usr/bin/productbuild --synthesize --product "$work_dir/requirements.plist" \
    --package "$work_dir/payload.pkg" "$work_dir/distribution.xml"
python3 - "$work_dir/distribution.xml" "$version" <<'PY'
import sys
import xml.etree.ElementTree as ET
path, version = sys.argv[1:]
tree = ET.parse(path)
root = tree.getroot()
ET.SubElement(root, "title").text = "Smoosher " + version
ET.SubElement(root, "domains", enable_localSystem="true",
              enable_currentUserHome="false", enable_anywhere="false")
tree.write(path, encoding="utf-8", xml_declaration=True)
PY
/usr/bin/productbuild --distribution "$work_dir/distribution.xml" --package-path "$work_dir" \
    --sign "$MACOS_INSTALLER_IDENTITY" --keychain "$MACOS_KEYCHAIN" \
    --timestamp "$2"
/usr/sbin/pkgutil --check-signature "$2"

# Verify the shipped payload, not just the pre-package staging directory.
/usr/sbin/pkgutil --expand-full "$2" "$work_dir/expanded"
for relative_path in Components/Smoosher.component VST3/Smoosher.vst3; do
    /usr/bin/codesign --verify --deep --strict --verbose=2 \
        "$work_dir/expanded/payload.pkg/Payload/Library/Audio/Plug-Ins/$relative_path"
done
