#!/bin/bash
# CI entry point: temporary credentials, signing, notarization, and verification.
set -euo pipefail
umask 077

if [[ $# -ne 2 ]]; then
    echo "Usage: bash scripts/release-macos.sh <directory-containing-AU-and-VST3> <output-directory>" >&2
    exit 1
fi
for variable in MACOS_APPLICATION_P12_BASE64 MACOS_INSTALLER_P12_BASE64 \
    MACOS_P12_PASSWORD MACOS_APPLICATION_IDENTITY MACOS_INSTALLER_IDENTITY \
    APPLE_ID APPLE_TEAM_ID APPLE_APP_SPECIFIC_PASSWORD; do
    if [[ -z "${!variable:-}" ]]; then
        echo "Missing required release credential: $variable" >&2
        exit 1
    fi
done

script_dir="$(cd "$(dirname "$0")" && pwd)"
version="$(bash "$script_dir/release-version.sh")"
mkdir -p "$2"
output_dir="$(cd "$2" && pwd)"
package="$output_dir/Smoosher-$version-macOS-universal.pkg"
work_dir="$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/smoosher-signing.XXXXXX")"
export MACOS_KEYCHAIN="$work_dir/release.keychain-db"
keychain_password="$(/usr/bin/openssl rand -hex 32)"

# Save the search list before adding this keychain, and restore it on all exits.
original_keychains=()
while IFS= read -r keychain; do
    keychain="${keychain#*\"}"
    keychain="${keychain%\"*}"
    original_keychains+=("$keychain")
done < <(/usr/bin/security list-keychains -d user)
cleanup() {
    /usr/bin/security list-keychains -d user -s "${original_keychains[@]}" || true
    /usr/bin/security delete-keychain "$MACOS_KEYCHAIN" >/dev/null 2>&1 || true
    rm -rf "$work_dir"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

printf '%s' "$MACOS_APPLICATION_P12_BASE64" | /usr/bin/base64 --decode > "$work_dir/application.p12"
printf '%s' "$MACOS_INSTALLER_P12_BASE64" | /usr/bin/base64 --decode > "$work_dir/installer.p12"
/usr/bin/security create-keychain -p "$keychain_password" "$MACOS_KEYCHAIN"
/usr/bin/security set-keychain-settings -lut 7200 "$MACOS_KEYCHAIN"
/usr/bin/security unlock-keychain -p "$keychain_password" "$MACOS_KEYCHAIN"
/usr/bin/security list-keychains -d user -s "$MACOS_KEYCHAIN" "${original_keychains[@]}"
for certificate in "$work_dir/application.p12" "$work_dir/installer.p12"; do
    /usr/bin/security import "$certificate" -k "$MACOS_KEYCHAIN" -P "$MACOS_P12_PASSWORD" \
        -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign
done
/usr/bin/security set-key-partition-list -S apple-tool:,apple: -s \
    -k "$keychain_password" "$MACOS_KEYCHAIN" >/dev/null
rm "$work_dir/application.p12" "$work_dir/installer.p12"

/usr/bin/xcrun notarytool store-credentials smoosher-notary --keychain "$MACOS_KEYCHAIN" \
    --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD"
bash "$script_dir/package-macos.sh" "$1" "$package"

bash "$script_dir/notarize-macos.sh" "$package" smoosher-notary
