# ConfigMap Examples

## Example 1: Default message.txt

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: kubesrv-config
  namespace: kubesrv-ns
data:
  message.txt: |
    Welcome from ConfigMap!
```

## Example 2: Custom file name

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: kubesrv-custom
  namespace: kubesrv-ns
data:
  greeting.txt: |
    Hello from custom greeting file!
  welcome.txt: |
    Welcome to production!
```

Use with deployment:
```yaml
env:
  - name: MESSAGE_FILE
    value: "greeting.txt"  # or "welcome.txt"
```

## Example 3: Multi-line message

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: kubesrv-multiline
  namespace: kubesrv-ns
data:
  banner.txt: |
    Welcome to kubesrv!
    Environment: Production
    Version: 1.0.0
```

## Example 4: Environment-specific

```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: kubesrv-dev
  namespace: dev
data:
  message.txt: "Development Environment"
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: kubesrv-prod
  namespace: prod
data:
  message.txt: "Production Environment"
```
