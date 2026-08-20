"""
Shared pytest fixtures for the HTTP server test suite.
Each test gets an isolated docroot (tmp_path) and a fresh server
instance on a free port, so tests can run in parallel and don't
depend on external state.
"""
import os
import signal
import socket
import subprocess
import textwrap
import time
import sys

import pytest

from pathlib import Path

SERVER_BIN = Path(__file__).resolve().parent.parent / "server"
STARTUP_TIMEOUT = 3.0
SHUTDOWN_TIMEOUT = 5.0


def _free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def _wait_for_port(port, timeout=STARTUP_TIMEOUT):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.05)
    return False


@pytest.fixture
def docroot(tmp_path):
    """A minimal document root: static file, large file, pdf, and a CGI script."""
    root = tmp_path / "www"
    root.mkdir()

    (root / "index.html").write_text("<html><body>hello</body></html>")
    (root / "big.bin").write_bytes(os.urandom(20 * 1024 * 1024))  # 20MB
    (root / "example.pdf").write_bytes(os.urandom(10))

    cgi = root / "cgi-bin"
    cgi.mkdir()
    script = cgi / "hello.py"
    script_body = f"""\
            #!{sys.executable} python3
            import sys
            input_data = sys.stdin.read()
            sys.stdout.write("CGI executed with arguments: " + input_data)
            sys.stdout.flush()
            """
    print()
    print(script_body)
    script.write_text(textwrap.dedent(script_body))
    script.chmod(0o755)

    return root


@pytest.fixture
def secret_file(tmp_path, docroot):
    """A file OUTSIDE the docroot. Traversal tests assert this is never served."""
    outside = tmp_path / "secret.txt"
    outside.write_text("THIS SHOULD NEVER BE SERVED")
    return outside


@pytest.fixture
def server(tmp_path, docroot):
    """Launches the server against an isolated docroot/config and tears it
    down with SIGINT afterward. Teardown failing (hang, crash, nonzero exit)
    fails the test — this doubles as a regression check on the shutdown path.
    """
    port = _free_port()
    conf_path = tmp_path / "server.conf"
    conf_path.write_text(
        textwrap.dedent(
            f"""\
            listen_port = {port}
            server_root = "{docroot}"
            server_signature = "test-server"
            log_file = "log_tests"
            timeout_ms = 2000
            pool_size = 4
            queue_size = 8
            backlog = 16
            """
        )
    )

    proc = subprocess.Popen(
        [SERVER_BIN],
        cwd=str(tmp_path),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    if not _wait_for_port(port):
        proc.kill()
        out, _ = proc.communicate(timeout=2)
        pytest.fail(
            f"Server did not start listening on port {port}.\n"
            f"Output:\n{out.decode(errors='replace')}"
        )

    yield f"http://127.0.0.1:{port}"

    proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=SHUTDOWN_TIMEOUT)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        pytest.fail("Server did not shut down cleanly after SIGINT (had to be force-killed).")

    if proc.returncode not in (0, -signal.SIGINT):
        out = proc.stdout.read().decode(errors="replace") if proc.stdout else ""
        pytest.fail(f"Server exited with code {proc.returncode} after SIGINT.\nOutput:\n{out}")
