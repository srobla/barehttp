#!/bin/bash

# Use:
# ./bench.sh TOTAL_PETITIONS CONCURRENCY [ROUTE]
# Example:
# ./bench.sh 10000 100 /

CONFIG_FILE="../server.conf"

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Use: $0 TOTAL_PETITIONS CONCURRENCY [PATH]"
    exit 1
fi

TOTAL=$1
CONC=$2
ROUTE=${3:-/}

# Extracts port
PORT=$(grep -E "^[[:space:]]*listen_port[[:space:]]*=" "$CONFIG_FILE" \
       | sed -E 's/.*=[[:space:]]*([0-9]+).*/\1/' )

if ! [[ "$PORT" =~ ^[0-9]+$ ]]; then
    echo "Error: listen_port not found in $CONFIG_FILE"
    exit 1
fi

echo "Detected: $PORT"
echo "Executing benchmark..."

ab -n "$TOTAL" -c "$CONC" "http://localhost:$PORT$ROUTE"
