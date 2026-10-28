#!/usr/bin/env python3
"""Verify coverage status persistence against extracted wxWidgets interface sources.

Usage: python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
No downloads, builds, GUI sessions or checkout mutations are performed.
"""
import contextlib
from collections import Counter
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(sys.argv.pop(1)).resolve()
spec = importlib.util.spec_from_file_location('coverage_inventory', ROOT / 'tools/coverage_inventory.py')
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)


class CoverageInventoryChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='wxlambda-coverage-check-')
        self.root = Path(self.temp.name)
        self.original_root = inventory.ROOT
        inventory.ROOT = self.root
        for name in ('docs/coverage/status.json', 'lisp/package.lisp', 'lisp/ffi.lisp', 'native/wxlambda.cpp'):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.status_path = self.root / 'docs/coverage/status.json'
        self.status = json.loads(self.status_path.read_text())

    def tearDown(self):
        inventory.ROOT = self.original_root
        self.temp.cleanup()

    def build(self):
        with contextlib.redirect_stdout(io.StringIO()):
            inventory.build(SOURCE)

    def save_status(self):
        self.status_path.write_text(json.dumps(self.status))

    def test_regeneration_preserves_every_mark_and_matches_checkout(self):
        self.build()
        output = self.root / 'docs/coverage'
        generated = [*output.glob('*.md'), output / 'manifest.json']
        first = {p.name: p.read_bytes() for p in generated}
        self.build()
        self.assertEqual(first, {p.name: p.read_bytes() for p in generated})
        for name, data in first.items():
            self.assertEqual(data, (ROOT / 'docs/coverage' / name).read_bytes(), name)
        manifest = json.loads((output / 'manifest.json').read_text())
        phases = {h['header']: h['phase'] for h in manifest['headers']}
        self.assertEqual(len(phases), 398)
        for record in self.status['declarations']:
            url = f'https://github.com/wxWidgets/wxWidgets/blob/v{inventory.VERSION}/interface/{record["header"]}'
            entry = f'- [{record["status"]}] [L{record["line"]}]({url}#L{record["line"]}) `{record["declaration"]}`'
            page = (output / f'phase-c{phases[record["header"]]}.md').read_text()
            self.assertEqual(page.count(entry), 1, record['symbol'])
            rendered = next(line for line in page.splitlines() if line.startswith(entry))
            self.assertIn(record['scope'], rendered)
            for name in record['lisp']:
                self.assertIn(f'`{name}`', rendered)
        index = (output / 'classes.md').read_text()
        for name, record in self.status['classes'].items():
            self.assertEqual(index.count(f'- [{record["status"]}] `{name}` —'), 1)
            self.assertIn(record['scope'], index)
        self.assertEqual(phases['wx/printdlg.h'], 5)
        for header in manifest['headers']:
            self.assertEqual(hashlib.sha256((SOURCE / header['header']).read_bytes()).hexdigest(), header['sha256'])
            self.assertEqual(header['entries'], header['tasks'] + header['context_entries'])

    def test_source_identifiers_and_documentation_links_are_preserved(self):
        def identifiers(text):
            return Counter(word for word in re.findall(r'[A-Za-z_][A-Za-z_0-9]*', text)
                           if word not in ('public', 'private', 'protected'))
        for header in SOURCE.glob('wx/**/*.h'):
            source = header.read_text(encoding='utf-8-sig')
            rendered = ' '.join(decl for _, decl in inventory.declarations(source))
            self.assertEqual(identifiers(inventory.uncomment(source)), identifiers(rendered), header)
        for page in [ROOT / 'README.md', ROOT / 'IMPLEMENTATION.md',
                     ROOT / 'docs/WXWIDGETS-COVERAGE.md',
                     *sorted((ROOT / 'docs/coverage').glob('*.md'))]:
            text = re.sub(r'`[^`]*`', 'CODE', page.read_text())
            for target in re.findall(r'\]\(([^)]+)\)', text):
                if '://' not in target and not target.startswith('#'):
                    self.assertTrue((page.parent / target.split('#')[0]).exists(), (page, target))

    def test_backlog_counts_match_rendered_checkboxes_and_manifest(self):
        self.build()
        output = self.root / 'docs/coverage'
        summary = (output / 'README.md').read_text()
        manifest = json.loads((output / 'manifest.json').read_text())
        total = Counter()
        for number in inventory.PHASES:
            text = (output / f'phase-c{number}.md').read_text()
            counts = Counter(re.findall(r'^- \[([x~ ])\]', text, re.M))
            counts['context'] = len(re.findall(r'^- \*\*Context\*\*:', text, re.M))
            headers = [row for row in manifest['headers'] if row['phase'] == number]
            tasks = sum(row['tasks'] for row in headers)
            self.assertEqual(tasks, sum(counts[mark] for mark in ('x', '~', ' ')))
            self.assertEqual(counts['context'], sum(row['context_entries'] for row in headers))
            row = next(line for line in summary.splitlines() if line.startswith(f'| [C{number}:'))
            values = [int(value.strip()) for value in row.split('|')[2:-1]]
            self.assertEqual(values, [len(headers), tasks, counts['x'], counts['~'],
                                      counts[' '], counts['context']])
            total.update(counts)
            for section in re.split(r'^## `', text, flags=re.M)[1:]:
                header = section.split('`', 1)[0]
                local = Counter(re.findall(r'^- \[([x~ ])\]', section, re.M))
                context = len(re.findall(r'^- \*\*Context\*\*:', section, re.M))
                row = next(line for line in summary.splitlines() if line.startswith(f'| [{header}]'))
                values = [int(value.strip()) for value in row.split('|')[2:-1]]
                self.assertEqual(values, [local['x'], local['~'], local[' '], context])
        row = next(line for line in summary.splitlines() if line.startswith('| Total |'))
        self.assertEqual([int(value.strip()) for value in row.split('|')[2:-1]],
                         [398, sum(total[mark] for mark in ('x', '~', ' ')),
                          total['x'], total['~'], total[' '], total['context']])
        self.assertIn('unsupported still prevent a full-coverage claim', summary)

    def test_multiline_macro_keeps_following_methods_separate(self):
        source = ("#define wxSTYLE (wxONE | \\" + "\n"
                  "    wxTWO)\n"
                  "class wxExample {\n"
                  "    wxString GetTitle() const;\n"
                  "    void SetTitle(const wxString& title);\n"
                  "};\n")
        entries = inventory.declarations(source)
        self.assertEqual(entries[0][0], 1)
        self.assertTrue(entries[0][1].startswith('#define wxSTYLE'))
        self.assertIn((3, 'class wxExample {'), entries)
        self.assertIn((4, 'wxString GetTitle() const;'), entries)
        self.assertIn((5, 'void SetTitle(const wxString& title);'), entries)

    def test_reviewed_bindings_have_persistent_status(self):
        required = {
            'wxFrame::wxFrame', 'wxPanel::wxPanel', 'wxButton::wxButton',
            'wxAppConsole::ProcessPendingEvents', 'wxAppConsole::HasPendingEvents',
            'wxAppConsole::IsMainLoopRunning',
            'wxEvtHandler::wxEvtHandler', 'wxEvtHandler::~wxEvtHandler',
            'wxEvtHandler::GetEvtHandlerEnabled', 'wxEvtHandler::SetEvtHandlerEnabled',
            'wxEvtHandler::Bind', 'wxEvtHandler::Unbind', 'wxEvtHandler::QueueEvent', 'wxEvent::GetId',
            'wxEvent::Clone', 'wxEvent::SetEventType', 'wxEvent::GetEventCategory', 'wxEVT_CATEGORY_ALL', 'wxEventCategory', 'wxEvent::GetEventObject', 'wxEvent::SetId', 'wxEvent::GetTimestamp', 'wxEvent::SetTimestamp', 'wxEvent::IsCommandEvent', 'wxEvent::GetEventType', 'wxEvent::GetSkipped', 'wxEvent::Skip',
            'wxEvent::ShouldPropagate', 'wxEvent::StopPropagation', 'wxEvent::ResumePropagation',
            'wxCloseEvent::wxCloseEvent', 'wxCloseEvent::CanVeto', 'wxCloseEvent::Veto',
            'wxCloseEvent::GetLoggingOff', 'wxCloseEvent::SetLoggingOff', 'wxEVT_END_SESSION', 'wxEVT_QUERY_END_SESSION', 'wxCloseEvent::GetVeto', 'wxCloseEvent::SetCanVeto', 'wxWindow::Close',
            'wxAppConsole::GetInstance', 'wxNewEventType', 'wxEvtHandler::ProcessEvent', 'wxCommandEvent::wxCommandEvent',
            'wxNotifyEvent::wxNotifyEvent', 'wxNotifyEvent::Veto', 'wxNotifyEvent::Allow', 'wxNotifyEvent::IsAllowed',
            'wxTextCtrl::wxTextCtrl', 'wxStaticText::wxStaticText',
            'wxCheckBox::wxCheckBox', 'wxChoice::wxChoice',
            'wxGauge::wxGauge', 'wxGauge::GetValue', 'wxGauge::SetValue',
            'wxGauge::GetRange', 'wxGauge::SetRange', 'wxGauge::IsVertical', 'wxGauge::Pulse',
            'wxTextEntry::SetValue', 'wxTextEntry::ChangeValue', 'wxTextEntry::GetValue',
            'wxCheckBox::GetValue', 'wxCheckBox::SetValue',
            'wxChoice::GetSelection', 'wxChoice::SetSelection',
            'wxStaticText::SetLabel', 'wxControl::GetLabel',
            'wxWindow::Show', 'wxWindow::Destroy', 'wxWindow::Layout',
            'wxWindow::GetPosition', 'wxWindow::GetSize',
            'wxWindow::IsShown', 'wxWindow::Enable', 'wxWindow::IsEnabled',
            'wxWindow::GetName', 'wxWindow::SetName',
            'wxTopLevelWindow::GetTitle', 'wxTopLevelWindow::SetTitle',
            'wxSizer::Add', 'wxSizer::AddSpacer', 'wxSizer::AddStretchSpacer',
        }
        self.assertTrue(required <= {record['symbol'] for record in self.status['declarations']})
        self.assertEqual(set(self.status['classes']), {
            'wxFrame', 'wxPanel', 'wxButton', 'wxTextCtrl', 'wxStaticText',
            'wxCheckBox', 'wxChoice', 'wxGauge', 'wxBoxSizer', 'wxWindow', 'wxTopLevelWindow',
            'wxAppConsole', 'wxApp', 'wxEvtHandler', 'wxEvent', 'wxCloseEvent', 'wxNotifyEvent', 'wxCommandEvent',
        })

    def test_compiler_scaffolding_is_context_but_public_macros_are_tasks(self):
        self.build()
        for page in (self.root / 'docs/coverage').glob('phase-c*.md'):
            self.assertNotRegex(page.read_text(), r'(?m)^- \[.\].* `#\s*(?:if|ifdef|ifndef|elif|else|endif|include|pragma|undef)\b')
        self.assertTrue(inventory.is_context('#define _WX_WIDGET_H_', {'_WX_WIDGET_H_'}))
        self.assertFalse(inventory.is_context('#define wxPUBLIC_API(arg)', set()))
        self.assertTrue(inventory.is_context('return value;', set()))
        self.assertFalse(inventory.is_context('wxString GetValue() const;', set()))

    def test_stale_signature_is_rejected_before_generated_files_are_written(self):
        self.status['declarations'][0]['declaration'] += ' changed'
        self.save_status()
        with self.assertRaisesRegex(ValueError, 'Stale declaration'):
            self.build()
        self.assertEqual(list((self.root / 'docs/coverage').glob('phase-c*.md')), [])

    def test_duplicate_mapping_is_rejected(self):
        self.status['declarations'].append(self.status['declarations'][0])
        self.save_status()
        with self.assertRaisesRegex(ValueError, 'Duplicate status'):
            self.build()

    def test_missing_class_is_rejected(self):
        self.status['classes']['wxNotARealClass'] = self.status['classes']['wxButton']
        self.save_status()
        with self.assertRaisesRegex(ValueError, 'Stale class'):
            self.build()

    def test_unknown_lisp_export_is_rejected(self):
        self.status['declarations'][0]['lisp'] = ['not-an-export']
        self.save_status()
        with self.assertRaisesRegex(ValueError, 'Unknown Lisp'):
            self.build()

    def test_unknown_native_mapping_is_rejected(self):
        self.status['declarations'][0]['native'] = ['wxl_not_a_binding']
        self.save_status()
        with self.assertRaisesRegex(ValueError, 'Unknown native'):
            self.build()

    def test_overlapping_phase_assignment_is_rejected(self):
        original = inventory.GROUPS[3]
        try:
            inventory.GROUPS[3] += ' printdlg'
            with self.assertRaisesRegex(ValueError, 'multiple phases'):
                self.build()
        finally:
            inventory.GROUPS[3] = original


if __name__ == '__main__':
    unittest.main()
