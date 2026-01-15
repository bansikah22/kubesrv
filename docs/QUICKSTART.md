# Quick Start Guide

## Build and Test Locally

```bash
# Build and run
make run

# Run tests
make test

# Clean up
make clean
```

## Build and Test with Docker

```bash
# Build Docker image
make docker-build

# Run container
make docker-run

# Run automated tests
make docker-test
```

## Deploy to Kubernetes

```bash
# Deploy
make k8s-deploy

# Test deployment
make k8s-test

# Clean up
make k8s-clean
```

## Manual Testing

### Start local server
```bash
make run
```

### In another terminal, test endpoints:

```bash
# Health check
curl http://localhost:8080/healthz

# Readiness (will be 503 for first 10 seconds)
curl -i http://localhost:8080/ready

# Pod identity
curl http://localhost:8080/identity

# Server info
curl http://localhost:8080/info

# Echo request details
curl http://localhost:8080/echo

# Simulate 500 error
curl -i http://localhost:8080/fail?code=500

# Simulate 503 error
curl -i http://localhost:8080/fail?code=503

# Simulate latency (2 seconds)
time curl http://localhost:8080/sleep?ms=2000

# Prometheus metrics
curl http://localhost:8080/metrics

# Failure injection examples
curl "http://localhost:8080/fail?code=503"                  # immediate 503
curl "http://localhost:8080/fail?rate=0.2&code=500"        # every 5th request fails with 500

# Readiness override
docker run -e READY_DELAY=0 -p 8080:80 bansikah/kubesrv:latest

# Shutdown delay
kubectl -n kubesrv-ns set env deploy/kubesrv SHUTDOWN_DELAY_MS=5000
```

## Kubernetes Testing

After deploying with `make k8s-deploy`:

```bash
# Port forward
kubectl port-forward svc/kubesrv-svc 8080:80 -n kubesrv-ns

# Test identity (shows real pod info)
curl http://localhost:8080/identity

# Scale and test load balancing
kubectl scale deployment kubesrv --replicas=3 -n kubesrv-ns
for i in {1..10}; do curl -s http://localhost:8080/identity | grep pod; done

# Watch readiness behavior
kubectl delete pod -l app=kubesrv -n kubesrv-ns
kubectl get pods -n kubesrv-ns -w
```

## Expected Outputs

### /identity
```json
{
  "pod": "kubesrv-7d4b45c4d7-xyz",
  "namespace": "kubesrv-ns",
  "node": "minikube",
  "ip": "10.244.0.5",
  "hostname": "kubesrv-7d4b45c4d7-xyz"
}
```

### /info
```json
{
  "hostname": "kubesrv-7d4b45c4d7-xyz",
  "version": "1.1.0",
  "uptime": 45,
  "requests": 12,
  "timestamp": "2024-01-15T10:30:00Z",
  "message": "Hello, Kubernetes!"
}
```

### /echo
```json
{
  "method": "GET",
  "path": "/echo",
  "client_ip": "127.0.0.1"
}
```

### /metrics
```
# HELP kubesrv_requests_total Total requests
# TYPE kubesrv_requests_total counter
kubesrv_requests_total 15
# HELP kubesrv_failures_total Total failures
# TYPE kubesrv_failures_total counter
kubesrv_failures_total 2
# HELP kubesrv_uptime_seconds Uptime
# TYPE kubesrv_uptime_seconds gauge
kubesrv_uptime_seconds 120
```
