# Helm Chart Documentation

This document provides detailed information about the `kubesrv` Helm chart, its security features, and the automated CI/CD workflow.

## Overview

The `kubesrv` Helm chart is designed to be a reusable, production-ready template for deploying the `kubesrv` application to any Kubernetes cluster.

## Features

- **Standard Kubernetes Objects**: Deployment, Service, ConfigMap, ServiceAccount.
- **Ingress Support**: Configurable Ingress with TLS support.
- **Autoscaling**: Built-in HorizontalPodAutoscaler (HPA) support.
- **Security**: NetworkPolicy, non-root security context, and read-only root filesystem.
- **Automated Testing**: Post-installation connection tests.

## Security Best Practices

### 1. Network Isolation
The chart includes a `NetworkPolicy` (enabled by default) that restricts traffic to the application. This prevents lateral movement within the cluster.
- **Toggle**: `networkPolicy.enabled` in `values.yaml`.

### 2. Pod Security Context
The application runs with a restricted security context:
- `runAsNonRoot: true`: Ensures the container does not run as the root user.
- `readOnlyRootFilesystem: true`: Prevents any modifications to the container's root filesystem.
- `capabilities.drop: ["ALL"]`: Removes all Linux capabilities.

### 3. Resource Management
To prevent "noisy neighbor" issues and OOM kills, the chart defines default resource requests and limits:
```yaml
resources:
  limits:
    cpu: 100m
    memory: 32Mi
  requests:
    cpu: 10m
    memory: 16Mi
```

## CI/CD Workflow

The project uses GitHub Actions for continuous validation located in `.github/workflows/helm-cd.yml`.

### Automated Checks
Every Pull Request triggers the following:
1. **Helm Lint**: Checks the chart for syntax errors and best practice violations.
2. **Template Validation**: Renders the chart to ensure valid Kubernetes YAML is produced.
3. **Security Scan**: Uses `kube-linter` to audit the rendered manifests against security standards.
4. **KinD Deployment**: Deploys the chart to a **KinD (Kubernetes in Docker)** cluster to verify it can be installed successfully in a real environment.
5. **Helm Test**: Runs functional tests (e.g., connectivity checks) against the deployed application.

## Local Validation

Before pushing changes, you can run the local validation script:
```bash
./scripts/validate-helm.sh
```
This script performs linting and template validation on your local machine.

## Accessing the Application

After installing the chart, you can access the application using `kubectl port-forward`.

### 1. Get the Pod Name
```bash
export POD_NAME=$(kubectl get pods -l "app.kubernetes.io/name=kubesrv,app.kubernetes.io/instance=my-release" -o jsonpath="{.items[0].metadata.name}")
```

### 2. Start Port Forwarding
```bash
kubectl port-forward $POD_NAME 8080:80
```

### 3. Open in Browser
Visit [http://localhost:8080](http://localhost:8080) to view the dashboard.

## Customization

You can override any value in `values.yaml` during installation:
```bash
helm install my-release ./charts/kubesrv --set replicaCount=3 --set config.message="Custom Message"
```
## Releasing the Chart
The project uses GitHub Actions to automatically publish the Helm chart to the **GitHub Container Registry (GHCR)** as an OCI artifact.

### How to trigger a release
You can release a new version of the chart by pushing a Git tag. There are two ways:
1. **Full Release**: Pushing a tag like `v1.2.0` will build the Docker image AND publish the Helm chart.
2. **Chart-only Release**: Pushing a tag like `helm-v0.1.5` will **only** package and publish the Helm chart.

```bash
# Example: Release a specific chart version
git tag helm-v0.1.0
git push origin helm-v0.1.0
```

### How to use the published chart
Once published, the chart is available at `oci://ghcr.io/bansikah22/charts/kubesrv`.

#### 1. Login to Registry
```bash
export HELM_EXPERIMENTAL_OCI=1
echo $GITHUB_TOKEN | helm registry login ghcr.io -u <username> --password-stdin
```

#### 2. Install the Chart
```bash
helm install my-kubesrv oci://ghcr.io/bansikah22/charts/kubesrv --version 0.1.0
```

#### 3. Inspect the Chart
```bash
helm show chart oci://ghcr.io/bansikah22/charts/kubesrv --version 0.1.0
```
