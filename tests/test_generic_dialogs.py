"""Check private symbol isolation and optional exact upstream regeneration."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("generic_dialogs", ROOT / "tools/generic_dialogs.py")
dialogs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dialogs)


class GenericDialogTests(unittest.TestCase):
    def test_private_classes_do_not_import_core_or_define_native_proxy(self):
        folder = ROOT / "native/generic-dialogs"
        for name in ("filedlgg.h", "filedlgg.cpp", "dirdlgg.h", "dirdlgg.cpp"):
            text = (folder / name).read_text()
            self.assertNotIn("WXDLLIMPEXP_CORE WxlGeneric", text)
            self.assertNotIn("WXDLLIMPEXP_FWD_CORE WxlGeneric", text)
            self.assertNotIn("#ifdef wxHAS_GENERIC_FILEDIALOG", text)
            self.assertNotIn("wxIMPLEMENT_DYNAMIC_CLASS(wxFileDialog", text)
            self.assertIn("Licence:     wxWindows licence", text)
            self.assertIn("WxlGeneric", text)
        self.assertEqual((folder / "LICENCE.txt").read_bytes().rstrip(b"\r\n"),
                         (ROOT / "docs/coverage/UPSTREAM-LICENCE.txt").read_bytes().rstrip(b"\r\n"))

    def test_unknown_source_hash_rejects(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / next(iter(dialogs.SOURCES))
            path.parent.mkdir(parents=True)
            path.write_text("different version")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                dialogs.generate(root)

    def test_original_classes_in_foreign_headers_are_not_globally_redefined(self):
        self.assertNotIn("#define wxGeneric", (ROOT / "native/wxlambda.cpp").read_text())
        source = (ROOT / "native/generic-dialogs/filedlgg.h").read_text()
        self.assertIn("WXDLLIMPEXP_FWD_CORE wxGenericFileCtrl", source)

    def test_pinned_upstream_regeneration_when_available(self):
        root = os.environ.get("WXLAMBDA_GENERIC_DIALOG_SOURCE")
        if not root:
            self.skipTest("Set WXLAMBDA_GENERIC_DIALOG_SOURCE for pinned upstream regeneration")
        for path, source in dialogs.generate(Path(root)).items():
            self.assertEqual(path.read_text(), source)


if __name__ == "__main__":
    unittest.main()
