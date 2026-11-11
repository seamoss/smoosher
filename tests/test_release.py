"""Exercise release gates without certificates, Apple requests, or keychain changes."""
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[1]


class ReleaseTests(unittest.TestCase):
    def test_version_tag_must_match_cmake(self):
        result = subprocess.run(
            ["bash", str(REPO / "scripts/release-version.sh"), "v0.0.0"],
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("to match CMakeLists.txt", result.stderr)

    def test_missing_credentials_stop_before_using_tools(self):
        result = subprocess.run(
            ["bash", str(REPO / "scripts/release-macos.sh"), "missing", "unused"],
            env={"PATH": os.environ["PATH"]}, capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing required release credential", result.stderr)

    def run_notary_case(self, status, exit_code=0, empty_response=False):
        # Substitute only external service/tool boundaries in a temporary copy.
        # The real release control flow, JSON checks, and cleanup traps execute.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            scripts = root / "scripts"
            scripts.mkdir()
            shutil.copy(REPO / "CMakeLists.txt", root)
            shutil.copy(REPO / "scripts/release-version.sh", scripts)
            (scripts / "package-macos.sh").write_text(
                '#!/bin/bash\nset -eu\ntouch "$2"\n'
            )
            tool = root / "mock-tool"
            tool.write_text("""#!/usr/bin/env python3
import json, os, pathlib, sys
args = sys.argv[1:]
with open(os.environ["MOCK_LOG"], "a") as log:
    log.write(json.dumps(args) + "\\n")
if args[:2] == ["security", "list-keychains"] and "-s" not in args:
    print('    "/original/login.keychain-db"')
elif args[:3] == ["xcrun", "notarytool", "submit"]:
    if os.environ["MOCK_EMPTY"] != "1":
        print(json.dumps({"id": "test-submission", "status": os.environ["MOCK_STATUS"]}))
    sys.exit(int(os.environ["MOCK_EXIT"]))
elif args[:3] == ["xcrun", "notarytool", "log"]:
    pathlib.Path(args[-1]).write_text("{}")
""")
            tool.chmod(0o755)
            for name in ("release-macos.sh", "notarize-macos.sh"):
                source = (REPO / "scripts" / name).read_text()
                for executable in ("/usr/bin/security", "/usr/bin/xcrun",
                                   "/usr/sbin/pkgutil", "/usr/sbin/spctl"):
                    source = source.replace(
                        executable, '"{}" {}'.format(tool, Path(executable).name)
                    )
                (scripts / name).write_text(source)
            log = root / "commands.jsonl"
            environment = dict(os.environ)
            environment.update({
                "MACOS_APPLICATION_P12_BASE64": base64.b64encode(b"fake").decode(),
                "MACOS_INSTALLER_P12_BASE64": base64.b64encode(b"fake").decode(),
                "MACOS_P12_PASSWORD": "test-only",
                "MACOS_APPLICATION_IDENTITY": "test-only",
                "MACOS_INSTALLER_IDENTITY": "test-only",
                "APPLE_ID": "test-only",
                "APPLE_TEAM_ID": "test-only",
                "APPLE_APP_SPECIFIC_PASSWORD": "test-only",
                "RUNNER_TEMP": str(root),
                "MOCK_LOG": str(log),
                "MOCK_STATUS": status,
                "MOCK_EXIT": str(exit_code),
                "MOCK_EMPTY": "1" if empty_response else "0",
            })
            result = subprocess.run(
                ["bash", str(scripts / "release-macos.sh"), "payload", str(root / "dist")],
                env=environment, capture_output=True, text=True,
            )
            commands = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertTrue(any(c[:2] == ["security", "delete-keychain"] for c in commands))
            self.assertIn(
                ["security", "list-keychains", "-d", "user", "-s", "/original/login.keychain-db"],
                commands,
            )
            self.assertEqual(list(root.glob("smoosher-signing.*")), [])
            return result, commands

    def test_accepted_submission_is_stapled_and_assessed(self):
        result, commands = self.run_notary_case("Accepted")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(any(c[:3] == ["xcrun", "stapler", "staple"] for c in commands))
        self.assertTrue(any(c[:3] == ["xcrun", "stapler", "validate"] for c in commands))
        self.assertTrue(any(c[:2] == ["spctl", "--assess"] for c in commands))

    def test_rejection_and_pending_status_never_reach_stapling(self):
        for status in ("Invalid", "In Progress", ""):
            with self.subTest(status=status):
                result, commands = self.run_notary_case(status)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(any(c[:2] == ["xcrun", "stapler"] for c in commands))
                self.assertFalse(any(c[0] == "spctl" for c in commands))

    def test_timeout_or_command_failure_never_reaches_stapling(self):
        for empty_response in (False, True):
            with self.subTest(empty_response=empty_response):
                result, commands = self.run_notary_case("In Progress", 1, empty_response)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("failed or timed out", result.stderr)
                self.assertFalse(any(c[:2] == ["xcrun", "stapler"] for c in commands))


if __name__ == "__main__":
    unittest.main()
