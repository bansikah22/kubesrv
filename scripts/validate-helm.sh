#!/bin/bash
set -e

CHART_PATH="charts/kubesrv"

echo "=== Linting Helm Chart ==="
helm lint $CHART_PATH

echo -e "\n=== Validating Templates (Default) ==="
helm template my-release $CHART_PATH --debug > /dev/null

echo -e "\n=== Validating Templates (CI Values) ==="
helm template my-release $CHART_PATH -f $CHART_PATH/ci/test-values.yaml --debug > /dev/null

echo -e "\n=== Security Scan (requires kube-linter) ==="
if command -v kube-linter &> /dev/null; then
    kube-linter lint $CHART_PATH
else
    echo "kube-linter not found. Skipping security scan."
    echo "Install it with: brew install kube-linter (or see https://github.com/stackrox/kube-linter)"
fi

echo -e "\n=== Helm Chart is valid! ==="
