"""Structural regressions for configure probes; real platform runs remain required."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ConfigureProbeTests(unittest.TestCase):
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
