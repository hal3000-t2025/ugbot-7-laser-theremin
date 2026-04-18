#!/usr/bin/env python3
"""Laser Theremin serial helper.

This tool intentionally uses only the Python standard library so it can run on
macOS/Linux without requiring pyserial.
"""

from __future__ import annotations

import argparse
import glob
import os
import select
import sys
import termios
import time
import tty
from dataclasses import dataclass
from typing import Iterable, List


DEFAULT_BAUD = 115200
DEFAULT_PORT = "/dev/cu.usbserial-A5069RR4"
DEFAULT_CAPTURE_TIMEOUT = 5.0
DEFAULT_READ_WINDOW = 1.0


def list_serial_ports() -> List[str]:
    patterns = ["/dev/cu.*", "/dev/tty.*"]
    ports: List[str] = []
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))


@dataclass
class DeviceResponse:
    lines: List[str]

    def print(self) -> None:
        for line in self.lines:
            print(line)


class SerialConnection:
    def __init__(self, port: str, baud: int = DEFAULT_BAUD):
        self.port = port
        self.baud = baud
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self._configure()
        self._drain_input()

    def __enter__(self) -> "SerialConnection":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        os.close(self.fd)

    def _configure(self) -> None:
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        tty.setraw(self.fd)

    def _drain_input(self) -> None:
        deadline = time.time() + 0.2
        while time.time() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.01)
            if not ready:
                continue
            try:
                os.read(self.fd, 4096)
            except BlockingIOError:
                break

    def write_line(self, line: str) -> None:
        payload = line.rstrip("\r\n").encode("utf-8") + b"\n"
        os.write(self.fd, payload)

    def read_lines(self, timeout: float, stop_text: str | None = None) -> DeviceResponse:
        deadline = time.time() + timeout
        buffer = b""
        lines: List[str] = []

        while time.time() < deadline:
            ready, _, _ = select.select([self.fd], [], [], 0.05)
            if not ready:
                continue

            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                continue

            if not chunk:
                continue

            buffer += chunk
            while b"\n" in buffer:
                raw_line, buffer = buffer.split(b"\n", 1)
                line = raw_line.decode("utf-8", errors="replace").rstrip("\r")
                lines.append(line)
                if stop_text and stop_text in line:
                    return DeviceResponse(lines)

        if buffer:
            lines.append(buffer.decode("utf-8", errors="replace").rstrip("\r"))
        return DeviceResponse(lines)


def ensure_port_exists(port: str) -> None:
    if not os.path.exists(port):
        raise SystemExit(f"串口不存在: {port}")


def print_header(port: str) -> None:
    print(f"port={port} baud={DEFAULT_BAUD}")


def cmd_ports(_: argparse.Namespace) -> int:
    ports = list_serial_ports()
    if not ports:
      print("未发现串口设备。")
      return 1

    for port in ports:
        print(port)
    return 0


def cmd_send(args: argparse.Namespace) -> int:
    ensure_port_exists(args.port)
    print_header(args.port)
    with SerialConnection(args.port) as device:
        if args.quiet_stream:
            device.write_line("stream off")
            device.read_lines(0.4).print()
        for command in args.commands:
            print(f">>> {command}")
            device.write_line(command)
            response = device.read_lines(args.read_window)
            response.print()
        if args.quiet_stream:
            device.write_line("stream on")
            device.read_lines(0.4).print()
    return 0


def cmd_monitor(args: argparse.Namespace) -> int:
    ensure_port_exists(args.port)
    print_header(args.port)
    with SerialConnection(args.port) as device:
        response = device.read_lines(args.duration)
        response.print()
    return 0


def cmd_smoke_test(args: argparse.Namespace) -> int:
    ensure_port_exists(args.port)
    print_header(args.port)
    commands = [
        "status",
        "warm",
        "status",
        "hollow",
        "status",
        "bright",
        "status",
        "sample",
        "status",
        "sample2",
        "status",
        "twinkle",
        "status",
        "twinkle2",
        "status",
        "sine",
        "status",
    ]
    with SerialConnection(args.port) as device:
        device.write_line("stream off")
        device.read_lines(0.4).print()
        for command in commands:
            print(f">>> {command}")
            device.write_line(command)
            response = device.read_lines(args.read_window)
            response.print()
        device.write_line("stream on")
        device.read_lines(0.4).print()
    return 0


def wait_for_capture(device: SerialConnection, expected_text: str, timeout: float) -> bool:
    response = device.read_lines(timeout, stop_text=expected_text)
    response.print()
    return any(expected_text in line for line in response.lines)


def cmd_defaults(args: argparse.Namespace) -> int:
    ensure_port_exists(args.port)
    print_header(args.port)
    commands = ["defaults", "set volume far 500"]
    if args.save:
        commands.append("save")
    commands.append("status")

    with SerialConnection(args.port) as device:
        device.write_line("stream off")
        device.read_lines(0.4).print()
        for command in commands:
            print(f">>> {command}")
            device.write_line(command)
            response = device.read_lines(args.read_window)
            response.print()
        device.write_line("stream on")
        device.read_lines(0.4).print()
    return 0


def prompt_user(message: str) -> None:
    input(f"{message}\n按回车继续...")


def cmd_calibrate(args: argparse.Namespace) -> int:
    ensure_port_exists(args.port)
    print_header(args.port)

    steps = [
        ("pitch near", "把手放到 pitch 最近位置"),
        ("pitch far", "把手放到 pitch 最远位置"),
        ("volume near", "把手放到 volume 最近位置"),
        ("volume far", "把手移开到 volume 最远位置"),
    ]

    with SerialConnection(args.port) as device:
        device.write_line("stream off")
        device.read_lines(0.4).print()
        print("先读一次当前状态：")
        device.write_line("status")
        device.read_lines(args.read_window).print()

        for target, prompt in steps:
            prompt_user(prompt)
            command = f"cal {target}"
            print(f">>> {command}")
            device.write_line(command)
            captured = wait_for_capture(device, f"Captured {target} =", args.capture_timeout)
            if not captured:
                print(f"校准失败: {target} 未在超时时间内完成。", file=sys.stderr)
                return 1

        if args.save:
            print(">>> save")
            device.write_line("save")
            device.read_lines(args.read_window).print()

        print(">>> status")
        device.write_line("status")
        device.read_lines(args.read_window).print()
        device.write_line("stream on")
        device.read_lines(0.4).print()

    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="激光特雷门琴串口辅助工具")
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"串口设备，默认 {DEFAULT_PORT}")

    subparsers = parser.add_subparsers(dest="command", required=True)

    ports_parser = subparsers.add_parser("ports", help="列出当前串口")
    ports_parser.set_defaults(func=cmd_ports)

    send_parser = subparsers.add_parser("send", help="发送一条或多条串口命令")
    send_parser.add_argument("commands", nargs="+", help="要发送的命令")
    send_parser.add_argument("--read-window", type=float, default=DEFAULT_READ_WINDOW, help="每条命令后的读取时长（秒）")
    send_parser.add_argument("--quiet-stream", action="store_true", help="发送前临时关闭自动状态流")
    send_parser.set_defaults(func=cmd_send)

    monitor_parser = subparsers.add_parser("monitor", help="读取一段串口输出")
    monitor_parser.add_argument("--duration", type=float, default=5.0, help="监视时长（秒）")
    monitor_parser.set_defaults(func=cmd_monitor)

    smoke_parser = subparsers.add_parser("smoke-test", help="执行音色切换和状态烟雾测试")
    smoke_parser.add_argument("--read-window", type=float, default=DEFAULT_READ_WINDOW, help="每条命令后的读取时长（秒）")
    smoke_parser.set_defaults(func=cmd_smoke_test)

    defaults_parser = subparsers.add_parser("apply-defaults", help="下发推荐默认校准")
    defaults_parser.add_argument("--save", action="store_true", help="下发后立即保存")
    defaults_parser.add_argument("--read-window", type=float, default=DEFAULT_READ_WINDOW, help="每条命令后的读取时长（秒）")
    defaults_parser.set_defaults(func=cmd_defaults)

    calibrate_parser = subparsers.add_parser("calibrate", help="引导完成 4 步校准")
    calibrate_parser.add_argument("--save", action="store_true", help="校准完成后立即保存")
    calibrate_parser.add_argument(
        "--capture-timeout",
        type=float,
        default=DEFAULT_CAPTURE_TIMEOUT,
        help="等待单次采样完成的最长时长（秒）",
    )
    calibrate_parser.add_argument("--read-window", type=float, default=DEFAULT_READ_WINDOW, help="普通命令后的读取时长（秒）")
    calibrate_parser.set_defaults(func=cmd_calibrate)

    return parser


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
