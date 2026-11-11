#!/bin/bash
# Notarize a signed installer using credentials already saved in Keychain.
set -euo pipefail

if [[ $# -ne 2 || ! -f "$1" ]]; then
    echo "Usage: bash scripts/notarize-macos.sh <signed-installer.pkg> <keychain-profile>" >&2
    exit 1
fi
output_dir="$(cd "$(dirname "$1")" && pwd)"
package="$output_dir/$(basename "$1")"
notary_credentials=(--keychain-profile "$2")
if [[ -n "${MACOS_KEYCHAIN:-}" ]]; then
    notary_credentials+=(--keychain "$MACOS_KEYCHAIN")
fi

# Preserve the response (including submission ID on timeout) for diagnosis.
submit_exit=0
/usr/bin/xcrun notarytool submit "$package" "${notary_credentials[@]}" --wait --timeout 45m --output-format json \
    > "$output_dir/notary-submission.json" || submit_exit=$?
submission_id="$(python3 - "$output_dir/notary-submission.json" <<'PY'
import json
import sys
try:
    with open(sys.argv[1]) as source:
        print(json.load(source).get("id", ""))
except (ValueError, OSError):
    print("")
PY
)"
if [[ -n "$submission_id" ]]; then
    /usr/bin/xcrun notarytool log "$submission_id" "${notary_credentials[@]}" "$output_dir/notary-log.json" || true
fi
if [[ "$submit_exit" -ne 0 ]]; then
    echo "Notarization command failed or timed out; see notary-submission.json" >&2
    exit "$submit_exit"
fi
python3 - "$output_dir/notary-submission.json" <<'PY'
import json
import sys
with open(sys.argv[1]) as source:
    result = json.load(source)
if result.get("status") != "Accepted":
    raise SystemExit("Notarization was not accepted: " + str(result.get("status")))
PY
/usr/bin/xcrun stapler staple "$package"
/usr/bin/xcrun stapler validate "$package"
/usr/sbin/pkgutil --check-signature "$package"
/usr/sbin/spctl --assess --type install --verbose=2 "$package"
