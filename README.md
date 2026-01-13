# kubesrv

Minimal HTTP server in C for Kubernetes testing.

## Features

- **Ultra-small image** - ~100KB with scratch base
- **Fork-based concurrency** - handles multiple connections
- **Health checks** - `/healthz` for K8s probes
- **Metrics endpoint** - `/metrics` for Prometheus
- **Info endpoint** - `/info` shows hostname, uptime, request count
- **Configurable message** - `MESSAGE` env var
- **Request logging** - method, path, client IP, timestamp
- **Graceful shutdown** - handles SIGTERM/SIGINT

## Quick Start

### Build and Run

```bash
docker build -t bansikah/kubesrv:latest .
docker run -p 8080:80 bansikah/kubesrv:latest
```

### Test

```bash
curl http://localhost:8080
curl http://localhost:8080/healthz
curl http://localhost:8080/info
curl http://localhost:8080/metrics
```

### Deploy to Kubernetes

```bash
kubectl apply -f k8s/
kubectl port-forward svc/kubesrv-svc 8080:80 -n kubesrv-ns
```

## Endpoints

| Path | Response | Description |
|------|----------|-------------|
| `/` | HTML UI | Dashboard with hostname, uptime, requests |
| `/healthz` | `OK` | Health check for K8s probes |
| `/info` | JSON | Server info (hostname, version, uptime) |
| `/metrics` | Prometheus | Request count and uptime metrics |

## Configuration

| Environment Variable | Default | Description |
|---------------------|---------|-------------|
| `PORT` | `80` | Server listening port |
| `MESSAGE` | `Hello, Kubernetes!` | Custom greeting message |



## Docker Hub

```bash
docker build -t bansikah/kubesrv:latest .
docker push bansikah/kubesrv:latest
```

## Kubernetes Health Probes

```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 80
  initialDelaySeconds: 3
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /healthz
    port: 80
  initialDelaySeconds: 1
  periodSeconds: 5
```

## Prometheus Metrics

```
kubesrv_requests_total 42
kubesrv_uptime_seconds 3600
```

## Logs

```bash
kubectl logs kubesrv -n kubesrv-ns -f
```

Output:
```
[kubesrv] kubesrv-pod-xyz v1.0.0 on port 80
[kubesrv] Endpoints: / /healthz /info /metrics
[2026-01-13T10:30:45Z] 10.244.0.1 GET /healthz
[2026-01-13T10:30:50Z] 10.244.0.1 GET /
```

## Cleanup

```bash
kubectl delete -f k8s/
```

## License

MIT
