FROM alpine:3.24 AS builder
RUN apk add --no-cache gcc musl-dev wget
WORKDIR /build
COPY src/ src/
COPY static/ static/
COPY scripts/ scripts/
RUN chmod +x scripts/embed-static.sh && ./scripts/embed-static.sh
RUN gcc -Os -static -s -fno-ident -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I src -o kubesrv src/main.c src/server.c src/http.c

# Download static busybox for shell access with integrity verification
RUN wget -O /build/busybox https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox && \
    echo "6e123e7f3202a8c1e9b1f94d8941580a25135382b99e8d3e34fb858bba311348  /build/busybox" | sha256sum -c - && \
    chmod +x /build/busybox

FROM scratch
COPY --from=builder /build/kubesrv /kubesrv
COPY --from=builder /build/busybox /bin/busybox
RUN ["/bin/busybox", "--install", "-s", "/bin"]
EXPOSE 80
ENV PORT=80
ENTRYPOINT ["/kubesrv"]
