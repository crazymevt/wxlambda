"""Offline SDK pinning/staging checks; no SDK installation or runtime calls."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("stage_webview2", ROOT / "ci/stage-webview2.py")
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class StageTests(unittest.TestCase):
    def archive(self):
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w") as archive:
            for name in sdk.FILES:
                archive.writestr(name, "test " + name)
        return output.getvalue()

    def test_bad_checksum_rejects_before_zip_handling(self):
        with self.assertRaisesRegex(ValueError, "SHA256"):
            sdk.contents(b"not the SDK")

    def test_verified_archive_stages_only_fixed_members_idempotently(self):
        raw = self.archive()
        with patch.object(sdk, "SHA256", hashlib.sha256(raw).hexdigest()):
            files = sdk.contents(raw)
        self.assertEqual(set(files), set(sdk.FILES))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sdk.stage(root, files)
            sdk.stage(root, files)
            self.assertEqual({str(p.relative_to(root)).replace("\\", "/") for p in root.rglob("*") if p.is_file()}, set(sdk.FILES))

    def test_existing_different_sdk_rejected_before_any_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            existing = root / sdk.FILES[-1]
            existing.parent.mkdir(parents=True)
            existing.write_bytes(b"other SDK")
            with self.assertRaisesRegex(ValueError, "Refusing"):
                sdk.stage(root, {name: b"new" for name in sdk.FILES})
            self.assertEqual([p for p in root.rglob("*") if p.is_file()], [existing])
            self.assertEqual(existing.read_bytes(), b"other SDK")

    def test_pinned_archive_when_available(self):
        path = os.environ.get("WXLAMBDA_WEBVIEW2_ARCHIVE")
        if not path:
            self.skipTest("Set WXLAMBDA_WEBVIEW2_ARCHIVE for the real pinned NuGet package")
        files = sdk.contents(Path(path).read_bytes())
        self.assertTrue(all(files.values()))


if __name__ == "__main__":
    unittest.main()
