"""Optional local Swank wire probe; not an editor-support or pinned-CI test."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parent.parent
if len(sys.argv) < 3 or sys.argv[1] not in ("nil", ":fd-handler", ":spawn"):
    raise SystemExit("Usage: editor-probe.py STYLE LISP [FLAGS incl. Quicklisp setup]")
style, *command = sys.argv[1:]


def receive(connection, count):
    data = b""
    while len(data) < count:
        chunk = connection.recv(count - len(data))
        if not chunk:
            raise RuntimeError("Swank closed before replying")
        data += chunk
    return data


def request(connection, form, identifier):
    data = f'(:emacs-rex {form} "CL-USER" t {identifier})'.encode()
    connection.sendall(f"{len(data):06x}".encode() + data)
    for _ in range(100):
        length = int(receive(connection, 6), 16)
        reply = receive(connection, length).decode()
        if ":debug " in reply.lower() or ":abort" in reply.lower():
            raise RuntimeError(f"Swank evaluation failed: {reply}")
        if reply.lower().startswith("(:return "):
            return reply
    raise RuntimeError("No Swank return after 100 messages")


with tempfile.TemporaryDirectory(prefix="wxlambda-swank-") as temporary:
    port_file = Path(temporary) / "port"
    stop_file = Path(temporary) / "stop"
    log = Path(temporary) / "server.log"
    with log.open("w") as output:
        process = subprocess.Popen(
            command + ["--load", str(root / "tests/editor-probe.lisp")], cwd=root,
            env={**os.environ, "WXLAMBDA_SWANK_STYLE": style,
                 "WXLAMBDA_SWANK_PORT_FILE": str(port_file),
                 "WXLAMBDA_SWANK_STOP_FILE": str(stop_file)},
            stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            while not port_file.exists() or not port_file.read_text():
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Swank did not announce a port within 60 seconds")
                time.sleep(0.02)
            with socket.create_connection(("127.0.0.1", int(port_file.read_text())), timeout=15) as connection:
                secret_file = Path.home() / ".slime-secret"
                if secret_file.exists():
                    # Swank authenticates before reading the first framed RPC.
                    secret = secret_file.read_text().splitlines()[0]
                    data = ('"' + secret.replace('\\', '\\\\').replace('"', '\\"') + '"').encode()
                    connection.sendall(f"{len(data):06x}".encode() + data)
                reply = request(connection, "(cl-user::editor-gui-probe)", 1)
                if "PROBE-PASS" not in reply:
                    raise RuntimeError(f"Unexpected probe result: {reply}")
                print(f"{command[0]} / {style}: {reply}", flush=True)
                stop_file.touch()
                process.stdin.close()
            process.wait(timeout=15)
            if process.returncode:
                raise RuntimeError(f"Swank process exited {process.returncode}")
        except BaseException:
            print(log.read_text()[-6000:], file=sys.stderr)
            raise
        finally:
            if not process.stdin.closed:
                process.stdin.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
