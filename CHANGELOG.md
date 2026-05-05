# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-05-05

### Added
- **Helm Chart**: Initial release of the production-grade Helm chart for `kubesrv`.
- **Security Hardening**: Enforced `runAsNonRoot`, `readOnlyRootFilesystem`, and dropped all capabilities in both deployment and test pods.
- **CI/CD Pipeline**: Created `helm-cd.yml` workflow for automated linting and security scanning with `kube-linter`.
- **Integration Testing**: Added KinD (Kubernetes in Docker) deployment preview and `helm test` execution on Pull Requests.
- **OCI Support**: Enabled automatic Helm chart publishing to GitHub Container Registry (GHCR) as an OCI artifact using `helm-v*` tags.
- **Documentation**: Added `docs/HELM.md` and updated `README.md` with installation guides and status badges.

### Fixed
- **Git Visibility**: Fixed `.gitignore` rule that was accidentally hiding the `charts/` directory from Git tracking.
- **Linter Compliance**: Hardened the `test-connection` pod with proper security contexts and resource limits to pass high-security linting rules.
- **Workflow Reliability**: Implemented SHA256 checksum verification for downloaded binaries and switched to official GitHub Actions for better stability.
