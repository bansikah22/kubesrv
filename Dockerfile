FROM alpine:3.19 AS builder
RUN apk add --no-cache gcc musl-dev
WORKDIR /build
COPY src/ src/
COPY static/ static/
COPY scripts/ scripts/
RUN chmod +x scripts/embed-static.sh && ./scripts/embed-static.sh
RUN gcc -Os -static -s -fno-ident -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I src -o kubesrv src/main.c src/server.c src/http.c

FROM scratch
COPY --from=builder /build/kubesrv /kubesrv
EXPOSE 80
ENV PORT=80
ENTRYPOINT ["/kubesrv"]
