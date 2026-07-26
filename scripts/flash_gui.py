#!/usr/bin/env python3
"""
OnePage (ESP32-C61) 烧录上位机 — terminal edition.

A dependency-free terminal flasher: auto-detects the C61 serial port, flashes
the merged firmware (.pio/build/onepage/onepage-full.bin), and shows a live
progress bar + elapsed timer. Success is judged by the "Hash of data verified"
count (4/4), not the exit code.

Flash mode (C61 caveat): esptool only compresses uploads when the STUB loader is
used, but the C61 stub crashes on large writes (reboot loop). So:
  --no-stub  (default) : ROM loader, NO compression, slow (~6-7 min) but reliable
  --stub               : stub loader, compressed, fast (~1 min) but may crash on C61
Use --stub only to experiment; --no-stub is the trustworthy path.

(Tkinter GUI was dropped: macOS system Python ships Tk 8.5, which renders blank
windows on modern macOS. A terminal UI is dependency-free and always works.)

Run:  python3 scripts/flash_gui.py [--stub|--no-stub] [--baud N] [--port DEV]
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import threading
import time

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = os.path.join(PROJECT_DIR, ".pio", "build", "onepage", "onepage-full.bin")
DEFAULT_BAUD = "1152000"
# esptool write progress lines look like: "Writing at 0x... 6.1% 229376/3781312 bytes"
# (older versions used "(6 %)"). Match a bare/decimal percent, parens optional.
PCT_RE = re.compile(r"(\d+(?:\.\d+)?)\s*%")

# ANSI
BOLD, GREEN, RED, BLUE, DIM, RESET = "\033[1m", "\033[32m", "\033[31m", "\033[34m", "\033[2m", "\033[0m"


def list_ports():
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*"))
    return ports


def pick_port(explicit):
    if explicit:
        return explicit
    ports = list_ports()
    if not ports:
        print(f"{RED}未找到串口设备。请插入 OnePage 后重试。{RESET}")
        sys.exit(1)
    if len(ports) == 1:
        return ports[0]
    print("检测到多个串口：")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p}")
    while True:
        sel = input("选择端口编号: ").strip()
        if sel.isdigit() and int(sel) < len(ports):
            return ports[int(sel)]


class Progress:
    def __init__(self):
        self.pct = 0
        self.hashes = 0
        self.start = time.time()
        self.done = False
        self.lock = threading.Lock()

    def bar(self):
        with self.lock:
            pct, hashes, elapsed = self.pct, self.hashes, time.time() - self.start
        width = 30
        filled = int(width * pct / 100)
        bar = "█" * filled + "░" * (width - filled)
        return f"\r  [{bar}] {pct:3d}%  校验 {hashes}/4  {BOLD}耗时 {elapsed:5.1f}s{RESET}  "


def ticker(prog):
    """Repaint the elapsed timer even when esptool is quiet (long erase/write)."""
    while not prog.done:
        sys.stdout.write(prog.bar())
        sys.stdout.flush()
        time.sleep(0.2)


def main():
    ap = argparse.ArgumentParser(description="OnePage C61 firmware flasher")
    ap.add_argument("--port", help="serial port (auto-detect if omitted)")
    ap.add_argument("--baud", default=DEFAULT_BAUD, help=f"baud rate (default {DEFAULT_BAUD})")
    grp = ap.add_mutually_exclusive_group()
    grp.add_argument("--stub", dest="stub", action="store_true",
                     help="use stub loader: compressed + fast, but may crash on C61")
    grp.add_argument("--no-stub", dest="stub", action="store_false",
                     help="ROM loader: no compression, slow but reliable (default)")
    ap.set_defaults(stub=False)
    # Legacy positional PORT still works: `flash_gui.py /dev/cu.usbmodemXXXX`
    ap.add_argument("pos_port", nargs="?", help=argparse.SUPPRESS)
    args = ap.parse_args()

    if not os.path.exists(FIRMWARE):
        print(f"{RED}固件缺失: {FIRMWARE}\n请先构建: pio run -e onepage 并合并 onepage-full.bin{RESET}")
        sys.exit(1)

    port = pick_port(args.port or args.pos_port)
    size_mb = os.path.getsize(FIRMWARE) / 1024 / 1024
    mode_label = (f"{RED}stub (压缩/快, C61 可能崩){RESET}" if args.stub
                  else f"{GREEN}no-stub (无压缩/稳){RESET}")
    print(f"{BOLD}OnePage 烧录工具{RESET}")
    print(f"  端口: {BLUE}{port}{RESET}")
    print(f"  固件: onepage-full.bin ({size_mb:.2f} MB)")
    print(f"  模式: {mode_label}   波特率: {args.baud}")
    if args.stub:
        print(f"  {DIM}stub 模式快很多；若出现重启循环请改用默认 --no-stub{RESET}\n")
    else:
        print(f"  {DIM}--no-stub 无压缩，约 6-7 分钟，请勿断开{RESET}\n")

    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32c61", "--port", port,
           "--baud", args.baud]
    if not args.stub:
        cmd.append("--no-stub")
    cmd += ["--before", "default_reset", "--after", "hard_reset",
            "write_flash", "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB",
            "0x0", FIRMWARE]

    prog = Progress()
    t = threading.Thread(target=ticker, args=(prog,), daemon=True)
    t.start()

    err_tail = []
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, bufsize=1, cwd=PROJECT_DIR)
    except Exception as e:
        prog.done = True
        print(f"\n{RED}启动 esptool 失败: {e}{RESET}")
        sys.exit(1)

    for raw in proc.stdout:
        line = raw.rstrip("\n")
        if not line:
            continue
        err_tail.append(line)
        if len(err_tail) > 6:
            err_tail.pop(0)
        m = PCT_RE.search(line)
        if m:
            with prog.lock:
                prog.pct = int(float(m.group(1)))
        if "Hash of data verified" in line:
            with prog.lock:
                prog.hashes += 1
        # surface interesting non-progress lines above the bar
        if any(k in line for k in ("Chip is", "Detecting", "fatal", "error", "Could not", "Hard resetting")):
            sys.stdout.write("\r" + " " * 70 + "\r")  # clear bar line
            print(f"  {DIM}{line}{RESET}")

    proc.wait()
    prog.done = True
    time.sleep(0.25)
    elapsed = time.time() - prog.start
    sys.stdout.write(prog.bar() + "\n")

    if prog.hashes >= 4:
        print(f"\n{GREEN}{BOLD}✓ 烧录成功！{RESET} {GREEN}4/4 校验通过，耗时 {elapsed:.1f}s{RESET}")
        sys.exit(0)
    else:
        print(f"\n{RED}{BOLD}✗ 烧录失败{RESET} {RED}({prog.hashes}/4 校验通过，耗时 {elapsed:.1f}s){RESET}")
        print(f"{DIM}最后输出:{RESET}")
        for l in err_tail:
            print(f"  {l}")
        sys.exit(1)


if __name__ == "__main__":
    main()
