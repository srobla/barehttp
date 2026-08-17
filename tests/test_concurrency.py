"""
Concurrency correctness. pool_size=4, queue_size=8 in the test config
(see conftest.py), so tests deliberately exceed that to exercise the
queue-full / thread-pool-saturated paths.
"""
import concurrent.futures

import requests


def test_many_concurrent_requests_all_succeed(server):
    def do(_):
        r = requests.get(f"{server}/", timeout=10)
        return r.status_code

    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as ex:
        results = list(ex.map(do, range(200)))

    assert all(code == 200 for code in results)


def test_queue_saturation_no_crash(server):
    # more concurrent clients than pool_size(4) + queue_size(8)
    def do(_):
        try:
            r = requests.get(f"{server}/", timeout=10)
            return r.status_code
        except requests.RequestException:
            return None

    with concurrent.futures.ThreadPoolExecutor(max_workers=40) as ex:
        results = list(ex.map(do, range(40)))

    # some requests may be slow/queued, but none should crash the server
    r = requests.get(f"{server}/", timeout=10)
    assert r.status_code == 200
    # at least the ones that completed should have succeeded
    assert results.count(200) > 0
