"""Verify fail-closed patching without fetching sources or launching a GUI."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("edge_patch", ROOT / "ci/patch-wx-edge.py")
edge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(edge)


class PatchTests(unittest.TestCase):
    def fixture(self, root, crlf=False):
        """Synthetic anchor fixture; real pinned-source integration is separate."""
        hashes = {}
        for name, changes in edge.REPLACEMENTS.items():
            raw = ("\n--- boundary ---\n".join(old for old, _ in changes)).encode()
            hashes[name] = hashlib.sha256(raw).hexdigest()
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw.replace(b"\n", b"\r\n") if crlf else raw)
        return hashes

    def apply(self, writes):
        for path, data in writes.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def test_exact_application_reversible_and_idempotent(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            hashes = self.fixture(root)
            with patch.object(edge, "HASHES", hashes):
                writes = edge.prepare(root)
                self.assertEqual(len(writes), 4)
                self.assertFalse((root / edge.REGISTRY).exists(), "prepare must not write")
                self.apply(writes)
                self.assertEqual(edge.prepare(root), {})
                source = (root / edge.SOURCE).read_text()
                self.assertNotIn("[this, clientData]", source)
                self.assertIn("[creationId, clientData]", source)
                self.assertIn("if (controller) controller->Close()", source)
                self.assertIn("if (FAILED(result) || !environment)", source)
                self.assertNotIn("this, &wxWebViewEdgeImpl::", source)
                self.assertIn("if (error == S_OK && result)", source)
                self.assertIn("if (SUCCEEDED(errorCode) && id)", source)

    def test_windows_crlf_checkouts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            hashes = self.fixture(root, crlf=True)
            with patch.object(edge, "HASHES", hashes):
                self.apply(edge.prepare(root))
                for name in (*edge.HASHES, edge.REGISTRY):
                    path = root / name
                    path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
                self.assertEqual(edge.prepare(root), {})

    def test_unknown_source_is_rejected_without_any_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            hashes = self.fixture(root)
            path = root / edge.SOURCE
            path.write_bytes(path.read_bytes() + b"\nunknown local edit\n")
            before = {p: p.read_bytes() for p in root.rglob("*.h")}
            with patch.object(edge, "HASHES", hashes):
                with self.assertRaises(ValueError):
                    edge.prepare(root)
            self.assertEqual(before, {p: p.read_bytes() for p in root.rglob("*.h")})
            self.assertFalse((root / edge.REGISTRY).exists())

    def test_partial_patch_or_changed_helper_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            hashes = self.fixture(root)
            with patch.object(edge, "HASHES", hashes):
                writes = edge.prepare(root)
                (root / edge.PUBLIC).write_bytes(writes[root / edge.PUBLIC])
                with self.assertRaises(ValueError):
                    edge.prepare(root)
                self.apply(writes)
                (root / edge.REGISTRY).write_bytes(b"different helper")
                with self.assertRaises(ValueError):
                    edge.prepare(root)

    def test_missing_helper_on_already_patched_tree_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            hashes = self.fixture(root)
            with patch.object(edge, "HASHES", hashes):
                self.apply(edge.prepare(root))
                (root / edge.REGISTRY).unlink()
                with self.assertRaises(ValueError):
                    edge.prepare(root)

    def test_pinned_upstream_integration_when_available(self):
        source = os.environ.get("WXLAMBDA_WX_EDGE_SOURCE")
        if not source:
            self.skipTest("Set WXLAMBDA_WX_EDGE_SOURCE for hash-pinned upstream integration")
        # Validation only; never modify the caller's source checkout.
        writes = edge.prepare(Path(source))
        self.assertIn(len(writes), (0, 4))


if __name__ == "__main__":
    unittest.main()
