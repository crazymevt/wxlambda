"""Focused documentation-tool checks, independent of GUI/native dependencies."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import re
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('docset_tool', ROOT / 'tools/docset.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class DocsetChecks(unittest.TestCase):
    def test_reader_content_excludes_agent_instructions_and_local_paths(self):
        self.assertTrue({'AGENTS.md', 'CLAUDE.md'}.isdisjoint(p.name for p in tool.sources()))
        manifest, entries = tool.content()
        content = tool.sexp([manifest, entries])
        self.assertNotIn('guide/AGENTS.md', content)
        self.assertNotIn('guide/CLAUDE.md', content)
        self.assertNotIn('/Users/', content)
        self.assertNotIn('.local/bin/quiet-run.sh', content)

    def test_missing_public_reference_fails(self):
        with patch.object(tool, 'exports', return_value=[*tool.exports(), 'missing-reference']):
            with self.assertRaisesRegex(ValueError, 'every public export'):
                tool.content()

    def test_guide_links_resolve_to_docset_entries(self):
        page = ROOT / 'README.md'
        links = {(ROOT / 'docs/DOCSET.md').resolve(): 'guide/docs/DOCSET.md'}
        blocks = tool.markdown('[Docset](docs/DOCSET.md) and [source](native/wxlambda.cpp)', page, links)
        self.assertIn(':ref', tool.sexp(blocks))
        self.assertIn('guide/docs/DOCSET.md', tool.sexp(blocks))
        self.assertNotIn(':link', tool.sexp(blocks))

    def test_lisp_string_and_lambda_list_handling(self):
        source = '(window &optional (title "λ (test) \\\"quoted\\\"")) trailing'
        self.assertEqual(tool.balanced(source, 0), source[:-9])
        self.assertEqual(tool.sexp('λ "quoted" \\'), '"λ \\"quoted\\" \\\\"')
        with self.assertRaisesRegex(ValueError, 'Unclosed'):
            tool.markdown('```lisp\n(unfinished)')

    def test_build_is_deterministic_and_stale_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'wxlambda'
            command = ['python3', str(ROOT / 'tools/docset.py')]
            def run(mode):
                return subprocess.run([*command, mode, '--output', str(output)],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            result = run('build')
            self.assertEqual(result.returncode, 0, result.stdout.decode())
            hashes = {n: hashlib.sha256((output / n).read_bytes()).digest() for n in tool.FILES}
            result = run('build')
            self.assertEqual(result.returncode, 0, result.stdout.decode())
            self.assertEqual(hashes, {n: hashlib.sha256((output / n).read_bytes()).digest() for n in tool.FILES})
            result = run('check')
            self.assertEqual(result.returncode, 0, result.stdout.decode())
            entries = output / 'entries.sexp'
            # Change an entry payload without corrupting syntax or byte offsets.
            data = entries.read_text(encoding='utf-8')
            entries.write_text(data.replace('Load the native bridge', 'Skip the native bridge', 1), encoding='utf-8')
            result = run('check')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'assertion', result.stdout.lower())
            self.assertEqual(run('build').returncode, 0)
            manifest = output / 'manifest.sexp'
            manifest.write_text(re.sub(r':version "[^"]*"', ':version "STALE"', manifest.read_text(encoding='utf-8')), encoding='utf-8')
            result = run('check')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'assertion', result.stdout.lower())


if __name__ == '__main__':
    unittest.main()
