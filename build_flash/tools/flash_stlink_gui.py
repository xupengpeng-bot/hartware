# -*- coding: utf-8 -*-
"""ST-Link 烧录界面：默认 st-flash（与 flash.cmd 一致）。设环境变量 USE_STM32_CUBE_CLI=1 才用 STM32CubeProgrammer CLI。"""

import os
import re
import shutil
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk
from typing import List, Optional

try:
    import serial as _pyserial  # type: ignore
except ImportError:
    _pyserial = None


def _query_usb_monitor_win() -> str:
    """Windows：枚举串口 + 与 ST-Link/STM32 相关的 USB 设备（供界面监测）。"""
    ps = r"""
$ErrorActionPreference = 'SilentlyContinue'
$com = [System.IO.Ports.SerialPort]::GetPortNames()
$coms = if ($com -and $com.Count -gt 0) { ($com | Sort-Object) -join ', ' } else { '(当前无串口)' }
$devs = Get-PnpDevice | Where-Object {
    $_.Status -eq 'OK' -and (
        $_.FriendlyName -match 'ST-Link|STM32 STLink|STLINK|STM32 ST-LINK|STMicroelectronics STLink|STLINK-V'
    )
} | Select-Object -ExpandProperty FriendlyName -Unique
$usb = if ($devs) { ($devs | Sort-Object) -join ' | ' } else { '(未检测到 ST-Link/STM32 调试器 USB)' }
"串口: $coms`nUSB: $usb"
"""
    r = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command", ps],
        capture_output=True,
        text=True,
        timeout=12,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0,
    )
    out = (r.stdout or "").strip()
    if r.returncode != 0 and not out:
        err = (r.stderr or "").strip()
        return f"查询失败 (code {r.returncode}): {err or '无输出'}"
    return out or "(无输出)"


def _query_usb_monitor() -> str:
    if sys.platform == "win32":
        return _query_usb_monitor_win()
    return "USB 监测：当前仅 Windows 下提供串口/ST-Link 枚举。"


def _com_port_names_win() -> List[str]:
    """无 pyserial 时枚举 Windows COM 口。"""
    if sys.platform != "win32":
        return []
    ps = "[System.IO.Ports.SerialPort]::GetPortNames()"
    r = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command", ps],
        capture_output=True,
        text=True,
        timeout=8,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0,
    )
    parts = (r.stdout or "").replace("\r", "\n").split()
    return sorted(set(parts), key=lambda s: (len(s), s.upper()))


def list_serial_ports() -> List[str]:
    if _pyserial is not None:
        try:
            from serial.tools import list_ports

            return sorted({p.device for p in list_ports.comports()})
        except Exception:
            pass
    return _com_port_names_win()


def _script_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def _build_flash_dir() -> str:
    return os.path.normpath(os.path.join(_script_dir(), ".."))


def _logs_dir() -> str:
    path = os.path.join(_build_flash_dir(), "logs")
    os.makedirs(path, exist_ok=True)
    return path


def _new_serial_log_paths(port: str):
    stamp = time.strftime("%Y%m%d_%H%M%S")
    safe_port = re.sub(r"[^A-Za-z0-9_.-]+", "_", port.strip()) or "COM"
    base = f"gui_serial_{safe_port}_{stamp}"
    root = _logs_dir()
    return (
        os.path.join(root, base + ".txt"),
        os.path.join(root, base + ".hex.txt"),
        os.path.join(root, base + ".raw.bin"),
    )


def _new_gui_log_path() -> str:
    stamp = time.strftime("%Y%m%d_%H%M%S")
    return os.path.join(_logs_dir(), f"flash_gui_{stamp}.log")


def _bytes_to_debug_text(data: bytes, keep_newlines: bool = True) -> str:
    parts = []
    for b in data:
        if 0x20 <= b <= 0x7E:
            parts.append(chr(b))
        elif b == 0x0D:
            parts.append("\r" if keep_newlines else "\\r")
        elif b == 0x0A:
            parts.append("\n" if keep_newlines else "\\n")
        elif b == 0x09:
            parts.append("\t" if keep_newlines else "\\t")
        else:
            parts.append(f"<{b:02X}>")
    return "".join(parts)


def _default_bin() -> str:
    # 优先选择“最新生成”的 controller_fw.bin，避免 GUI 误用陈旧产物。
    candidates = []
    la = os.environ.get("LOCALAPPDATA")
    if la:
        candidates.extend(
            [
                os.path.join(la, "hw_embedded_build", "verify_netdiag", "controller_fw.bin"),
                os.path.join(la, "hw_embedded_build", "out", "build", "controller_fw.bin"),
            ]
        )
    candidates.append(os.path.join(_build_flash_dir(), "out", "build", "controller_fw.bin"))

    existing = [p for p in candidates if os.path.isfile(p)]
    if existing:
        return max(existing, key=os.path.getmtime)
    return candidates[0] if candidates else os.path.join(_build_flash_dir(), "out", "build", "controller_fw.bin")


def find_stm32_programmer_cli() -> Optional[str]:
    """PATH、STM32_PROG_CLI、STM32_CUBE_PROGRAMMER_BIN、Program Files、resolve 脚本搜索智能体目录"""
    full = os.environ.get("STM32_PROG_CLI")
    if full and os.path.isfile(full):
        return full
    env_bin = os.environ.get("STM32_CUBE_PROGRAMMER_BIN")
    if env_bin:
        exe = os.path.join(env_bin, "STM32_Programmer_CLI.exe")
        if os.path.isfile(exe):
            return exe
    w = shutil.which("STM32_Programmer_CLI") or shutil.which("STM32_Programmer_CLI.exe")
    if w:
        return w
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    cand = os.path.join(
        pf,
        "STMicroelectronics",
        "STM32Cube",
        "STM32CubeProgrammer",
        "bin",
        "STM32_Programmer_CLI.exe",
    )
    if os.path.isfile(cand):
        return cand
    ps1 = os.path.join(_script_dir(), "resolve_stm32_programmer.ps1")
    if os.path.isfile(ps1):
        try:
            r = subprocess.run(
                [
                    "powershell",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    ps1,
                    "-BuildFlashDir",
                    _build_flash_dir(),
                ],
                capture_output=True,
                text=True,
                timeout=120,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0,
            )
            if r.returncode == 0 and r.stdout.strip():
                d = r.stdout.strip().splitlines()[-1].strip()
                exe = os.path.join(d, "STM32_Programmer_CLI.exe")
                if os.path.isfile(exe):
                    return exe
        except (OSError, subprocess.TimeoutExpired):
            pass
    return None


def find_st_flash() -> Optional[str]:
    """本机固定路径优先，其次仓库内路径，最后 PATH。"""
    u = os.environ.get("STFLASH_EXE_USER", "").strip()
    if u and os.path.isfile(u):
        return u
    fixed = r"D:\20251211\智能体\skills\stlink-1.8.0-win32\bin\st-flash.exe"
    if os.path.isfile(fixed):
        return fixed
    bf = _build_flash_dir()
    candidates = [
        os.path.join(bf, "tools", "st-flash.exe"),
        os.path.join(bf, "tools", "stlink-1.8.0-win32", "bin", "st-flash.exe"),
        os.path.normpath(os.path.join(bf, "..", "..", "..", "skills", "stlink-1.8.0-win32", "bin", "st-flash.exe")),
        os.path.normpath(os.path.join(bf, "..", "..", "skills", "stlink-1.8.0-win32", "bin", "st-flash.exe")),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return shutil.which("st-flash") or shutil.which("st-flash.exe")


_ADDR_RE = re.compile(r"^0x[0-9a-fA-F]+$")


def parse_addr(s: str) -> Optional[str]:
    s = s.strip()
    if not s:
        return None
    if _ADDR_RE.match(s):
        return s
    return None


def _flash_bin_addr_sanity_warning(bin_path: str, addr: str) -> bool:
    """
    若 bin 与 Flash 地址明显不匹配，返回 False 表示用户取消烧录。
    在 UI 线程调用（可用 messagebox）。
    """
    bn = os.path.basename(bin_path).replace("\\", "/").lower()
    a = addr.strip().lower()
    if bn == "controller_fw.bin" and a == "0x08000000":
        return messagebox.askokcancel(
            "地址可能错误",
            "controller_fw.bin 链接在 0x08010000，应烧到 0x08010000。\n"
            "若烧到 0x08000000 会覆盖 Bootloader 区，且向量表与运行地址不一致，极易无法启动。\n\n"
            "除非使用 controller_fw_standalone.bin，否则请选「0x08010000」后重试。\n\n"
            "仍要强制烧录吗？",
        )
    if bn == "bootloader.bin" and a == "0x08010000":
        return messagebox.askokcancel(
            "地址可能错误",
            "bootloader.bin 必须烧到 0x08000000（片首）。\n"
            "烧到 0x08010000 将无法从上电向量进入 Bootloader。\n\n仍要强制烧录吗？",
        )
    if "controller_fw_standalone.bin" in bn and a == "0x08010000":
        return messagebox.askokcancel(
            "地址可能错误",
            "controller_fw_standalone.bin 为独立固件，应烧到 0x08000000。\n"
            "烧到 0x08010000 将无法从上电正确运行。\n\n仍要强制烧录吗？",
        )
    return True


class FlashApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("ST-Link 烧录 (st-flash)")
        self.minsize(560, 420)
        self.geometry("640x480")

        if os.environ.get("USE_STM32_CUBE_CLI", "").strip() == "1":
            self._cli_path = find_stm32_programmer_cli()
            self._stflash_path = None if self._cli_path else find_st_flash()
        else:
            self._cli_path = None
            self._stflash_path = find_st_flash()
        self._running = False
        self._pending_flash_after_build = False
        self._usb_after_id: Optional[str] = None
        self._serial_stop = threading.Event()
        self._serial_thread: Optional[threading.Thread] = None
        self._gui_log_path = _new_gui_log_path()
        self._gui_log_fp = open(self._gui_log_path, "w", encoding="utf-8", newline="")
        self._serial_text_fp = None
        self._serial_hex_fp = None
        self._serial_raw_fp = None
        self._serial_text_path = ""
        self._serial_hex_path = ""
        self._serial_raw_path = ""
        self._serial_hex_offset = 0

        pad = {"padx": 8, "pady": 4}

        frm = ttk.Frame(self, padding=8)
        frm.pack(fill=tk.BOTH, expand=True)

        row0 = ttk.Frame(frm)
        row0.pack(fill=tk.X, **pad)
        ttk.Label(row0, text="烧录工具:").pack(side=tk.LEFT)
        _tool = self._cli_path or self._stflash_path
        _tool_txt = _tool or "(未找到 st-flash.exe，请放到 tools 或 skills\\stlink-1.8.0-win32\\bin)"
        self.lbl_cli = ttk.Label(row0, text=_tool_txt, foreground="red" if not _tool else "")
        self.lbl_cli.pack(side=tk.LEFT, padx=(8, 0))

        row1 = ttk.Frame(frm)
        row1.pack(fill=tk.X, **pad)
        ttk.Label(row1, text="固件 .bin:").pack(side=tk.LEFT)
        self.var_bin = tk.StringVar(value=_default_bin())
        ent_bin = ttk.Entry(row1, textvariable=self.var_bin)
        ent_bin.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(8, 4))
        ttk.Button(row1, text="浏览…", command=self._browse_bin).pack(side=tk.RIGHT)

        row2 = ttk.LabelFrame(frm, text="Flash 起始地址", padding=6)
        row2.pack(fill=tk.X, **pad)
        self.var_preset = tk.StringVar(value="app")
        ttk.Radiobutton(
            row2,
            text="0x08000000（片首 / Bootloader 区）",
            variable=self.var_preset,
            value="boot",
            command=self._on_preset,
        ).pack(anchor=tk.W)
        ttk.Radiobutton(
            row2,
            text="0x08010000（应用程序区，与当前工程 bootloader 跳转一致）",
            variable=self.var_preset,
            value="app",
            command=self._on_preset,
        ).pack(anchor=tk.W)
        ttk.Radiobutton(
            row2,
            text="自定义:",
            variable=self.var_preset,
            value="custom",
            command=self._on_preset,
        ).pack(anchor=tk.W)
        row2c = ttk.Frame(row2)
        row2c.pack(fill=tk.X, pady=(4, 0))
        self.var_addr = tk.StringVar(value="0x08010000")
        ttk.Entry(row2c, textvariable=self.var_addr, width=18).pack(side=tk.LEFT)

        row3 = ttk.Frame(frm)
        row3.pack(fill=tk.X, **pad)
        self.var_rst = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            row3,
            text="烧录完成后复位芯片 (CLI: -rst / st-flash: reset)",
            variable=self.var_rst,
        ).pack(side=tk.LEFT)

        row_serial = ttk.LabelFrame(frm, text="串口日志（与固件 USART 调试输出）", padding=6)
        row_serial.pack(fill=tk.X, **pad)
        rs0 = ttk.Frame(row_serial)
        rs0.pack(fill=tk.X)
        ttk.Label(rs0, text="端口:").pack(side=tk.LEFT)
        self.var_com = tk.StringVar()
        self.combo_com = ttk.Combobox(rs0, textvariable=self.var_com, width=11)
        self.combo_com.pack(side=tk.LEFT, padx=(4, 8))
        ttk.Button(rs0, text="刷新 COM", command=self._refresh_com_ports).pack(side=tk.LEFT)
        ttk.Label(rs0, text="波特率:").pack(side=tk.LEFT, padx=(12, 4))
        self.var_baud = tk.StringVar(value="115200")
        ttk.Entry(rs0, textvariable=self.var_baud, width=8).pack(side=tk.LEFT)
        self.var_auto_serial = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            rs0,
            text="烧录成功后自动读串口到下方日志",
            variable=self.var_auto_serial,
        ).pack(side=tk.LEFT, padx=(16, 0))
        rs1 = ttk.Frame(row_serial)
        rs1.pack(fill=tk.X, pady=(6, 0))
        self.btn_serial_start = ttk.Button(rs1, text="开始读串口", command=self._start_serial_monitor)
        self.btn_serial_start.pack(side=tk.LEFT)
        self.btn_serial_stop = ttk.Button(
            rs1,
            text="停止读串口",
            command=lambda: self._stop_serial_monitor(log_it=True),
            state=tk.DISABLED,
        )
        self.btn_serial_stop.pack(side=tk.LEFT, padx=(8, 0))
        if _pyserial is None:
            ttk.Label(
                rs1,
                text="（未安装 pyserial，请 pip install pyserial）",
                foreground="red",
            ).pack(side=tk.LEFT, padx=(12, 0))

        usb_frame = ttk.LabelFrame(frm, text="USB / 串口烧录监测", padding=6)
        usb_frame.pack(fill=tk.X, **pad)
        row_usb0 = ttk.Frame(usb_frame)
        row_usb0.pack(fill=tk.X)
        self.var_usb_monitor = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            row_usb0,
            text="定时刷新",
            variable=self.var_usb_monitor,
            command=self._on_usb_monitor_toggle,
        ).pack(side=tk.LEFT)
        ttk.Label(row_usb0, text="间隔(秒):").pack(side=tk.LEFT, padx=(12, 4))
        self.var_usb_interval = tk.StringVar(value="2")
        self.spin_usb = ttk.Spinbox(
            row_usb0,
            from_=1,
            to=30,
            width=4,
            textvariable=self.var_usb_interval,
        )
        self.spin_usb.pack(side=tk.LEFT)
        ttk.Button(row_usb0, text="立即刷新", command=self._poll_usb_trigger).pack(side=tk.LEFT, padx=(12, 0))
        self.lbl_usb = ttk.Label(
            usb_frame,
            text="正在查询 USB / 串口…",
            justify=tk.LEFT,
            wraplength=600,
            font=("Consolas", 9),
        )
        self.lbl_usb.pack(anchor=tk.W, fill=tk.X, pady=(6, 0))

        row4 = ttk.Frame(frm)
        row4.pack(fill=tk.X, **pad)
        self.btn_flash = ttk.Button(row4, text="开始烧录 (SWD)", command=self._start_flash)
        self.btn_flash.pack(side=tk.LEFT)
        self.btn_build = ttk.Button(row4, text="编译", command=lambda: self._start_build(False))
        self.btn_build.pack(side=tk.LEFT, padx=(12, 0))
        self.btn_build_flash = ttk.Button(
            row4,
            text="编译·烧录·串口",
            command=lambda: self._start_build(True),
        )
        self.btn_build_flash.pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(row4, text="打开构建目录", command=self._open_build_dir).pack(side=tk.LEFT, padx=(12, 0))

        row_log = ttk.Frame(frm)
        row_log.pack(fill=tk.X, **pad)
        ttk.Label(row_log, text="日志:").pack(side=tk.LEFT, anchor=tk.W)
        ttk.Button(
            row_log,
            text="清空显示",
            command=self._clear_log_view,
        ).pack(side=tk.RIGHT)
        self.txt = scrolledtext.ScrolledText(frm, height=14, wrap=tk.WORD, font=("Consolas", 9))
        self.txt.pack(fill=tk.BOTH, expand=True, **pad)

        self._log(
            "说明: 默认 st-flash，参数与 flash.cmd 一致（--connect-under-reset --freq=100）。"
            "若 NRST is not connected：请接 NRST，或设 STFLASH_FLAGS 含 --hot-plug。\n"
            "需要 STM32CubeProgrammer 时请设置 USE_STM32_CUBE_CLI=1。\n"
            "「编译」在 build_flash 目录执行 build.cmd；「编译·烧录·串口」会编译、烧录，"
            "若勾选「烧录成功后自动读串口」则在烧录成功后打开当前端口读日志（需 pip install pyserial）。\n"
            "\n【推荐烧录流程 — 分区 APP（VTOR=0x08010000）】\n"
            "  1) build_all.cmd 或至少编过 bootloader 目标，得到 bootloader.bin。\n"
            "  2) 先烧 bootloader.bin → 地址选 0x08000000。\n"
            "  3) 再烧 controller_fw.bin → 地址选 0x08010000。\n"
            "  或命令行: build_flash\\flash_full.cmd\n"
            "【仅升级 APP】已有 bootloader 时: 只烧 controller_fw.bin → 0x08010000（勿写到 0x08000000）。\n"
            "【无 Bootloader 排障】controller_fw_standalone.bin → 0x08000000（见 CMake 目标 controller_fw_standalone）。\n"
            "【串口日志】波特率 115200（与固件 UART5 一致），接 MCU PC12=TX5、GND；COM 被占用会 PermissionError。\n"
            f"GUI 运行日志保存到: {self._gui_log_path}\n"
            f"串口日志目录: {_logs_dir()}（开始读串口后会生成 .txt / .hex.txt / .raw.bin）\n"
        )
        if not self._cli_path and not self._stflash_path:
            self._log(
                "错误: 未找到 st-flash.exe。可复制到 build_flash\\tools\\st-flash.exe 或"
                " tools\\stlink-1.8.0-win32\\bin\\，或 智能体\\skills\\stlink-1.8.0-win32\\bin\\。\n"
            )

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(400, self._poll_usb_trigger)
        self._refresh_com_ports()
        self._refresh_serial_buttons()

    def _on_close(self) -> None:
        self._stop_serial_monitor(log_it=False)
        if self._usb_after_id is not None:
            try:
                self.after_cancel(self._usb_after_id)
            except tk.TclError:
                pass
            self._usb_after_id = None
        try:
            if self._gui_log_fp is not None:
                self._gui_log_fp.flush()
                self._gui_log_fp.close()
        except Exception:
            pass
        self.destroy()

    def _close_serial_log_files(self) -> None:
        for attr in ("_serial_text_fp", "_serial_hex_fp", "_serial_raw_fp"):
            fp = getattr(self, attr, None)
            if fp is not None:
                try:
                    fp.flush()
                except Exception:
                    pass
                try:
                    fp.close()
                except Exception:
                    pass
                setattr(self, attr, None)
        self._serial_text_path = ""
        self._serial_hex_path = ""
        self._serial_raw_path = ""
        self._serial_hex_offset = 0

    def _write_serial_hex_chunk(self, data: bytes) -> None:
        if self._serial_hex_fp is None or not data:
            return
        for start in range(0, len(data), 16):
            line = data[start : start + 16]
            hex_text = " ".join(f"{b:02X}" for b in line)
            if len(line) < 16:
                hex_text += "   " * (16 - len(line))
            ascii_text = "".join(chr(b) if 0x20 <= b <= 0x7E else "." for b in line).ljust(16)
            self._serial_hex_fp.write(f"{self._serial_hex_offset:08X}  {hex_text}  |{ascii_text}|\n")
            self._serial_hex_offset += len(line)
        self._serial_hex_fp.flush()

    def _on_usb_monitor_toggle(self) -> None:
        if self.var_usb_monitor.get():
            self._poll_usb_trigger()
        elif self._usb_after_id is not None:
            try:
                self.after_cancel(self._usb_after_id)
            except tk.TclError:
                pass
            self._usb_after_id = None

    def _poll_usb_trigger(self) -> None:
        if self._usb_after_id is not None:
            try:
                self.after_cancel(self._usb_after_id)
            except tk.TclError:
                pass
            self._usb_after_id = None
        threading.Thread(target=self._poll_usb_thread, daemon=True).start()

    def _poll_usb_thread(self) -> None:
        try:
            txt = _query_usb_monitor()
        except (OSError, subprocess.TimeoutExpired) as e:
            txt = f"查询异常: {e}"
        self.after(0, lambda t=txt: self._apply_usb_status(t))

    def _apply_usb_status(self, txt: str) -> None:
        self.lbl_usb.configure(text=txt)
        if not self.var_usb_monitor.get():
            return
        try:
            sec = max(1, min(60, int(float(self.var_usb_interval.get().strip() or "2"))))
        except ValueError:
            sec = 2
        self._usb_after_id = self.after(sec * 1000, self._poll_usb_trigger)

    def _on_preset(self) -> None:
        p = self.var_preset.get()
        if p == "boot":
            self.var_addr.set("0x08000000")
        elif p == "app":
            self.var_addr.set("0x08010000")
        # custom: 不自动改

    def _browse_bin(self) -> None:
        path = filedialog.askopenfilename(
            title="选择固件",
            filetypes=[("BIN 文件", "*.bin"), ("所有文件", "*.*")],
            initialdir=os.path.dirname(self.var_bin.get()) or _build_flash_dir(),
        )
        if path:
            self.var_bin.set(path)

    def _open_build_dir(self) -> None:
        la = os.environ.get("LOCALAPPDATA")
        d = (
            os.path.join(la, "hw_embedded_build", "out", "build")
            if la
            else os.path.join(_build_flash_dir(), "out", "build")
        )
        if not os.path.isdir(d):
            d = os.path.join(_build_flash_dir(), "out", "build")
        os.makedirs(d, exist_ok=True)
        try:
            os.startfile(d)  # type: ignore[attr-defined]
        except OSError as e:
            messagebox.showerror("错误", str(e))

    def _log(self, s: str) -> None:
        self.txt.insert(tk.END, s)
        self.txt.see(tk.END)
        try:
            if self._gui_log_fp is not None:
                self._gui_log_fp.write(s)
                self._gui_log_fp.flush()
        except Exception:
            pass

    def _clear_log_view(self) -> None:
        """仅清空界面上的日志区；flash_gui_*.log 与串口 .txt/.hex.txt/.raw.bin 不受影响。"""
        self.txt.delete("1.0", tk.END)

    def _refresh_com_ports(self) -> None:
        ports = list_serial_ports()
        self.combo_com["values"] = ports
        cur = self.var_com.get().strip()
        if cur and cur in ports:
            return
        if ports:
            self.var_com.set(ports[-1])

    def _refresh_serial_buttons(self) -> None:
        ser_alive = self._serial_thread is not None and self._serial_thread.is_alive()
        busy = self._running
        self.btn_serial_start.configure(
            state=tk.DISABLED if (busy or ser_alive or _pyserial is None) else tk.NORMAL,
        )
        self.btn_serial_stop.configure(state=tk.NORMAL if ser_alive else tk.DISABLED)

    def _set_action_buttons_busy(self, busy: bool) -> None:
        st = tk.DISABLED if busy else tk.NORMAL
        self.btn_flash.configure(state=st)
        self.btn_build.configure(state=st)
        self.btn_build_flash.configure(state=st)
        self._refresh_serial_buttons()

    def _stop_serial_monitor(self, log_it: bool = False) -> None:
        self._serial_stop.set()
        th = self._serial_thread
        if th is not None and th.is_alive():
            th.join(timeout=5.0)
        self._serial_thread = None
        self._close_serial_log_files()
        if log_it:
            self._log("\n[串口] 已停止。\n")
        self._refresh_serial_buttons()

    def _serial_cleanup_after_worker(self) -> None:
        self._serial_thread = None
        self._close_serial_log_files()
        self._refresh_serial_buttons()

    def _log_serial_chunk(self, data: bytes) -> None:
        if not data:
            return
        try:
            if self._serial_raw_fp is not None:
                self._serial_raw_fp.write(data)
                self._serial_raw_fp.flush()
            if self._serial_text_fp is not None:
                self._serial_text_fp.write(_bytes_to_debug_text(data))
                self._serial_text_fp.flush()
            self._write_serial_hex_chunk(data)
        except Exception:
            pass
        try:
            s = _bytes_to_debug_text(data)
        except Exception:
            s = repr(data)
        self._log(s)

    def _serial_worker(self, port: str, baud: int) -> None:
        assert _pyserial is not None
        ser = None
        try:
            ser = _pyserial.Serial(port, baudrate=baud, timeout=0.15)
        except Exception as e:
            self.after(0, lambda err=str(e): self._log(f"\n[串口] 打开失败: {err}\n"))
            self.after(0, lambda err=str(e): messagebox.showerror("串口", err))
            self.after(0, self._serial_cleanup_after_worker)
            return
        try:
            while not self._serial_stop.is_set():
                try:
                    n = ser.in_waiting
                    if n:
                        chunk = ser.read(n)
                        if chunk:
                            self.after(0, lambda c=chunk: self._log_serial_chunk(c))
                    else:
                        time.sleep(0.02)
                except Exception as e:
                    self.after(0, lambda err=str(e): self._log(f"\n[串口] 读取出错: {err}\n"))
                    break
        finally:
            try:
                if ser is not None:
                    ser.close()
            except Exception:
                pass
            self.after(0, self._serial_cleanup_after_worker)

    def _start_serial_monitor(self) -> None:
        if _pyserial is None:
            messagebox.showerror("缺少依赖", "串口功能需要 pyserial：\npip install pyserial")
            return
        if self._running:
            return
        self._stop_serial_monitor(log_it=False)
        self._serial_stop.clear()
        port = self.var_com.get().strip()
        if not port:
            messagebox.showerror("错误", "请选择或输入串口（如 COM3）。")
            return
        try:
            baud = int(self.var_baud.get().strip() or "115200")
            if baud <= 0:
                raise ValueError("波特率须为正整数")
        except ValueError as e:
            messagebox.showerror("错误", f"波特率无效: {e}")
            return
        self._log(f"\n[串口] 打开 {port}，波特率 {baud}\n")
        self._close_serial_log_files()
        self._serial_text_path, self._serial_hex_path, self._serial_raw_path = _new_serial_log_paths(port)
        self._serial_text_fp = open(self._serial_text_path, "w", encoding="utf-8", newline="")
        self._serial_hex_fp = open(self._serial_hex_path, "w", encoding="utf-8", newline="")
        self._serial_raw_fp = open(self._serial_raw_path, "wb")
        self._serial_hex_offset = 0
        self._serial_text_fp.write("=== gui serial capture started ===\n")
        self._serial_text_fp.write(f"port={port}\n")
        self._serial_text_fp.write(f"baud={baud}\n")
        self._serial_text_fp.write(f"local_time={time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        self._serial_text_fp.flush()
        self._serial_hex_fp.write("=== gui serial hex capture started ===\n")
        self._serial_hex_fp.write(f"port={port}\n")
        self._serial_hex_fp.write(f"baud={baud}\n")
        self._serial_hex_fp.write(f"local_time={time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        self._serial_hex_fp.flush()
        self._log(f"[serial] text log: {self._serial_text_path}\n")
        self._log(f"[serial] hex log : {self._serial_hex_path}\n")
        self._log(f"[serial] raw log : {self._serial_raw_path}\n")
        t = threading.Thread(target=lambda: self._serial_worker(port, baud), daemon=True)
        self._serial_thread = t
        t.start()
        self._refresh_serial_buttons()

    def _build_cmd_path(self) -> str:
        return os.path.join(_build_flash_dir(), "build.cmd")

    def _start_build(self, then_flash: bool) -> None:
        if self._running:
            return
        self._stop_serial_monitor(log_it=False)
        if sys.platform != "win32":
            messagebox.showerror("错误", "GUI 内编译仅支持 Windows（调用 build.cmd）。")
            return
        bpath = self._build_cmd_path()
        if not os.path.isfile(bpath):
            messagebox.showerror("错误", f"未找到编译脚本:\n{bpath}")
            return
        self._pending_flash_after_build = then_flash
        self._running = True
        self._set_action_buttons_busy(True)
        self._log("\n" + "=" * 60 + "\n")
        self._log(f"编译: cmd /c call build.cmd （工作目录: {_build_flash_dir()}）\n")

        def worker() -> None:
            try:
                proc = subprocess.Popen(
                    ["cmd.exe", "/c", "call", "build.cmd"],
                    cwd=_build_flash_dir(),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    stdin=subprocess.DEVNULL,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
                )
                assert proc.stdout is not None
                for line in proc.stdout:
                    self.after(0, lambda l=line: self._log(l))
                code = proc.wait()
                self.after(0, lambda c=code: self._build_done(c))
            except OSError as e:
                self.after(0, lambda: self._build_fail(str(e)))

        threading.Thread(target=worker, daemon=True).start()

    def _build_fail(self, msg: str) -> None:
        self._running = False
        self._pending_flash_after_build = False
        self._set_action_buttons_busy(False)
        self._log(f"\n[编译异常] {msg}\n")
        messagebox.showerror("错误", msg)

    def _build_done(self, code: int) -> None:
        self._running = False
        self._set_action_buttons_busy(False)
        if code != 0:
            self._log(f"\n[编译失败] 退出码 {code}\n")
            self._pending_flash_after_build = False
            messagebox.showerror("编译失败", f"build.cmd 退出码 {code}，详见日志。")
            return
        self._log(f"\n[编译成功] 退出码 {code}\n")
        db = _default_bin()
        if os.path.isfile(db):
            self.var_bin.set(db)
        if self._pending_flash_after_build:
            self._pending_flash_after_build = False
            self._start_flash()
        else:
            messagebox.showinfo("完成", "编译成功。")

    def _start_flash(self) -> None:
        if self._running:
            return
        self._stop_serial_monitor(log_it=False)
        if not self._cli_path and not self._stflash_path:
            messagebox.showerror(
                "错误",
                "未找到 st-flash.exe。\n"
                "请放到 build_flash\\tools\\st-flash.exe 或 tools\\stlink-1.8.0-win32\\bin\\，或 智能体\\skills 下含 st-flash 的目录。",
            )
            return
        bin_path = self.var_bin.get().strip()
        if not bin_path or not os.path.isfile(bin_path):
            messagebox.showerror("错误", f"找不到固件文件:\n{bin_path}")
            return
        addr = parse_addr(self.var_addr.get())
        if not addr:
            messagebox.showerror("错误", "起始地址格式无效，应为例如 0x08010000")
            return
        if not _flash_bin_addr_sanity_warning(bin_path, addr):
            return

        if self._cli_path:
            cmd = [
                self._cli_path,
                "-c",
                "port=SWD",
                "-w",
                bin_path,
                addr,
                "-v",
            ]
            if self.var_rst.get():
                cmd.append("-rst")
        else:
            assert self._stflash_path
            flags = os.environ.get(
                "STFLASH_FLAGS",
                "--connect-under-reset --freq=100 --flash=256k",
            ).strip()
            extra = flags.split() if flags else []
            cmd = [self._stflash_path, *extra, "write", bin_path, addr]

        self._running = True
        self._set_action_buttons_busy(True)
        self._log("\n" + "=" * 60 + "\n")
        self._log(f"执行: {' '.join(cmd)}\n")

        def worker() -> None:
            try:
                proc = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    stdin=subprocess.DEVNULL,
                    cwd=os.path.dirname(bin_path) or None,
                    creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
                )
                assert proc.stdout is not None
                for raw in proc.stdout:
                    line = raw.decode("utf-8", errors="replace")
                    self.after(0, lambda l=line: self._log(l))
                code = proc.wait()
                if code == 0 and self._stflash_path and self.var_rst.get():
                    r2 = subprocess.run(
                        [self._stflash_path, "reset"],
                        capture_output=True,
                        text=True,
                        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
                    )
                    if r2.stdout:
                        self.after(0, lambda: self._log(r2.stdout))
                    if r2.stderr:
                        self.after(0, lambda: self._log(r2.stderr))
                    if r2.returncode != 0:
                        self.after(
                            0,
                            lambda: self._log("提示: st-flash reset 未成功时可手动复位板子。\n"),
                        )
                self.after(0, lambda: self._flash_done(code))
            except OSError as e:
                self.after(0, lambda: self._flash_fail(str(e)))

        threading.Thread(target=worker, daemon=True).start()

    def _flash_done(self, code: int) -> None:
        self._running = False
        self._set_action_buttons_busy(False)
        if code == 0:
            self._log(f"\n[完成] 退出码 {code}\n")
            want_serial = self.var_auto_serial.get()
            if want_serial and _pyserial is None:
                messagebox.showinfo(
                    "完成",
                    "烧录成功。\n未安装 pyserial，无法自动读串口。请执行: pip install pyserial",
                )
            elif want_serial:
                messagebox.showinfo(
                    "完成",
                    "烧录成功。\n随后将打开串口并输出日志到下方。",
                )
                self.after(400, self._start_serial_monitor)
            else:
                messagebox.showinfo("完成", "烧录成功。")
        else:
            self._log(f"\n[失败] 退出码 {code}\n")
            messagebox.showerror(
                "失败",
                f"烧录失败，退出码 {code}。\n"
                "请检查 ST-Link、SWD、NRST 与供电；可试环境变量 STFLASH_FLAGS 含\n"
                "--connect-under-reset --freq=100 --hot-plug",
            )

    def _flash_fail(self, msg: str) -> None:
        self._running = False
        self._set_action_buttons_busy(False)
        self._log(f"\n[异常] {msg}\n")
        messagebox.showerror("错误", msg)


def main() -> None:
    app = FlashApp()
    app.mainloop()


if __name__ == "__main__":
    main()
