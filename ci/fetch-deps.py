"""Fetch the pinned Lisp test dependencies into .ci-deps/, without Quicklisp."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def git(directory, *arguments, capture=False):
    return subprocess.run(
        ["git", "-C", str(directory), *arguments],
        check=True, text=True, stdout=subprocess.PIPE if capture else None,
        timeout=120,
    ).stdout


def main():
    dependencies = json.loads((ROOT / "ci/lisp-dependencies.json").read_text())
    destination = ROOT / ".ci-deps"
    destination.mkdir(exist_ok=True)
    for name, dependency in dependencies.items():
        target = destination / name
        revision = dependency["revision"]
        if not target.exists():
            # Publish only complete checkouts; an interrupted fetch is retryable.
            with tempfile.TemporaryDirectory(prefix=name + "-", dir=destination) as temporary:
                checkout = Path(temporary) / "checkout"
                checkout.mkdir()
                git(checkout, "init", "--quiet")
                git(checkout, "remote", "add", "origin", dependency["url"])
                git(checkout, "fetch", "--depth=1", "origin", revision)
                git(checkout, "checkout", "--quiet", "--detach", "FETCH_HEAD")
                if git(checkout, "rev-parse", "HEAD", capture=True).strip() != revision:
                    raise RuntimeError(f"Unexpected revision fetched for {name}")
                checkout.rename(target)
        if (git(target, "rev-parse", "HEAD", capture=True).strip() != revision
                or git(target, "status", "--porcelain", capture=True).strip()):
            raise RuntimeError(f"{target} differs from the dependency lock; remove it and retry")
        print(f"Verified {name}: {revision}", flush=True)


if __name__ == "__main__":
    main()
