"""No installer execution: reject bad payloads and exercise bounded host fallback."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, Mock

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("sbcl_download", ROOT / "ci/download-sbcl.py")
sbcl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sbcl)
PAYLOAD = b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1test fixture, not executable"


class DownloadTests(unittest.TestCase):
    def test_html_then_valid_mirror(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "sbcl.msi"
            calls = []
            def fetch(url):
                self.assertFalse(destination.exists())
                calls.append(url)
                return b"<html>download challenge</html>" if len(calls) == 1 else PAYLOAD
            with patch.object(sbcl, "SHA256", hashlib.sha256(PAYLOAD).hexdigest()):
                sbcl.download(destination, fetch)
            self.assertEqual(calls, list(sbcl.URLS))
            self.assertEqual(destination.read_bytes(), PAYLOAD)
            self.assertEqual(list(Path(temporary).iterdir()), [destination])

    def test_all_invalid_responses_leave_no_installer(self):
        calls = []
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "sbcl.msi"
            with self.assertRaisesRegex(ValueError, "pin unchanged"):
                sbcl.download(destination, lambda url: calls.append(url) or b"not MSI")
            self.assertEqual(calls, list(sbcl.URLS))
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_existing_destination_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "sbcl.msi"
            destination.write_bytes(b"existing")
            with self.assertRaisesRegex(ValueError, "refusing overwrite"):
                sbcl.download(destination, lambda _: self.fail("Must not fetch"))
            self.assertEqual(destination.read_bytes(), b"existing")

    def test_signature_required_even_if_hash_matches(self):
        data = b"HTML with a contrived matching test pin"
        with patch.object(sbcl, "SHA256", hashlib.sha256(data).hexdigest()):
            with self.assertRaises(ValueError):
                sbcl.verified(data)

    def test_powershell_defense_pin_matches_downloader(self):
        self.assertIn(f"$expected = '{sbcl.SHA256}'", (ROOT / "ci/install-sbcl.ps1").read_text())

    def test_network_failure_falls_back(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "sbcl.msi"
            fetch = Mock(side_effect=[ConnectionError("test network error"), PAYLOAD])
            with patch.object(sbcl, "SHA256", hashlib.sha256(PAYLOAD).hexdigest()):
                sbcl.download(destination, fetch)
            self.assertEqual(fetch.call_count, 2)
            self.assertEqual(destination.read_bytes(), PAYLOAD)

    def test_oversize_rejects_even_with_matching_pin(self):
        with patch.object(sbcl, "SHA256", hashlib.sha256(PAYLOAD).hexdigest()), patch.object(sbcl, "LIMIT", 8):
            with self.assertRaises(ValueError):
                sbcl.verified(PAYLOAD)

    def test_write_failure_cleans_temporary_file(self):
        original = sbcl.tempfile.NamedTemporaryFile
        def failing_file(*args, **kwargs):
            file = original(*args, **kwargs)
            file.write = Mock(side_effect=OSError("test write failure"))
            return file
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "sbcl.msi"
            with patch.object(sbcl, "SHA256", hashlib.sha256(PAYLOAD).hexdigest()), patch.object(sbcl.tempfile, "NamedTemporaryFile", side_effect=failing_file):
                with self.assertRaisesRegex(OSError, "write failure"):
                    sbcl.download(destination, lambda _: PAYLOAD)
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_real_pinned_installer_when_available(self):
        path = os.environ.get("WXLAMBDA_SBCL_MSI")
        if not path:
            self.skipTest("Set WXLAMBDA_SBCL_MSI for real upstream MSI verification")
        data = Path(path).read_bytes()
        self.assertIs(sbcl.verified(data), data)


if __name__ == "__main__":
    unittest.main()
