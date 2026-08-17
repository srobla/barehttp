#!/bin/bash

if [ "$#" -ne 3 ]; then
  echo "Use: $0 PORT NAME VALUE"
  exit 1
fi

PORT=$1
NAME=$2
VALUE=$3

URL="http://localhost:$PORT/scripts/test.py"

echo "---- GET (CGI) ----"
curl -X GET "$URL?$NAME=$VALUE"

echo -e "\n\n---- POST ----"
curl -X POST "$URL" \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "$NAME=$VALUE"

echo ""
