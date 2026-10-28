"""Verify the macOS package outside the checkout with development paths denied."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, nargs="?", default=ROOT / "dist/wxlambda-form")
    parser.add_argument("--report", type=Path, default=ROOT / "evidence/packaging/macos-sbcl")
    args = parser.parse_args()
    if sys.platform != "darwin" or not shutil.which("sandbox-exec"):
        parser.error("This test requires macOS sandbox-exec; it does not silently skip isolation")
    report = args.report.absolute()
    report.mkdir(parents=True, exist_ok=False)
    results = {"status": "running", "package": str(args.package.absolute()), "checks": []}

    def save():
        (report / "summary.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    save()
    try:
        with tempfile.TemporaryDirectory(prefix="wxlambda-relocation-") as temporary:
            temporary = Path(temporary)
            bundle = temporary / "Application folder with spaces"
            shutil.copytree(args.package, bundle)
            manifest = json.loads((bundle / "manifest.json").read_text())
            for record in manifest["native_files"]:
                assert hashlib.sha256((bundle / record["file"]).read_bytes()).hexdigest() == record["sha256"]
            assert hashlib.sha256((bundle / "app.core").read_bytes()).hexdigest() == manifest["core_sha256"]
            (report / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            home = temporary / "empty-home"
            home.mkdir()
            (home / ".sbclrc").write_text('(error "User init must not be loaded")\n', encoding="utf-8")
            cwd = temporary / "unrelated-working-directory"
            cwd.mkdir()
            denied = [ROOT, Path("/opt/homebrew"), Path("/usr/local"),
                      Path.home() / "quicklisp", Path.home() / ".cache/common-lisp"]
            profile = '(version 1) (allow default) (deny file-read* ' + ' '.join(
                f'(subpath {json.dumps(str(path), ensure_ascii=False)})' for path in denied) + ')'
            environment = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": str(home),
                           "SBCL_HOME": str(home), "XDG_CACHE_HOME": str(home / "cache"),
                           "LANG": "en_US.UTF-8", "TMPDIR": str(temporary)}
            command = ["/usr/bin/sandbox-exec", "-p", profile, str(bundle / "wxlambda-form")]
            (report / "sandbox.sb").write_text(profile + "\n", encoding="utf-8")
            control = subprocess.run(
                ["/usr/bin/sandbox-exec", "-p", profile, "/usr/bin/head", "-c", "1",
                 str(ROOT / "wxlambda.asd")], cwd=cwd, env=environment,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
            (report / "development-path-denied.log").write_bytes(control.stdout)
            denied_read = control.returncode != 0 and b"Operation not permitted" in control.stdout
            results["checks"].append({"name": "development-path-denied", "exit_code": control.returncode,
                                      "status": "passed" if denied_read else "failed"})
            save()
            if not denied_read:
                raise RuntimeError("Sandbox did not deny the development-tree read control")

            def check(name, expect_success, arguments=("--self-test",), input_data=None, missing=None):
                try:
                    process = subprocess.run(command + list(arguments), cwd=cwd, env=environment,
                                             input=input_data,
                                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                             timeout=30)
                    code, output = process.returncode, process.stdout
                except subprocess.TimeoutExpired as failure:
                    code, output = 124, (failure.output or b"") + b"\nTimed out\n"
                (report / f"{name}.log").write_bytes(output)
                passed = b"PASS: packaged" in output
                if expect_success:
                    expected = code == 0 and passed and b"debugger invoked" not in output
                else:
                    expected = (code == 1 and b"Application failed:" in output and not passed
                                and missing.name.encode() in output)
                results["checks"].append({"name": name, "exit_code": code, "expected_success": expect_success,
                                          "status": "passed" if expected else "failed"})
                save()
                if not expected:
                    raise RuntimeError(f"{name} failed (exit {code}); see {report / (name + '.log')}")

            check("relocated", True)
            check("relocated-repl", True, arguments=("--repl",), input_data=(
                '(assert (null sb-ext:*invoke-debugger-hook*))\n'
                '(wxlambda:queue-click wxlambda-form:*reopen*)\n'
                '(assert (string= "Ada" (wxlambda:text wxlambda-form:*name*)))\n'
                '(wxlambda-form:close-window)\n'
                '(format t "PASS: packaged REPL recreation and exit~%")\n'
                '(wxlambda:stop)\n').encode())
            # Removing staged libraries must fail rather than use the checkout
            # bridge or Homebrew libraries that are still installed on this Mac.
            for name, library in (
                    ("missing-bridge", bundle / "lib/libwxlambda.dylib"),
                    ("missing-wx", next((bundle / "lib").glob("libwx_*core*.dylib")))):
                hidden = library.with_suffix(".withheld")
                library.rename(hidden)
                try:
                    check(name, False, missing=library)
                finally:
                    hidden.rename(library)
            results["denied_paths"] = [str(path) for path in denied]
            results["empty_home"] = True
            results["relocated_path_contains_spaces"] = True
        results["status"] = "passed"
        save()
    except BaseException:
        results["status"] = "failed"
        save()
        raise
    print(f"PASS: relocation without checkout/Quicklisp/Homebrew, callbacks, cleanup, missing-library failures; {report}")


if __name__ == "__main__":
    main()
