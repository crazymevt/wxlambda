"""Exercise callbacks while switching dependency sources in fresh Lisp processes."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
command = sys.argv[1:] or ["sbcl", "--noinform", "--no-sysinit", "--no-userinit",
                           "--non-interactive"]
quicklisp = Path(os.environ.get("QUICKLISP_SETUP", Path.home() / "quicklisp/setup.lisp"))
if not quicklisp.is_file():
    raise SystemExit("Quicklisp setup missing; set QUICKLISP_SETUP to its setup.lisp")

# Check both the source registry and the compiled-output destination. A GUI
# callback test then catches incompatible cached CFFI callback definitions.
probe = r'''
(let* ((pinned (string= (uiop:getenv "WXLAMBDA_DEPENDENCIES") "pinned"))
       (tree (truename ".ci-deps/"))
       (source (asdf:system-source-directory "cffi"))
       (component (asdf:find-component "wxlambda" "lisp/api"))
       (output (first (asdf:output-files 'asdf:compile-op component))))
  (assert (eq pinned (not (null (uiop:subpathp source tree)))))
  (assert (eq pinned (not (null (uiop:subpathp output tree)))))
  (format t "PASS: ~A source ~A, compiled output ~A~%"
          (if pinned "pinned" "Quicklisp") source output))
'''
for mode, setup in (("pinned", root / "ci/setup.lisp"),
                    ("quicklisp", quicklisp),
                    ("pinned", root / "ci/setup.lisp")):
    print(f"Testing cache switch: {command[0]} / {mode}", flush=True)
    subprocess.run(command + ["--load", str(setup), "--load", str(root / "tests/api.lisp"),
                              "--eval", probe],
                   cwd=root, env={**os.environ, "WXLAMBDA_DEPENDENCIES": mode},
                   check=True, timeout=60)
