FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git pkg-config \
    libboost-all-dev libssl-dev ca-certificates curl jq \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j"$(nproc)" && \
    ctest --test-dir build --output-on-failure

FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl jq libboost-system1.74.0 libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/internet-monitor
COPY --from=builder /app/build/monitor_server /opt/internet-monitor/monitor_server
COPY --from=builder /app/build/loadgen /opt/internet-monitor/loadgen
COPY --from=builder /app/frontend /opt/internet-monitor/frontend
COPY --from=builder /app/build_test.sh /opt/internet-monitor/build_test.sh
RUN sed -i 's/\r$//' /opt/internet-monitor/build_test.sh
RUN chmod +x /opt/internet-monitor/build_test.sh

EXPOSE 8080 9090
ENV MODE=server

CMD ["/bin/bash", "-lc", "if [ \"$MODE\" = \"server\" ]; then ./monitor_server; elif [ \"$MODE\" = \"test\" ]; then ./monitor_server & sleep 2 && ./loadgen 10000 4; else ./monitor_server & sleep 2 && ./loadgen 1000000 4; fi"]