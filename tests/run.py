"""Run each single-session GUI test in a fresh Lisp process with a timeout."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
command = sys.argv[1:] or ["sbcl", "--noinform", "--non-interactive",
                           "--load", str(Path.home() / "quicklisp/setup.lisp")]
for mode in ("normal", "callback-error", "setup-error"):
    print(f"Testing {mode}: {command[0]}", flush=True)
    subprocess.run(command + ["--load", str(root / "tests/smoke.lisp")],
                   cwd=root, env={**os.environ, "WXLAMBDA_TEST_MODE": mode},
                   check=True, timeout=30)

print(f"Testing text and handler API: {command[0]}", flush=True)
subprocess.run(command + ["--load", str(root / "tests/api.lisp")],
               cwd=root, check=True, timeout=30)

for mode in ("payloads", "error-before-run", "error-in-loop", "error-handled-in-loop"):
    print(f"Testing events {mode}: {command[0]}", flush=True)
    subprocess.run(command + ["--load", str(root / "tests/events.lisp")],
                   cwd=root, env={**os.environ, "WXLAMBDA_EVENT_TEST_MODE": mode},
                   check=True, timeout=30)

print(f"Testing layout geometry: {command[0]}", flush=True)
subprocess.run(command + ["--load", str(root / "tests/layout.lisp")],
               cwd=root, check=True, timeout=30)
