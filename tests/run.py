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
