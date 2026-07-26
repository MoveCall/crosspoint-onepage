#!/usr/bin/env python3
"""
OnePage (ESP32-C61) 烧录上位机 — Tkinter GUI.

IMPORTANT: run with a Python whose Tk is >= 8.6 (macOS system Python ships the
broken Tk 8.5 that renders blank windows). Use Homebrew's python3.12 after
`brew install python-tk@3.12`:

    /opt/homebrew/bin/python3.12 scripts/flash_ui.py

The GUI itself only needs tkinter. Flashing runs esptool in a subprocess using a
Python that HAS esptool (the system python3), auto-detected below — so the GUI
Python doesn't need esptool installed.

Features: port dropdown + refresh, stub/no-stub toggle (compression), baud,
live progress bar, live elapsed timer, and success judged by the
"Hash of data verified" count (4/4), not the exit code.

Flash mode note (confirmed by experiment): esptool only compresses with the
STUB loader, and stub support depends on the external SPI flash vendor:
  - Puya PY25Q128HA  : stub CRASHES (~5s) -> must use no-stub (~500s, reliable)
  - Winbond W25Q128JW: stub works, compressed, ~114s (4.4x faster)
Pick stub only if the board has a stub-compatible flash (Winbond etc.).
"""

import glob
import os
import re
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk, scrolledtext

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE = os.path.join(PROJECT_DIR, ".pio", "build", "onepage", "onepage-full.bin")
DEFAULT_BAUD = "1152000"
# Progress lines look like "Writing at 0x... 6.1% 229376/3781312 bytes"
# (older esptool used "(6 %)"). Match bare/decimal percent, parens optional.
PCT_RE = re.compile(r"(\d+(?:\.\d+)?)\s*%")


def find_esptool_python():
    """Return a python executable that can `import esptool` (for the subprocess)."""
    candidates = [
        os.path.expanduser("~/.platformio/penv/bin/python"),
        "/usr/bin/python3",
        sys.executable,
    ]
    for py in candidates:
        if not py or not os.path.exists(py):
            continue
        try:
            r = subprocess.run([py, "-c", "import esptool"], capture_output=True, timeout=10)
            if r.returncode == 0:
                return py
        except Exception:
            continue
    return sys.executable  # last resort; may fail, surfaced in log


class FlashUI:
    def __init__(self, root):
        self.root = root
        root.title("OnePage 烧录工具")
        root.geometry("640x500")
        root.minsize(560, 420)

        self.proc = None
        self.start_time = None
        self.timer_running = False
        self.hash_count = 0
        self.esptool_py = find_esptool_python()

        pad = {"padx": 8, "pady": 4}

        # Row 1: port + refresh
        r1 = ttk.Frame(root)
        r1.pack(fill="x", **pad)
        ttk.Label(r1, text="端口:").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(r1, textvariable=self.port_var, width=26, state="readonly")
        self.port_combo.pack(side="left", padx=4)
        ttk.Button(r1, text="刷新", width=6, command=self.refresh_ports).pack(side="left")

        # Row 2: mode (stub/no-stub) + baud
        r2 = ttk.Frame(root)
        r2.pack(fill="x", **pad)
        ttk.Label(r2, text="模式:").pack(side="left")
        self.stub_var = tk.StringVar(value="nostub")
        ttk.Radiobutton(r2, text="no-stub (无压缩/稳)", variable=self.stub_var, value="nostub").pack(side="left", padx=4)
        ttk.Radiobutton(r2, text="stub (压缩/快, 可能崩)", variable=self.stub_var, value="stub").pack(side="left", padx=4)
        ttk.Label(r2, text="波特率:").pack(side="left", padx=(12, 2))
        self.baud_var = tk.StringVar(value=DEFAULT_BAUD)
        ttk.Combobox(r2, textvariable=self.baud_var, width=9, state="readonly",
                     values=["1152000", "921600", "460800", "115200"]).pack(side="left")

        # Row 3: firmware info + flash button
        r3 = ttk.Frame(root)
        r3.pack(fill="x", **pad)
        fw_ok = os.path.exists(FIRMWARE)
        size = f"{os.path.getsize(FIRMWARE)/1024/1024:.2f} MB" if fw_ok else "缺失!"
        ttk.Label(r3, text=f"固件: onepage-full.bin ({size})",
                  foreground=("black" if fw_ok else "red")).pack(side="left")
        self.flash_btn = ttk.Button(r3, text="开始烧录", command=self.start_flash)
        self.flash_btn.pack(side="right")

        # Progress + timer
        r4 = ttk.Frame(root)
        r4.pack(fill="x", **pad)
        self.progress = ttk.Progressbar(r4, mode="determinate", maximum=100)
        self.progress.pack(fill="x")
        r5 = ttk.Frame(root)
        r5.pack(fill="x", padx=8)
        self.pct_label = ttk.Label(r5, text="0 %", font=("Menlo", 13))
        self.pct_label.pack(side="left")
        self.hash_label = ttk.Label(r5, text="校验 0/4", font=("Menlo", 13))
        self.hash_label.pack(side="left", padx=16)
        self.time_label = ttk.Label(r5, text="耗时 0.0 s", font=("Menlo", 13, "bold"))
        self.time_label.pack(side="right")

        self.status = ttk.Label(root, text="就绪", padding=(8, 2))
        self.status.pack(fill="x")

        self.log = scrolledtext.ScrolledText(root, height=14, font=("Menlo", 10))
        self.log.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        self.refresh_ports()

    def refresh_ports(self):
        ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*"))
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        if not ports:
            self.port_var.set("")

    def log_line(self, t):
        self.log.insert("end", t + "\n")
        self.log.see("end")

    def set_status(self, t, color="black"):
        self.status.config(text=t, foreground=color)

    def tick(self):
        if self.timer_running and self.start_time:
            self.time_label.config(text=f"耗时 {time.time()-self.start_time:.1f} s")
            self.root.after(100, self.tick)

    def start_flash(self):
        port = self.port_var.get()
        if not port:
            self.set_status("请先选择端口（点刷新）", "red")
            return
        if not os.path.exists(FIRMWARE):
            self.set_status("固件缺失，请先构建合并", "red")
            return
        self.flash_btn.config(state="disabled")
        self.log.delete("1.0", "end")
        self.progress["value"] = 0
        self.pct_label.config(text="0 %")
        self.hash_count = 0
        self.hash_label.config(text="校验 0/4")
        self.start_time = time.time()
        self.timer_running = True
        stub = self.stub_var.get() == "stub"
        self.set_status(f"烧录中 {port}  ({'stub' if stub else 'no-stub'}) ...", "blue")
        self.tick()
        threading.Thread(target=self._worker, args=(port, stub, self.baud_var.get()), daemon=True).start()

    def _worker(self, port, stub, baud):
        cmd = [self.esptool_py, "-m", "esptool", "--chip", "esp32c61", "--port", port, "--baud", baud]
        if not stub:
            cmd.append("--no-stub")
        cmd += ["--before", "default_reset", "--after", "hard_reset",
                "write_flash", "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB",
                "0x0", FIRMWARE]
        self.root.after(0, self.log_line, "$ " + " ".join(cmd))
        try:
            self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                         text=True, bufsize=1, cwd=PROJECT_DIR)
        except Exception as e:
            self.root.after(0, self._finish, False, f"启动失败: {e}")
            return
        for raw in self.proc.stdout:
            line = raw.rstrip("\n")
            if not line:
                continue
            m = PCT_RE.search(line)
            if m:
                # Progress line: update the bar only, don't spam the log.
                self.root.after(0, self._set_pct, int(float(m.group(1))))
            else:
                self.root.after(0, self.log_line, line)
            if "Hash of data verified" in line:
                self.hash_count += 1
                self.root.after(0, lambda n=self.hash_count: self.hash_label.config(text=f"校验 {n}/4"))
        self.proc.wait()
        self.root.after(0, self._finish, self.hash_count >= 4, None)

    def _set_pct(self, pct):
        self.progress["value"] = pct
        self.pct_label.config(text=f"{pct} %")

    def _finish(self, ok, err):
        self.timer_running = False
        elapsed = time.time() - self.start_time if self.start_time else 0
        self.time_label.config(text=f"耗时 {elapsed:.1f} s")
        self.flash_btn.config(state="normal")
        if err:
            self.set_status(err, "red")
        elif ok:
            self.progress["value"] = 100
            self.pct_label.config(text="100 %")
            self.set_status(f"✓ 烧录成功！4/4 校验通过，耗时 {elapsed:.1f}s", "green")
        else:
            self.set_status(f"✗ 烧录失败 ({self.hash_count}/4)，耗时 {elapsed:.1f}s — 看日志", "red")


def main():
    if tk.TkVersion < 8.6:
        sys.stderr.write(
            f"错误: Tk {tk.TkVersion} 太旧（macOS 会显示空白窗口）。\n"
            "请用带 Tk>=8.6 的 Python 运行：\n"
            "  brew install python-tk@3.12\n"
            "  /opt/homebrew/bin/python3.12 scripts/flash_ui.py\n")
        sys.exit(1)
    root = tk.Tk()
    FlashUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
