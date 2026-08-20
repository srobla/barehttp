"""
Core functional correctness: does the server serve what it should,
with the status codes and headers it should.
"""
import requests


def test_index_ok(server):
    r = requests.get(f"{server}/")
    assert r.status_code == 200
    assert "hello" in r.text


def test_nonexistent_file_is_404(server):
    r = requests.get(f"{server}/noexists.txt")
    assert r.status_code == 404


def test_unsupported_method_rejected(server):
    r = requests.request("PUT", f"{server}/")
    assert r.status_code == 405


def test_options_ok(server):
    r = requests.options(f"{server}/")
    assert r.status_code == 204


def test_content_length_matches_body(server):
    r = requests.get(f"{server}/")
    assert "Content-Length" in r.headers
    assert int(r.headers["Content-Length"]) == len(r.content)


def test_large_file_download_is_complete(server):
    r = requests.get(f"{server}/big.bin")
    assert r.status_code == 200
    assert len(r.content) == 20 * 1024 * 1024


def test_server_signature_present(server):
    r = requests.get(f"{server}/")
    assert r.status_code == 200


#TODO: Update when updating to 1.1
def test_connection_close(server):
    r = requests.get(f"{server}/")
    assert "Connection" in r.headers
    assert r.headers["Connection"] == "Close"


def test_mime_type(server):
    r = requests.get(f"{server}/")
    assert "Content-Type" in r.headers
    assert "html" in r.headers["Content-Type"]
    r = requests.get(f"{server}/example.pdf")
    assert "Content-Type" in r.headers
    assert "application/pdf" in r.headers["Content-Type"]


def test_cgi_get(server):
    r = requests.get(f"{server}/cgi-bin/hello.py?name=bob")
    assert "CGI executed with arguments: name=bob" in r.text

def test_cgi_post(server):
    r = requests.post(f"{server}/cgi-bin/hello.py", "name=alice")
    assert "CGI executed with arguments: name=alice" in r.text

