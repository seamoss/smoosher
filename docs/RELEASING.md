# Releasing Smoosher

Version 1.3.1 adds the one-knob Destroy view while preserving Normal settings and older-session audio. Keep JUCE at 8.0.4 and preserve plugin/parameter identifiers. Changed binaries must be signed and notarized again before distribution.

See the [1.3.1 release notes](releases/1.3.1.md) for current release status and verification.

## 1.3.1 tester release

The 1.3.1 release is being prepared from the reviewed source changes. Local validation passed; CI and final artifact verification are pending. The published release notes will record the final results.

## Previous 1.3.0 tester release

[Smoosher 1.3.0 — Professional Help](https://github.com/seamoss/smoosher/releases/tag/v1.3.0) was published as a tester prerelease from commit `e099d837f78e65af935eaae9764472f181641b7e`. [Main CI](https://github.com/seamoss/smoosher/actions/runs/43123484927) and [release CI](https://github.com/seamoss/smoosher/actions/runs/43124441343) passed Mac/Windows builds, processor regressions, release-script tests, and AU validation. The tag run signed and notarized the universal installer, stapled its ticket, and created the release assets.

The downloaded assets passed SHA-256 checks. The Mac installer passed signature, stapled-ticket, Gatekeeper, embedded bundle-signature, architecture/version/identifier, minimum-OS metadata, and system install-path checks; Apple reported Accepted with no issues. Its signed VST3 passed pluginval strictness 5 with GUI tests skipped. The Windows ZIP passed CRC, x64 PE, 1.3.0 manifest, and bundled documentation checks, and retains the shipped 1.2 VST3 class identifiers. Broader DAW playback, clean-machine installation, and older-OS testing remain tester work. See the [release notes](releases/1.3.0.md).

## Previous 1.2.0 tester release

[Smoosher 1.2.0 — More Trouble](https://github.com/seamoss/smoosher/releases/tag/v1.2.0) was published as a tester prerelease from commit `cd2859a6a770f327078220fb8714d9bb8c59c3aa`. [Main CI](https://github.com/seamoss/smoosher/actions/runs/43116683138) and [release CI](https://github.com/seamoss/smoosher/actions/runs/43117552899) passed Mac/Windows builds, processor tests, release-script tests, and AU validation. The tag run signed and notarized the universal installer, stapled its ticket, and created the release assets.

The exact downloaded assets passed their checksums. The Mac installer passed signature, stapled-ticket, Gatekeeper, embedded bundle-signature, architecture/version, and install-path checks; Apple reported Accepted with no issues. Its signed VST3 passed pluginval strictness 5 with GUI tests skipped. The Windows ZIP passed CRC, x64 PE, 1.2.0 manifest, and bundled README/guide/screenshot checks. Wider DAW playback, clean-machine installation, and minimum-OS testing remain tester work. See the [release notes](releases/1.2.0.md).

## Previous 1.1.0 tester release

[Smoosher 1.1.0](https://github.com/seamoss/smoosher/releases/tag/v1.1.0) was built from commit `0ce8b2b092bd48321cb06d472e1656463cb92309` by [release CI run 43106218266](https://github.com/seamoss/smoosher/actions/runs/43106218266). Both platform builds and regression checks passed. CI validated the AU, signed the universal Mac payload and installer, obtained Apple notarization acceptance with no issues, stapled the ticket, and created the draft release.

Before publication, the downloaded assets passed their checksums; the Mac installer passed signature, stapled-ticket, Gatekeeper, and embedded bundle-signature verification. Its VST3 passed pluginval strictness 5 with GUI tests skipped. The Windows ZIP was checked for its x64 VST3 binary, 1.1.0 manifest, and installation instructions. The release is marked as a **prerelease** for broader DAW and OS testing.

The `macos-release` environment is configured with the five secrets and three variables described below, and access is restricted to `v*` tags. Credential values stay in GitHub's encrypted secrets and the local Keychain.

## Release outputs

- `Smoosher-1.3.1-macOS-universal.pkg`: Developer ID signed AU/VST3 installer, notarized by Apple with the ticket stapled. Installs to `/Library/Audio/Plug-Ins/` for all users. Targets macOS 11+ on arm64 and x86_64.
- `Smoosher-1.3.1-Windows.zip`: unsigned x64 VST3 bundle, README, guides, and screenshots.
- `SHA256SUMS.txt`: SHA-256 hashes of the final installer and ZIP, generated after stapling.

The workflow creates a **draft** GitHub release. It never publishes automatically or overwrites assets on an already-published release. Main/master, pull requests, and manual branch runs build without signing credentials. The unsigned Mac ZIP is an Actions artifact only.

## Apple account setup

Use the Apple Developer Program team that should appear as the publisher. The certificate display name follows that team's legal identity; it need not match the plugin's Helvete Sound branding.

1. Create a **Developer ID Application** certificate and a **Developer ID Installer** certificate. These are the certificates for distribution outside the Mac App Store. Use the Account Holder or an account with the appropriate Apple certificate access. Follow [Apple's Developer ID certificate instructions](https://developer.apple.com/help/account/certificates/create-developer-id-certificates).
2. Install each certificate on the Mac that generated its certificate signing request. In Keychain Access → My Certificates, confirm that each certificate expands to show its private key.
3. Export each certificate **with its private key** as a password-protected `.p12`: `application.p12` and `installer.p12`. Use the same strong export password for both; the pipeline uses one `MACOS_P12_PASSWORD`. A downloaded `.cer` alone does not contain the private key needed for signing.
4. Record the exact names shown in Keychain Access, including the Team ID suffix. Typically they look like `Developer ID Application: Your Name or Company (TEAMID)` and `Developer ID Installer: Your Name or Company (TEAMID)`.
5. Record your **Team ID** from Apple Developer membership details and the **Apple Account email** authorized for that team.
6. Generate an **app-specific password** at [account.apple.com](https://account.apple.com/) → Sign-In and Security → App-Specific Passwords. This pipeline uses Apple Account + Team ID + app-specific password for notarization, as supported by [Apple's command-line notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow). An App Store Connect API key is not required by this implementation.

Store private keys, exports, and passwords in your password manager or GitHub environment secrets. Do not commit them or paste them into issues or chat.

## Using certificates already in your local Keychain

If both Developer ID identities are already installed with their private keys, local signing does not require exports. Set their exact names and the keychain path, then package the universal build:

```sh
export MACOS_APPLICATION_IDENTITY='Developer ID Application: Your Name (TEAMID)'
export MACOS_INSTALLER_IDENTITY='Developer ID Installer: Your Name (TEAMID)'
export MACOS_KEYCHAIN="$HOME/Library/Keychains/login.keychain-db"
bash scripts/package-macos.sh build/Smoosher_artefacts/Release dist/Smoosher-1.3.1-macOS-universal.pkg
```

Signing keys and notarization credentials are separate. Save the latter with this interactive command; it prompts for your Apple Account, app-specific password, and Team ID without requiring them in shell history:

```sh
xcrun notarytool store-credentials smoosher-notary --keychain "$MACOS_KEYCHAIN"
```

Then submit, require acceptance, staple, and verify the same installer with:

```sh
bash scripts/notarize-macos.sh dist/Smoosher-1.3.1-macOS-universal.pkg smoosher-notary
```

This uses the same notarization script as CI. A successfully signed installer is **not yet notarized** until this step passes. Hosted GitHub runners still need the exports and environment setup below because they cannot access your Mac's keychain.

## GitHub environment

In the repository's Settings → Environments, create **`macos-release`**. Set its deployment branches/tags policy to allow release tags matching `v*`. Only trusted maintainers should be able to create these tags or change the release workflow.

Add the following **environment secrets**:

| Secret | Value |
| --- | --- |
| `MACOS_APPLICATION_P12_BASE64` | Base64 of the Developer ID Application `.p12`, including its private key |
| `MACOS_INSTALLER_P12_BASE64` | Base64 of the Developer ID Installer `.p12`, including its private key |
| `MACOS_P12_PASSWORD` | The shared password used for both `.p12` exports |
| `APPLE_ID` | Apple Account email authorized for your developer team |
| `APPLE_APP_SPECIFIC_PASSWORD` | The app-specific password created for notarization |

Add the following **environment variables**:

| Variable | Value |
| --- | --- |
| `MACOS_APPLICATION_IDENTITY` | Exact `Developer ID Application: … (TEAMID)` certificate name |
| `MACOS_INSTALLER_IDENTITY` | Exact `Developer ID Installer: … (TEAMID)` certificate name |
| `APPLE_TEAM_ID` | Your developer Team ID |

Using an authenticated GitHub CLI, these commands upload the exports without printing their contents. Run them from the repository after creating the environment; use the actual export paths:

```sh
base64 -i /path/to/application.p12 | gh secret set MACOS_APPLICATION_P12_BASE64 --env macos-release
base64 -i /path/to/installer.p12 | gh secret set MACOS_INSTALLER_P12_BASE64 --env macos-release
gh secret set MACOS_P12_PASSWORD --env macos-release
gh secret set APPLE_ID --env macos-release
gh secret set APPLE_APP_SPECIFIC_PASSWORD --env macos-release
gh variable set MACOS_APPLICATION_IDENTITY --env macos-release --body 'Developer ID Application: Your Name (TEAMID)'
gh variable set MACOS_INSTALLER_IDENTITY --env macos-release --body 'Developer ID Installer: Your Name (TEAMID)'
gh variable set APPLE_TEAM_ID --env macos-release --body 'TEAMID'
```

The three `gh secret set` commands without input pipes prompt for their values. Keep the exports outside the repository. The job imports them into a temporary keychain, grants signing tools access, stores notarization credentials there, and removes the keychain and exported files on exit. No signing secrets are needed for pull-request checks.

## Build and release flow

1. Update `project(Smoosher VERSION ...)` in `CMakeLists.txt`. It is the version source for bundles, installer, and filenames. Commit and merge the intended release changes; confirm ordinary CI passes.
2. Set up the environment above before pushing the release tag. For this release:

   ```sh
   git tag -a v1.3.1 -m 'Smoosher 1.3.1'
   git push origin v1.3.1
   ```

   Run these from the reviewed release commit. The workflow rejects a tag that differs from the CMake version.
3. The build job creates universal AU/VST3 binaries on `macos-15` with Xcode 16.4 and x64 VST3 on `windows-2022`. The Xcode selection is explicit so runner-default changes do not silently raise the deployment target. It verifies Mac architecture/version metadata and runs `auval -v aufx Smsh Smsh` on the Mac runner.
4. The signing job downloads the exact Mac build artifact and signs staged copies with a secure timestamp and hardened runtime. It builds a signed installer with fixed installation paths, disabled bundle relocation, and minimum OS/architecture requirements.
5. The job submits the installer to Apple, requires `Accepted`, downloads the notary log, staples and validates the ticket, then checks the installer signature and Gatekeeper assessment. Failed or pending notarization cannot produce a release artifact.
6. Only after Mac signing and both platform builds succeed does the workflow attach the installer, Windows ZIP, and final checksums to a draft release. Re-running updates an existing draft but refuses to modify a published release.
7. Download the draft assets and complete the checks below, then publish the draft in GitHub Releases.

To rerun against an existing tag, use Actions → Build and release Smoosher → Run workflow and select that tag, or:

```sh
gh workflow run build.yml --ref v1.3.1
```

## Before publishing

- Verify the downloaded files against `SHA256SUMS.txt` (`shasum -a 256 -c SHA256SUMS.txt` on macOS, with both files present).
- Check `pkgutil --check-signature Smoosher-1.3.1-macOS-universal.pkg`, `xcrun stapler validate Smoosher-1.3.1-macOS-universal.pkg`, and `spctl --assess --type install --verbose=2 Smoosher-1.3.1-macOS-universal.pkg`.
- Test the downloaded installer on a separate Mac or clean user/test system, including offline ticket validation. Confirm AU/VST3 land in the documented system directories and upgrading replaces those copies without relocating them.
- Verify the installed bundles with `codesign --verify --deep --strict /Library/Audio/Plug-Ins/Components/Smoosher.component` and the equivalent VST3 path. Run `auval -v aufx Smsh Smsh`.
- Load AU and VST3 in actual DAWs on Apple Silicon and Intel (or an Intel DAW under Rosetta as an additional check), and test Windows VST3. Test older supported macOS versions before claiming them as verified.
- Recall sessions saved with 1.0.x, 1.1.0, or 1.2.0 and compare settled audio. Confirm preset/modified labels, independent Mix, automation, meter/clip behavior, A/B recall, grouped Undo, and Ego Check learning/cancellation. Sessions before 1.2 must load with Ego Check off; 1.2 sessions must retain their A/B and Ego Check settings. Sessions before 1.3 must load with Stereo Link at 0% and the detector filter Off. Check Shape automation and Bad Decisions locks, inactive-slot generation, and Undo. Pre-1.3.1 sessions must reset Destroy off. Check Destroy automation/recall, clean zero, mode transitions, Undo, and returning to the saved Normal sound. Run the processor regression tests with `SMOOSHER_BUILD_TESTS=ON`.
- Run [pluginval](https://github.com/Tracktion/pluginval) at strictness 5 or higher against the built plugins for additional compatibility coverage. This is a manual release check, not currently a bundled CI dependency.
- Check the notary log artifact for warnings even when Apple accepts the submission, and review the generated release notes.

## Troubleshooting

**Missing credential:** the signing script names the missing secret/variable before importing keys. Check the `macos-release` environment, spelling, and tag access policy.

**Signing identity not found:** confirm the exact certificate name, certificate validity, and that the `.p12` contains the matching private key. Both exports must use the configured password.

**Notarization rejected:** download the `macOS-notarization-logs` Actions artifact and inspect `notary-log.json`. No release is drafted. See [Apple's notarization guidance](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

**Notarization timed out:** the script waits up to 45 minutes; Apple may continue processing afterward. Keep the submission ID in `notary-submission.json` and check it with `xcrun notarytool info` using your local credentials before deciding whether to resubmit. A pending result never counts as success.

**Duplicate plugin versions:** 1.0.0 manual installs may be in the user's Library. Move those copies aside before testing the new system-wide installation. The installer intentionally leaves per-user files alone.

**AU exists but `auval` cannot find it:** the registrar may retain stale discovery data even after a normal termination signal. Close DAWs, confirm the component is installed in one of the documented Components directories, and refresh your user's registrar:

```sh
killall -9 -u "$(id -un)" AudioComponentRegistrar 2>/dev/null || true
sleep 2
auval -v aufx Smsh Smsh
```

CI uses this same refresh before validation. On Apple Silicon with Rosetta installed, also check the Intel slice with `arch -x86_64 auval -v aufx Smsh Smsh`. Each invocation must exit successfully and report `AU VALIDATION SUCCEEDED`. A discovery failure is not a successful validation.
