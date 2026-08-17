#!/usr/bin/env python3
import sys
import subprocess

if len(sys.argv) != 2:
    print(f"Use: {sys.argv[0]} <puerto>")
    sys.exit(1)

port = sys.argv[1]
host = f"http://localhost:{port}"


def run_test(cmd):
    print("========================================")
    print(f"Executing: {cmd}")
    print("Response headers: ")

    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )

    headers = []
    for line in result.stdout.splitlines():
        if line.strip() == "":
            break
        headers.append(line)

    print("\n".join(headers))
    print()


print(f"==== Starting tests against {host} ====\n")

# get 
run_test(f"curl -i -s {host}/")

# get nonexisting file 
run_test(f"curl -i -s {host}/noexists.txt")

# get path traveral
run_test(f"curl --path-as-is -i {host}/../src/server.c")

# get path traveral 2
run_test(f"curl --path-as-is -i {host}/media/../src/server.c")

# put
run_test(f"curl -i -s -X PUT {host}/")

# options
run_test(f"curl -i -s -X OPTIONS {host}/")

# huge get
big_path = "A" * 10000
run_test(f"curl -i -s {host}/{big_path}")

print("==== Tests ended ====")
