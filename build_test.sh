#!/usr/bin/env bash
set -euo pipefail

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -TERM "$SERVER_PID" || true
    wait "$SERVER_PID" || true
  fi
}
trap cleanup EXIT INT TERM

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

./build/monitor_server &
SERVER_PID=$!
sleep 3

./build/loadgen 1000000 4 || true

curl -fsS "http://127.0.0.1:8080/metrics" | jq .
curl -fsS "http://127.0.0.1:8080/" >/dev/null

kill -TERM "$SERVER_PID"
wait "$SERVER_PID" || true

echo "[BUILD_TEST] Готово"
