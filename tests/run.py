"""Run GUI scenarios in fresh processes; optionally retain logs and JSON evidence."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent


def scenarios():
    for mode in ("normal", "callback-error", "setup-error"):
        yield f"smoke-{mode}", "smoke.lisp", {"WXLAMBDA_TEST_MODE": mode}
    yield "api", "api.lisp", {}
    for mode in ("payloads", "error-before-run", "error-in-loop", "error-handled-in-loop"):
        yield f"events-{mode}", "events.lisp", {"WXLAMBDA_EVENT_TEST_MODE": mode}
    yield "layout", "layout.lisp", {}
    yield "window-state", "window-state.lisp", {}
    yield "window-metadata", "window-metadata.lisp", {}
    yield "clos", "clos.lisp", {}
    yield "gauge", "gauge.lisp", {}
    yield "application-events", "application-events.lisp", {}
    yield "event-handlers", "event-handlers.lisp", {}
    yield "event-bindings", "event-bindings.lisp", {}
    yield "event-contexts", "event-contexts.lisp", {}
    yield "close-events", "close-events.lisp", {}
    yield "window-handler-stacks", "window-handler-stacks.lisp", {}
    yield "handler-chains", "handler-chains.lisp", {}
    yield "pending-events", "pending-events.lisp", {}
    yield "owned-events", "owned-events.lisp", {}
    yield "event-tags", "event-tags.lisp", {}
    yield "event-categories", "event-categories.lisp", {}
    yield "event-origins", "event-origins.lisp", {}
    yield "event-routing", "event-routing.lisp", {}
    yield "event-metadata", "event-metadata.lisp", {}
    yield "session-events", "session-events.lisp", {}
    yield "application-handler", "application-handler.lisp", {}
    yield "notify-events", "notify-events.lisp", {}
    for mode in ("normal", "error-before-run", "error-in-loop", "error-handled-in-loop"):
        yield f"controls-{mode}", "controls.lisp", {"WXLAMBDA_CONTROL_TEST_MODE": mode}
    for name in ("repl", "repl-eof", "repl-delayed", "repl-debugger", "live"):
        yield name, f"{name}.lisp", {}
    for mode in ("normal", "live"):
        yield f"form-{mode}", "form.lisp", {"WXLAMBDA_FORM_TEST_MODE": mode}
    for mode in ("normal", "error-before-run", "error-in-loop"):
        yield f"choice-{mode}", "choice.lisp", {"WXLAMBDA_CHOICE_TEST_MODE": mode}


def run_case(command, path, environment):
    """Return exit status and raw combined output, including failure evidence."""
    try:
        result = subprocess.run(command + ["--load", str(path)], cwd=ROOT,
                                env=environment, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30)
        return result.returncode, result.stdout, "passed" if result.returncode == 0 else "failed"
    except subprocess.TimeoutExpired as failure:
        return 124, (failure.output or b"") + b"\nScenario timed out after 30 seconds.\n", "timeout"
    except OSError as failure:
        return 127, f"Cannot launch Lisp: {failure}\n".encode(), "failed"


def main():
    command = sys.argv[1:] or ["sbcl", "--noinform", "--non-interactive",
                               "--load", str(Path.home() / "quicklisp/setup.lisp")]
    cases = [("platform", ROOT / "ci/platform.lisp", {}, "preflight")]
    cases += [(name, ROOT / "tests" / file, env, "gui") for name, file, env in scenarios()]
    report_dir = os.environ.get("WXLAMBDA_TEST_REPORT")
    report_dir = Path(report_dir) if report_dir else None
    if report_dir:
        # Refuse stale evidence: each run needs a fresh directory.
        report_dir.mkdir(parents=True, exist_ok=False)
    try:
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                         stderr=subprocess.DEVNULL, text=True).strip()
        dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT,
                                            stderr=subprocess.DEVNULL, text=True).strip())
    except (OSError, subprocess.CalledProcessError):
        commit, dirty = None, None
    evidence = {"schema_version": 1, "started_utc": datetime.now(timezone.utc).isoformat(),
                "platform": platform.platform(), "command": command,
                "commit": commit, "working_tree_dirty": dirty,
                "expected_gui_scenarios": len(cases) - 1,
                "completed_gui_scenarios": 0, "status": "running", "results": []}

    def save():
        if report_dir:
            (report_dir / "summary.json").write_text(
                json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    save()
    for name, path, overrides, kind in cases:
        print(f"Testing {name}: {command[0]}", flush=True)
        started = time.monotonic()
        code, output, status = run_case(command, path, {**os.environ, **overrides})
        print(output.decode("utf-8", errors="replace"), end="", flush=True)
        evidence["results"].append({"name": name, "kind": kind, "status": status,
                                    "exit_code": code, "seconds": time.monotonic() - started,
                                    "log": f"{name}.log"})
        if report_dir:
            (report_dir / f"{name}.log").write_bytes(output)
        if status == "passed" and kind == "gui":
            evidence["completed_gui_scenarios"] += 1
        evidence["status"] = "failed" if code else "running"
        save()
        if code:
            return code if code > 0 else 128 - code
    evidence["status"] = "passed"
    evidence["finished_utc"] = datetime.now(timezone.utc).isoformat()
    save()
    return 0


if __name__ == "__main__":
    sys.exit(main())
