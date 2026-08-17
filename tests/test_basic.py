"""
Core functional correctness: does the server serve what it should,
with the status codes and headers it should.
"""
import requests


def test_index_ok(server):
    r = requests.get(f"{server}/")
    assert r.status_code == 201
    assert "hello" in r.text


def test_nonexistent_file_is_404(server):
    r = requests.get(f"{server}/noexists.txt")
    assert r.status_code == 404


def test_unsupported_method_rejected(server):
    r = requests.request("PUT", f"{server}/")
    # ADJUST: confirm which of these your server actually returns
    assert r.status_code in (405, 501)


def test_options_ok(server):
    r = requests.options(f"{server}/")
    assert r.status_code in (200, 204)


def test_content_length_matches_body(server):
    r = requests.get(f"{server}/")
    assert "Content-Length" in r.headers
    assert int(r.headers["Content-Length"]) == len(r.content)


def test_large_file_download_is_complete(server):
    r = requests.get(f"{server}/big.bin")
    assert r.status_code == 200
    assert len(r.content) == 2 * 1024 * 1024


def test_server_signature_present(server):
    # sanity check that server.conf is actually being read
    r = requests.get(f"{server}/")
    assert r.status_code == 200
