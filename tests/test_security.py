import requests
import socket


def test_path_traversal_dotdot(server, secret_file):
    r = requests.get(f"{server}/../secret.txt")
    assert "THIS SHOULD NEVER BE SERVED" not in r.text


def test_path_traversal_nested(server, secret_file):
    r = requests.get(f"{server}/cgi-bin/../../secret.txt")
    assert "THIS SHOULD NEVER BE SERVED" not in r.text


def test_path_traversal_encoded_dots(server, secret_file):
    r = requests.get(f"{server}/%2e%2e/secret.txt")
    assert "THIS SHOULD NEVER BE SERVED" not in r.text


def test_path_traversal_double_slash(server, secret_file):
    r = requests.get(f"{server}/media/..//../secret.txt")
    assert "THIS SHOULD NEVER BE SERVED" not in r.text


def test_huge_url_rejected_not_crashed(server):
    big_path = "A" * 10000
    r = requests.get(f"{server}/{big_path}", timeout=5)
    assert r.status_code == 400

    # Still alive?
    r2 = requests.get(f"{server}/")
    assert r2.status_code == 200

def test_null_byte_in_path(server, docroot):
    r = requests.get(f"{server}/index.html%00.jpg")
    assert r.status_code == 404
    assert "hello" not in r.text  # must not have served index.html's content

