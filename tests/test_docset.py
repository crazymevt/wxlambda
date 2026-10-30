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
    def test_coverage_summary_uses_docset_tables(self):
        path = ROOT / 'docs/coverage/README.md'
        blocks = tool.markdown(path.read_text(), path, {})
        tables = [block for block in blocks if block[0] == ':table']
        self.assertEqual(len(tables), 8)  # phase totals plus seven header backlogs
        self.assertEqual(len(tables[0][4]), 8)  # C1-C7 and total
        self.assertEqual(sum(len(table[4]) for table in tables[1:]), 398)
        self.assertIn('"guide/docs/coverage/README.md"', tool.sexp(tool.content()[1]))

    def test_markdown_tables_preserve_inline_code_escaped_pipes_and_links(self):
        text = '| Name | Detail |\n| :--- | ---: |\n| `a|b` | left\\|right |\n| [Guide](DOCSET.md) | |'
        path = ROOT / 'docs/example.md'
        blocks = tool.markdown(text, path, {(ROOT / 'docs/DOCSET.md').resolve(): 'guide/docs/DOCSET.md'})
        self.assertEqual(blocks, [[tool.key('table'), tool.key('header'), [['Name'], ['Detail']],
                                  tool.key('rows'), [[[[tool.key('code'), 'a|b']], ['left|right']],
                                                     [[[tool.key('ref'), 'guide/docs/DOCSET.md', 'Guide']], []]]]])
        self.assertEqual(tool.markdown('```text\n' + text + '\n```')[0][0], ':code-block')
        with self.assertRaisesRegex(ValueError, 'column mismatch'):
            tool.markdown('| A | B |\n| --- | --- |\n| missing |')

    def test_guide_and_example_names_do_not_shadow_public_symbols(self):
        _, entries = tool.content()
        public = {name.casefold() for name in tool.exports()}
        for entry in entries:
            record = dict(zip(entry[::2], entry[1::2]))
            if record.get(':package') == 'WXLAMBDA':
                continue
            self.assertTrue(public.isdisjoint(name.casefold() for name in record[':names']))
        text = tool.sexp(entries)
        self.assertIn('"guide/docs/GAUGE.md"', text)
        self.assertIn('"example/examples/gauge.lisp"', text)

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

    def test_clos_signatures_keep_defaults_without_specializers(self):
        self.assertEqual(tool.method_signature('((window window) &optional (show t))'),
                         '(window &optional (show t))')
        self.assertEqual(tool.method_signature('((parent window) (child window) &key (border 8) expand)'),
                         '(parent child &key (border 8) expand)')
        manifest, entries = tool.content()
        name = tool.sexp(entries)
        self.assertIn('(wxlambda:show-window window &optional (show t))', name)
        self.assertIn(':kind :generic-function', name)
        self.assertIn(':kind :class', name)

    def test_construction_and_setf_are_documented_and_indexed(self):
        manifest, entries = tool.content()
        text = tool.sexp(entries)
        self.assertIn('"Construction"', text)
        self.assertIn('"SETF"', text)
        self.assertIn('"(setf frame-title)"', text)
        self.assertIn('"(setf text)"', text)
        self.assertIn('"(setf label-text)"', text)
        self.assertNotIn('"api/get-text"', text)
        self.assertNotIn('"api/get-label"', text)
        self.assertIn("(make-instance 'wxlambda:button", text)
        self.assertNotIn('"api/set-frame-title"', text)
        self.assertNotIn('"api/set-text"', text)

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
            data = entries.read_bytes()
            self.assertIn(b'Load the native bridge', data)
            changed = data.replace(b'Load the native bridge', b'Skip the native bridge', 1)
            self.assertEqual(len(data), len(changed))
            self.assertEqual(data.count(b'\r\n'), changed.count(b'\r\n'))
            entries.write_bytes(changed)
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
