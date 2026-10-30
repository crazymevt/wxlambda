"""Fetch the pinned Windows SBCL MSI, retrying official hosts without weakening the pin."""
import argparse
import hashlib
import http.client
from pathlib import Path
import tempfile
import urllib.error
import urllib.request

SHA256 = "2be695e44a69ff29f01cf7980f67472cb383e75e9292d5fcd27cf5418b871226"
PACKAGE = "project/sbcl/sbcl/2.6.9/sbcl-2.6.9-x86-64-windows-binary.msi"
URLS = (f"https://downloads.sourceforge.net/{PACKAGE}",
        f"https://master.dl.sourceforge.net/{PACKAGE}")
LIMIT = 64 * 1024 * 1024


def verified(data):
    digest = hashlib.sha256(data).hexdigest()
    if len(data) > LIMIT or digest != SHA256 or data[:8] != b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
        raise ValueError(f"Rejected response: bytes={len(data)}, SHA256={digest}; expected pinned MSI")
    return data


def download(destination, fetch=None):
    if destination.exists():
        raise ValueError("Installer destination already exists; refusing overwrite")
    if fetch is None:
        def fetch(url):
            request = urllib.request.Request(url, headers={"User-Agent": "curl/8.0 wxlambda-ci"})
            with urllib.request.urlopen(request, timeout=60) as response:
                return response.read(LIMIT + 1)
    for url in URLS:
        try:
            data = verified(fetch(url))
        except (OSError, ValueError, http.client.HTTPException) as error:
            print(f"SBCL download attempt failed ({url}): {error}")
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        # No rejected/HTML payload is ever written as the installer.
        staging = None
        try:
            with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as temporary:
                staging = Path(temporary.name)
                temporary.write(data)
            staging.replace(destination)
        finally:
            if staging is not None:
                staging.unlink(missing_ok=True)
        print("PASS: pinned SBCL2.6.9 x64 MSI checksum and compound-file signature")
        return
    raise ValueError("All official SBCL download hosts failed validation; pin unchanged")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        download(args.destination)
    except (OSError, ValueError) as error:
        parser.exit(1, f"SBCL download failed: {error}\n")


if __name__ == "__main__":
    main()
