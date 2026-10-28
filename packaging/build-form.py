"""Build the macOS SBCL form package using pinned Lisp dependencies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run(*command, **options):
    return subprocess.check_output(command, text=True, **options).strip()


def system_library(name):
    return name.startswith(("/usr/lib/", "/System/Library/"))


def install_id(path):
    lines = run("otool", "-D", str(path)).splitlines()
    return lines[1].strip() if len(lines) > 1 else None


def dependencies(path):
    identity = install_id(path)
    return [name for line in run("otool", "-L", str(path)).splitlines()[1:]
            if (name := line.strip().split(" (compatibility version", 1)[0]) != identity]


def resolve_dependency(name, source, runtime):
    def expand(value):
        return Path(value.replace("@loader_path", str(source.parent))
                         .replace("@executable_path", str(runtime.parent)))

    if name.startswith("@rpath/"):
        # Resolve existing source-image rpaths before rewriting the copied file.
        lines = run("otool", "-l", str(source)).splitlines()
        for index, line in enumerate(lines):
            if line.strip() == "cmd LC_RPATH":
                value = lines[index + 2].strip().removeprefix("path ").split(" (offset", 1)[0]
                candidate = expand(value) / name.removeprefix("@rpath/")
                if candidate.is_file():
                    return candidate.resolve()
        raise RuntimeError(f"Cannot resolve {name} from {source}; no matching source rpath")
    candidate = expand(name)
    if not candidate.is_absolute() or not candidate.is_file():
        raise RuntimeError(f"Unresolved dependency {name} from {source}")
    return candidate.resolve()


def stage_native(bundle, bridge, runtime):
    libraries = bundle / "lib"
    libraries.mkdir()
    (bundle / "runtime").mkdir()
    targets = {bridge: libraries / "libwxlambda.dylib", runtime: bundle / "runtime/sbcl"}
    edges = {}
    pending = list(targets)
    while pending:
        source = pending.pop()
        edges[source] = {}
        for name in dependencies(source):
            if system_library(name):
                continue
            dependency = resolve_dependency(name, source, runtime)
            edges[source][name] = dependency
            if dependency not in targets:
                target = libraries / dependency.name
                if target in targets.values():
                    raise RuntimeError(f"Conflicting library basenames: {dependency}")
                targets[dependency] = target
                pending.append(dependency)

    architecture = set(run("lipo", "-archs", str(runtime)).split())
    for source, target in targets.items():
        if not architecture.issubset(set(run("lipo", "-archs", str(source)).split())):
            raise RuntimeError(f"Architecture mismatch: {source}")
        shutil.copy2(source, target)
        target.chmod(target.stat().st_mode | stat.S_IWUSR)
        if install_id(source):
            run("install_name_tool", "-id", "@loader_path/" + target.name, str(target))
        for old_name, dependency in edges[source].items():
            relative = os.path.relpath(targets[dependency], target.parent)
            run("install_name_tool", "-change", old_name, "@loader_path/" + relative, str(target))
        run("codesign", "--force", "--sign", "-", "--preserve-metadata=entitlements",
            "--timestamp=none", str(target))

    # Audit every copied object, including SBCL itself. No build-machine dylib
    # path or unresolved rpath may remain in the staged dependency graph.
    records = []
    for source, target in targets.items():
        for name in dependencies(target):
            if not system_library(name):
                if not name.startswith("@loader_path/"):
                    raise RuntimeError(f"Non-relocatable dependency: {target}: {name}")
                if not (target.parent / name.removeprefix("@loader_path/")).is_file():
                    raise RuntimeError(f"Missing staged dependency: {target}: {name}")
        records.append({"source": str(source), "file": str(target.relative_to(bundle)),
                        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                        "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist/wxlambda-form")
    parser.add_argument("--sbcl", default="sbcl")
    parser.add_argument("--bridge", type=Path, default=ROOT / "build/libwxlambda.dylib")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This packaging recipe currently supports macOS only")
    output = args.output.absolute()
    if output.exists():
        parser.error(f"Output already exists: {output}; choose a fresh directory")
    bridge = args.bridge.resolve(strict=True)
    command = [args.sbcl, "--noinform", "--no-sysinit", "--no-userinit", "--non-interactive"]
    info = run(*command, "--eval",
               '(format t "~A~%~A~%" sb-ext:*runtime-pathname* (lisp-implementation-version))').splitlines()
    runtime = Path(info[0]).resolve(strict=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".wxlambda-package-", dir=output.parent) as temporary:
        bundle = Path(temporary) / "application"
        bundle.mkdir()
        records = stage_native(bundle, bridge, runtime)
        subprocess.run(command + ["--load", str(ROOT / "ci/setup.lisp"),
                                   "--load", str(ROOT / "packaging/save-form.lisp")],
                       cwd=ROOT, env={**os.environ, "WXLAMBDA_PACKAGE_CORE": str(bundle / "app.core")},
                       check=True, timeout=120)
        launcher = bundle / "wxlambda-form"
        launcher.write_text('#!/bin/sh\n'
                            'bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1\n'
                            'exec "$bundle_dir/runtime/sbcl" --noinform --core "$bundle_dir/app.core" '
                            '--end-runtime-options "$@"\n', encoding="utf-8")
        launcher.chmod(0o755)
        manifest = {"sbcl_version": info[1], "native_files": records,
                    "core_sha256": hashlib.sha256((bundle / "app.core").read_bytes()).hexdigest()}
        (bundle / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        # Retain available installed license notices with the runtime files.
        notices = bundle / "licenses"
        notices.mkdir()
        for record in records:
            source = Path(record["source"])
            for ancestor in source.parents:
                if (ancestor / "INSTALL_RECEIPT.json").is_file():
                    folder = notices / ancestor.parent.name
                    folder.mkdir(exist_ok=True)
                    for pattern in ("COPYING*", "LICENSE*", "LICENCE*", "COPYRIGHT*"):
                        for notice in ancestor.glob(pattern):
                            if notice.is_file():
                                shutil.copy2(notice, folder / notice.name)
                    break
        bundle.rename(output)
    print(f"Built {output}: SBCL {info[1]}, {len(records)} native objects")


if __name__ == "__main__":
    main()
