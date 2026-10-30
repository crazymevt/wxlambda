"""Stage the checksum-pinned x64 WebView2 SDK privately; no runtime install."""
import argparse
import hashlib
import io
from pathlib import Path
import urllib.request
import zipfile

VERSION = "1.0.3179.45"
SHA256 = "70bd381c5d67f97a6a80294001cde125ab5b24b6d36098ad901d8e15efd752d0"
URL = f"https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/{VERSION}/microsoft.web.webview2.{VERSION}.nupkg"
FILES = (
    "LICENSE.txt", "NOTICE.txt",
    "build/native/include/WebView2.h",
    "build/native/include/WebView2EnvironmentOptions.h",
    "build/native/x64/WebView2Loader.dll",
    "build/native/x64/WebView2Loader.dll.lib",
    "build/native/x64/WebView2LoaderStatic.lib",
)


def contents(raw):
    if hashlib.sha256(raw).hexdigest() != SHA256:
        raise ValueError("WebView2 SDK SHA256 does not match pinned package")
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        return {name: archive.read(name) for name in FILES}


def stage(destination, files):
    # Validate the whole staging set before overwriting anything.
    for name, data in files.items():
        path = destination / name
        if path.exists() and path.read_bytes() != data:
            raise ValueError(f"Refusing to replace different SDK file: {path}")
    for name, data in files.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--archive", type=Path, help="offline copy of the exact pinned nupkg")
    args = parser.parse_args()
    try:
        if args.archive:
            with args.archive.open("rb") as archive:
                raw = archive.read(32 * 1024 * 1024 + 1)
        else:
            with urllib.request.urlopen(URL, timeout=60) as response:
                raw = response.read(32 * 1024 * 1024 + 1)
        if len(raw) > 32 * 1024 * 1024:
            raise ValueError("SDK archive exceeds32MiB limit")
        stage(args.destination, contents(raw))
        print(f"PASS: WebView2 SDK {VERSION} x64 staged; SHA256 verified; runtime not installed")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"WebView2 staging rejected: {error}\n")


if __name__ == "__main__":
    main()
