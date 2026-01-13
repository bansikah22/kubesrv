# kubesrv

Minimal HTTP server in C for Kubernetes testing.

## Features

- Ultra-small image (~50KB)
- Fork-based concurrency
- Health checks (`/healthz`)
- Prometheus metrics (`/metrics`)
- Configurable via environment variables

## Quick Start

```bash
# Run locally
docker run -p 8080:80 bansikah/kubesrv:latest

# Deploy to Kubernetes
kubectl apply -f k8s/
kubectl port-forward svc/kubesrv-svc 8080:80 -n kubesrv-ns
```

## Endpoints

| Path | Description |
|------|-------------|
| `/` | HTML dashboard |
| `/healthz` | Health check |
| `/info` | JSON server info |
| `/metrics` | Prometheus metrics |

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `80` | Listen port |
| `MESSAGE` | `Hello, Kubernetes!` | Greeting message |

## Build

```bash
docker build -t bansikah/kubesrv:latest .
```

## License

GPL-3.0 - See [LICENSE](LICENSE)
