"""Structural regressions for configure probes; real platform runs remain required."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ConfigureProbeTests(unittest.TestCase):
    def test_gtk_probe_runtime_can_find_pinned_wx(self):
        workflow = (ROOT / '.github/workflows/ci.yml').read_text()
        step = workflow.split('- name: Build experimental bridge and native regressions', 1)[1]
        step = step.split('- name:', 1)[0]
        self.assertLess(step.index('export LD_LIBRARY_PATH='), step.index('cmake -S .'))

    def test_png_fixtures_do_not_assume_macos_tmp(self):
        for relative in ('tests/images.lisp', 'tests/drawing-canvas.lisp',
                         'examples/drawing-monitor.lisp', 'tests/client-data-native.cpp',
                         'tests/images-native.inc', 'tests/canvas-native.inc', 'tests/aui-native.inc'):
            self.assertNotIn('/private/tmp', (ROOT / relative).read_text(), relative)

    def test_all_isolated_checks_inherit_wx_definitions(self):
        source = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('foreach(definition IN LISTS wxWidgets_DEFINITIONS)', source)
        self.assertIn('"-D${definition}"', source)
        blocks = source.split('cmake_push_check_state(RESET)')[1:]
        self.assertEqual(3, len(blocks))
        for block in blocks:
            scoped = block.split('cmake_pop_check_state()', 1)[0]
            self.assertIn('set(CMAKE_REQUIRED_DEFINITIONS ${WXLAMBDA_WX_PROBE_DEFINITIONS})', scoped)

    def test_backend_gates_remain_linked_and_executed(self):
        source = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('#include <wx/webview.h>\n#include <wx/msw/webview_edge.h>', source)
        self.assertIn('MSWGetWXLambdaCompletionGuardVersion()!=1', source)
        self.assertIn('check_cxx_source_runs(', source)
        self.assertIn('return seen==${WXLAMBDA_EXPECTED_WEBKITGTK_MASK} ? 0 : 1;', source)
        self.assertIn('-DCMAKE_TRY_COMPILE_CONFIGURATION=Release',
                      (ROOT / '.github/workflows/ci.yml').read_text())


if __name__ == '__main__':
    unittest.main()
