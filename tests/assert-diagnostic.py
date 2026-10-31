"""Verify the private native assertion fixture fails promptly with useful stderr."""
import os
import subprocess
import sys

if os.name == "posix":
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

if len(sys.argv) != 2:
    raise SystemExit("Usage: assert-diagnostic.py NATIVE-TEST-EXECUTABLE")
result = subprocess.run(
    [sys.argv[1], "--assert-diagnostic"],
    env={**os.environ, "WXLAMBDA_TEST_ABORT_ON_ASSERT": "1"},
    capture_output=True, timeout=10,
)
if result.returncode == 0 or b"WXLAMBDA WX ASSERT: assert-fixture:17" not in result.stderr:
    sys.stderr.buffer.write(result.stdout + result.stderr)
    raise SystemExit("Native assertion diagnostic did not fail as expected")
print("PASS: native assertion diagnostic exits promptly with file/line metadata")
