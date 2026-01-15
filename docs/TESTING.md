# Testing Guide for kubesrv

This guide covers how to test all the new Kubernetes-aware features.

## Prerequisites

- Docker installed
- Kubernetes cluster (minikube, kind, or cloud)
- kubectl configured
- curl or similar HTTP client

## Local Testing (Docker)

### 1. Build the Image

```bash
docker build -t bansikah/kubesrv:latest .
```

### 2. Run Locally

```bash
docker run -p 8080:80 \
  -e MESSAGE="Testing locally" \
  -e POD_NAME="local-test" \
  -e POD_NAMESPACE="local" \
  bansikah/kubesrv:latest
```

### 3. Test Basic Endpoints

```bash
# Health check
curl http://localhost:8080/healthz

# Server info
curl http://localhost:8080/info

# Identity (will show local values)
curl http://localhost:8080/identity

# Echo request details
curl http://localhost:8080/echo
```

### 4. Test Failure Injection

```bash
# Simulate 500 error
curl -i http://localhost:8080/fail?code=500

# Simulate 503 error
curl -i http://localhost:8080/fail?code=503

# Simulate 404 error
curl -i http://localhost:8080/fail?code=404
```

### 5. Test Latency Simulation

```bash
# 250ms delay
time curl http://localhost:8080/sleep?ms=250

# 2 second delay
time curl http://localhost:8080/sleep?ms=2000

# 5 second delay
time curl http://localhost:8080/sleep?ms=5000
```

### 6. Test Readiness Endpoint

```bash
# Test immediately after start (should return 503)
docker run -p 8080:80 bansikah/kubesrv:latest &
sleep 2
curl -i http://localhost:8080/ready  # Should return 503

# Wait for ready state
sleep 10
curl -i http://localhost:8080/ready  # Should return 200
```

### 7. Test Metrics

```bash
# Generate some traffic
for i in {1..10}; do curl http://localhost:8080/info; done
curl http://localhost:8080/fail?code=500
curl http://localhost:8080/fail?code=503

# Check metrics
curl http://localhost:8080/metrics
```

## Kubernetes Testing

### 1. Deploy to Kubernetes

```bash
# Apply manifests
kubectl apply -f k8s/

# Wait for pod to be ready
kubectl wait --for=condition=ready pod -l app=kubesrv -n kubesrv-ns --timeout=60s

# Check pod status
kubectl get pods -n kubesrv-ns
```

### 2. Test Pod Identity

```bash
# Port forward
kubectl port-forward svc/kubesrv-svc 8080:80 -n kubesrv-ns &

# Test identity endpoint
curl http://localhost:8080/identity
```

Expected output:
```json
{
  "pod": "kubesrv-7d4b45c4d7-xyz",
  "namespace": "kubesrv-ns",
  "node": "minikube",
  "ip": "10.244.0.5",
  "hostname": "kubesrv-7d4b45c4d7-xyz"
}
```

### 3. Test Readiness Probe Behavior

```bash
# Watch pod events
kubectl get events -n kubesrv-ns --watch &

# Delete and recreate pod
kubectl delete pod -l app=kubesrv -n kubesrv-ns
kubectl get pods -n kubesrv-ns -w

# Observe:
# - Pod starts
# - Readiness probe fails for ~10 seconds
# - Readiness probe succeeds after 10 seconds
# - Pod becomes Ready
```

### 4. Test Service Load Balancing

Scale to multiple replicas:

```bash
# Scale deployment
kubectl scale deployment kubesrv --replicas=3 -n kubesrv-ns

# Wait for all pods
kubectl wait --for=condition=ready pod -l app=kubesrv -n kubesrv-ns --timeout=60s

# Test which pod handles requests
kubectl port-forward svc/kubesrv-svc 8080:80 -n kubesrv-ns &

for i in {1..10}; do
  echo "Request $i:"
  curl -s http://localhost:8080/identity | jq -r .pod
done
```

You should see requests distributed across different pods.

### 5. Test Failure Scenarios

```bash
# Test retry logic with failures
for i in {1..5}; do
  echo "Attempt $i:"
  curl -i http://localhost:8080/fail?code=503
  sleep 1
done

# Check failure metrics
curl http://localhost:8080/metrics | grep failures
```

### 6. Test Timeout Behavior

```bash
# Test with short timeout (should fail)
curl --max-time 2 http://localhost:8080/sleep?ms=5000

# Test with adequate timeout (should succeed)
curl --max-time 10 http://localhost:8080/sleep?ms=5000
```

### 7. Test Liveness vs Readiness

```bash
# Get pod name
POD=$(kubectl get pod -l app=kubesrv -n kubesrv-ns -o jsonpath='{.items[0].metadata.name}')

# Check liveness (should always be healthy)
kubectl exec -n kubesrv-ns $POD -- wget -q -O- http://localhost:80/healthz

# Check readiness
kubectl exec -n kubesrv-ns $POD -- wget -q -O- http://localhost:80/ready
```

### 8. Test Echo Endpoint for Debugging

```bash
# Test with custom headers
curl -H "X-Custom-Header: test" \
     -H "Authorization: Bearer token123" \
     http://localhost:8080/echo
```

### 9. Stress Test

```bash
# Generate load
for i in {1..100}; do
  curl -s http://localhost:8080/info > /dev/null &
done
wait

# Check metrics
curl http://localhost:8080/metrics
```

### 10. Test with Ingress (if available)

```bash
# Create ingress
cat <<EOF | kubectl apply -f -
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: kubesrv-ingress
  namespace: kubesrv-ns
spec:
  rules:
  - host: kubesrv.local
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: kubesrv-svc
            port:
              number: 80
EOF

# Test (add kubesrv.local to /etc/hosts first)
curl http://kubesrv.local/echo
curl http://kubesrv.local/identity
```

## Automated Test Script

Create a test script:

```bash
#!/bin/bash

BASE_URL="http://localhost:8080"

echo "Testing kubesrv endpoints..."

# Test healthz
echo -n "Testing /healthz... "
if curl -sf $BASE_URL/healthz > /dev/null; then
  echo "PASS"
else
  echo "FAIL"
fi

# Test info
echo -n "Testing /info... "
if curl -sf $BASE_URL/info | jq . > /dev/null 2>&1; then
  echo "PASS"
else
  echo "FAIL"
fi

# Test identity
echo -n "Testing /identity... "
if curl -sf $BASE_URL/identity | jq . > /dev/null 2>&1; then
  echo "PASS"
else
  echo "FAIL"
fi

# Test echo
echo -n "Testing /echo... "
if curl -sf $BASE_URL/echo | jq . > /dev/null 2>&1; then
  echo "PASS"
else
  echo "FAIL"
fi

# Test fail
echo -n "Testing /fail?code=500... "
if curl -sf $BASE_URL/fail?code=500 > /dev/null 2>&1; then
  echo "FAIL (should have failed)"
else
  echo "PASS"
fi

# Test sleep
echo -n "Testing /sleep?ms=100... "
START=$(date +%s)
curl -sf $BASE_URL/sleep?ms=100 > /dev/null
END=$(date +%s)
DURATION=$((END - START))
if [ $DURATION -ge 0 ]; then
  echo "PASS"
else
  echo "FAIL"
fi

# Test metrics
echo -n "Testing /metrics... "
if curl -sf $BASE_URL/metrics | grep kubesrv_requests_total > /dev/null; then
  echo "PASS"
else
  echo "FAIL"
fi

echo "All tests completed!"
```

Save as `test.sh`, make executable, and run:

```bash
chmod +x test.sh
./test.sh
```

## Cleanup

```bash
# Stop port-forward
pkill -f "port-forward"

# Delete Kubernetes resources
kubectl delete -f k8s/

# Stop local Docker container
docker stop $(docker ps -q --filter ancestor=bansikah/kubesrv:latest)
```

## Expected Behavior Summary

| Endpoint | Expected Response | Status Code |
|----------|------------------|-------------|
| `/healthz` | "OK" | 200 |
| `/ready` (< 10s) | "Not Ready..." | 503 |
| `/ready` (> 10s) | "Ready" | 200 |
| `/info` | JSON with server info | 200 |
| `/identity` | JSON with K8s identity | 200 |
| `/echo` | JSON with request details | 200 |
| `/fail?code=500` | "Simulated failure: 500" | 500 |
| `/fail?code=503` | "Simulated failure: 503" | 503 |
| `/sleep?ms=250` | "OK" (after delay) | 200 |
| `/metrics` | Prometheus format | 200 |
