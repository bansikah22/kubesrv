# kubesrv

Minimal HTTP server in C for Kubernetes testing.
![UI](./docs/images/latest-ui-14-26.png)

## Features

- Ultra-small image (~1.19MB)
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

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `80` | Listen port |
| `MESSAGE` | `Hello, Kubernetes!` | Greeting message (fallback) |
| `MESSAGE_FILE` | - | Full path or filename to read message from |

### ConfigMap Volume Mount

You can mount any ConfigMap as a volume and specify the file path:

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: my-config
data:
  greeting.txt: |
    Hello from my custom ConfigMap!
---
apiVersion: apps/v1
kind: Deployment
spec:
  template:
    spec:
      containers:
      - name: kubesrv
        env:
        - name: MESSAGE_FILE
          value: "/usr/share/kubesrv/greeting.txt"  # Full path
        volumeMounts:
        - name: config
          mountPath: /usr/share/kubesrv
          readOnly: true
      volumes:
      - name: config
        configMap:
          name: my-config
```

**Priority order:**
1. File from `${MESSAGE_FILE}` (if set)
2. Environment variable `MESSAGE`
3. Default: "Hello, Kubernetes!"

## Build

```bash
docker build -t bansikah/kubesrv:latest .
```

## License

GPL-3.0 - See [LICENSE](LICENSE)
