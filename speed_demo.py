import json
import math
import os
import random
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import webbrowser
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "build"
SERVER_BIN = BUILD_DIR / "monitor_server"
HOST = "127.0.0.1"
PORT = 8080

RUN = True


def log(msg: str) -> None:
    print(f"[QUICK_DEMO] {msg}", flush=True)


def run_cmd(cmd: list[str]) -> None:
    log("Выполняется: " + " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def ensure_build() -> None:
    if SERVER_BIN.exists():
        return
    log("Бинарник monitor_server не найден запуск сборки")
    run_cmd(["cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"])
    run_cmd(["cmake", "--build", "build", "-j"])


def http_post(path: str, body: dict) -> None:
    req = urllib.request.Request(
        url=f"http://{HOST}:{PORT}{path}",
        data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=3):
        pass


def wait_server_ready(timeout_sec: float = 20.0) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(f"http://{HOST}:{PORT}/metrics", timeout=1):
                log("Сервер готов")
                return
        except Exception:
            time.sleep(0.2)
    raise RuntimeError("Сервер не ответил на /metrics в заданный таймаут")


def point_in_radius(center_lat: float, center_lon: float, radius_km: float) -> tuple[float, float]:
    earth_r = 6371.0088
    dist = radius_km * math.sqrt(random.random())
    brng = random.random() * 2.0 * math.pi
    lat1 = math.radians(center_lat)
    lon1 = math.radians(center_lon)
    dr = dist / earth_r

    lat2 = math.asin(math.sin(lat1) * math.cos(dr) + math.cos(lat1) * math.sin(dr) * math.cos(brng))
    lon2 = lon1 + math.atan2(
        math.sin(brng) * math.sin(dr) * math.cos(lat1),
        math.cos(dr) - math.sin(lat1) * math.sin(lat2),
    )
    return math.degrees(lat2), math.degrees(lon2)


def make_payload(ts: int) -> dict:
    lat, lon = point_in_radius(56.8389, 60.6057, 1000.0)
    return {
        "lat": lat,
        "lon": lon,
        "ts": ts,
        "flags": {
            "wifi": random.random() > 0.15,
            "mobile": random.random() > 0.2,
            "whitelist": random.random() > 0.25,
            "non_whitelist": random.random() > 0.3,
            "vpn": random.random() > 0.7,
            "yt_tg_no_vpn": random.random() > 0.2,
            "geolocation": True,
        },
    }


def feeder(rate_per_sec: int, duration_sec: int) -> None:
    global RUN
    sent = 0
    ts = 1715000000
    tick = 1.0 / max(1, rate_per_sec)
    started = time.time()
    while RUN and (time.time() - started) < duration_sec:
        payload = make_payload(ts)
        ts += 1
        try:
            http_post("/ingest", payload)
            sent += 1
            if sent % 1000 == 0:
                log(f"Отправлено тестовых сообщений: {sent}")
        except urllib.error.URLError as exc:
            log(f"Ошибка отправки: {exc}")
            time.sleep(0.3)
        time.sleep(tick)
    log(f"Подача тестовых данных завершена, всего отправлено: {sent}")


def read_metrics() -> str:
    try:
        with urllib.request.urlopen(f"http://{HOST}:{PORT}/metrics", timeout=2) as resp:
            return resp.read().decode("utf-8")
    except Exception as exc:
        return f'{{"error":"{exc}"}}'


def stop_server(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    log("Останавливаю сервер")
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()


def main() -> int:
    global RUN
    duration_sec = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    rate_per_sec = int(sys.argv[2]) if len(sys.argv) > 2 else 300

    def on_signal(_sig, _frame):
        nonlocal duration_sec
        duration_sec = 0
        RUN = False

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    ensure_build()
    log("Запускаю monitor_server")
    server_proc = subprocess.Popen([str(SERVER_BIN)], cwd=ROOT)

    try:
        wait_server_ready()
        url = f"http://{HOST}:{PORT}/"
        log(f"Открываю UI: {url}")
        webbrowser.open(url)

        feeder_thread = threading.Thread(target=feeder, args=(rate_per_sec, duration_sec), daemon=True)
        feeder_thread.start()

        log(f"Демо запущено на {duration_sec} сек, скорость {rate_per_sec} msg/s. Нажмите Ctrl+C для остановки.")
        started = time.time()
        while RUN and (time.time() - started) < duration_sec:
            time.sleep(2)
            log("Текущие метрики: " + read_metrics())

        RUN = False
        feeder_thread.join(timeout=3)
        log("Финальные метрики: " + read_metrics())
    finally:
        stop_server(server_proc)

    log("Быстрый тест завершён")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
