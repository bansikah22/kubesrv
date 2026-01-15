#!/bin/bash

set -e

echo "=== Building kubesrv locally ==="

# Generate static files
./scripts/embed-static.sh

# Compile
gcc -Os -I src -o kubesrv src/main.c src/server.c src/http.c

echo "=== Build complete ==="
echo ""
echo "=== Starting kubesrv on port 8080 ==="
echo ""

# Run with test environment
PORT=8080 \
MESSAGE="Local Test" \
POD_NAME="test-pod" \
POD_NAMESPACE="test-ns" \
POD_IP="127.0.0.1" \
NODE_NAME="local-node" \
./kubesrv
