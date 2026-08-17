#!/bin/bash

# Use: ./script.sh N PORT

N=$1
PORT=$2

if [ -z "$N" ] || [ -z "$PORT" ]; then
    echo "Uso: $0 N PORT"
    exit 1
fi

for ((i=1; i<=N; i++)); do
    curl -s "http://localhost:$PORT" &
done

wait

echo "All curls finished."
