.PHONY: help build run test clean docker-build docker-run docker-test k8s-deploy k8s-test k8s-clean

BINARY := kubesrv
IMAGE := bansikah/kubesrv:latest
PORT := 8080

help:
	@echo "kubesrv - Makefile commands"
	@echo ""
	@echo "Local Development:"
	@echo "  make build        - Build binary locally"
	@echo "  make run          - Build and run locally on port $(PORT)"
	@echo "  make test         - Run local tests"
	@echo "  make clean        - Clean build artifacts"
	@echo ""
	@echo "Docker:"
	@echo "  make docker-build - Build Docker image"
	@echo "  make docker-run   - Run Docker container on port $(PORT)"
	@echo "  make docker-test  - Test Docker container"
	@echo ""
	@echo "Kubernetes:"
	@echo "  make k8s-deploy   - Deploy to Kubernetes"
	@echo "  make k8s-test     - Test Kubernetes deployment"
	@echo "  make k8s-clean    - Remove from Kubernetes"

build:
	@echo "=== Generating static files ==="
	@chmod +x scripts/embed-static.sh
	@./scripts/embed-static.sh
	@echo ""
	@echo "=== Compiling kubesrv ==="
	gcc -Os -I src -o $(BINARY) src/main.c src/server.c src/http.c
	@echo ""
	@echo "=== Build complete ==="
	@ls -lh $(BINARY)

run: build
	@echo ""
	@echo "=== Starting kubesrv on port $(PORT) ==="
	@echo "Press Ctrl+C to stop"
	@echo ""
	PORT=$(PORT) \
	MESSAGE="Local Development" \
	POD_NAME="local-pod" \
	POD_NAMESPACE="local-ns" \
	POD_IP="127.0.0.1" \
	NODE_NAME="local-node" \
	./$(BINARY)

test: build
	@echo "=== Starting kubesrv for testing ==="
	@PORT=$(PORT) ./$(BINARY) & \
	SERVER_PID=$$!; \
	sleep 2; \
	echo ""; \
	echo "=== Running tests ==="; \
	echo ""; \
	echo -n "Testing /healthz... "; \
	curl -sf http://localhost:$(PORT)/healthz > /dev/null && echo "PASS" || echo "FAIL"; \
	echo -n "Testing /info... "; \
	curl -sf http://localhost:$(PORT)/info > /dev/null && echo "PASS" || echo "FAIL"; \
	echo -n "Testing /identity... "; \
	curl -sf http://localhost:$(PORT)/identity > /dev/null && echo "PASS" || echo "FAIL"; \
	echo -n "Testing /echo... "; \
	curl -sf http://localhost:$(PORT)/echo > /dev/null && echo "PASS" || echo "FAIL"; \
	echo -n "Testing /fail?code=500... "; \
	curl -sf http://localhost:$(PORT)/fail?code=500 > /dev/null 2>&1 && echo "FAIL" || echo "PASS"; \
	echo -n "Testing /sleep?ms=100... "; \
	curl -sf http://localhost:$(PORT)/sleep?ms=100 > /dev/null && echo "PASS" || echo "FAIL"; \
	echo -n "Testing /metrics... "; \
	curl -sf http://localhost:$(PORT)/metrics | grep kubesrv_requests_total > /dev/null && echo "PASS" || echo "FAIL"; \
	echo ""; \
	echo "=== Identity Response ==="; \
	curl -s http://localhost:$(PORT)/identity; \
	echo ""; \
	echo "=== Metrics ==="; \
	curl -s http://localhost:$(PORT)/metrics | grep -E "(requests|failures|uptime)"; \
	echo ""; \
	kill $$SERVER_PID 2>/dev/null || true

clean:
	@echo "=== Cleaning build artifacts ==="
	rm -f $(BINARY)
	rm -rf src/generated/
	@echo "Clean complete"

docker-build:
	@echo "=== Building Docker image ==="
	docker build -t $(IMAGE) .
	@echo ""
	@echo "=== Image built ==="
	docker images | grep kubesrv

docker-run:
	@echo "=== Running Docker container on port $(PORT) ==="
	@echo "Press Ctrl+C to stop"
	@echo ""
	docker run --rm -p $(PORT):80 \
		-e MESSAGE="Docker Test" \
		-e POD_NAME="docker-pod" \
		-e POD_NAMESPACE="docker-ns" \
		$(IMAGE)

docker-test: docker-build
	@echo "=== Starting Docker container for testing ==="
	@docker run -d --rm -p $(PORT):80 \
		-e MESSAGE="Docker Test" \
		--name kubesrv-test \
		$(IMAGE)
	@sleep 2
	@echo ""
	@echo "=== Running tests ==="
	@echo ""
	@echo -n "Testing /healthz... "; \
	curl -sf http://localhost:$(PORT)/healthz > /dev/null && echo "PASS" || echo "FAIL"
	@echo -n "Testing /identity... "; \
	curl -sf http://localhost:$(PORT)/identity > /dev/null && echo "PASS" || echo "FAIL"
	@echo -n "Testing /fail?code=503... "; \
	curl -sf http://localhost:$(PORT)/fail?code=503 > /dev/null 2>&1 && echo "FAIL" || echo "PASS"
	@echo -n "Testing /sleep?ms=200... "; \
	curl -sf http://localhost:$(PORT)/sleep?ms=200 > /dev/null && echo "PASS" || echo "FAIL"
	@echo ""
	@echo "=== Identity ==="
	@curl -s http://localhost:$(PORT)/identity
	@echo ""
	@echo "=== Metrics ==="
	@curl -s http://localhost:$(PORT)/metrics | grep -E "(requests|failures|uptime)"
	@echo ""
	@docker stop kubesrv-test

k8s-deploy:
	@echo "=== Deploying to Kubernetes ==="
	kubectl apply -f k8s/
	@echo ""
	@echo "=== Forcing rollout restart ==="
	kubectl -n kubesrv-ns rollout restart deployment/kubesrv || true
	@echo ""
	@echo "=== Waiting for rollout ==="
	kubectl -n kubesrv-ns rollout status deployment/kubesrv --timeout=180s
	@echo ""
	kubectl get pods -n kubesrv-ns

k8s-test:
	@echo "=== Testing Kubernetes deployment ==="
	@echo ""
	kubectl get pods -n kubesrv-ns
	@echo ""
	@echo "Starting port-forward in background..."
	@kubectl port-forward svc/kubesrv-svc $(PORT):80 -n kubesrv-ns > /dev/null 2>&1 & \
	PF_PID=$$!; \
	sleep 3; \
	echo ""; \
	echo "=== Identity ==="; \
	curl -s http://localhost:$(PORT)/identity; \
	echo ""; \
	echo "=== Info ==="; \
	curl -s http://localhost:$(PORT)/info; \
	echo ""; \
	echo "=== Testing failure ==="; \
	curl -i http://localhost:$(PORT)/fail?code=503 2>&1 | head -1; \
	echo ""; \
	echo "=== Metrics ==="; \
	curl -s http://localhost:$(PORT)/metrics | grep -E "(requests|failures|uptime)"; \
	echo ""; \
	kill $$PF_PID 2>/dev/null || true

k8s-clean:
	@echo "=== Removing from Kubernetes ==="
	kubectl delete -f k8s/ --ignore-not-found=true
	@echo "Cleanup complete"
