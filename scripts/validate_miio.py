#!/usr/bin/env python3
"""在电脑上验证米家 miIO 流程，通过后再烧录固件。

用法:
  python3 scripts/validate_miio.py
  python3 scripts/validate_miio.py --ip 192.168.31.85 --token <hex32>
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from miio import Device
except ImportError:
    print("请先安装: pip install python-miio")
    sys.exit(1)


def load_default_config() -> tuple[str, str]:
    cfg_path = Path(__file__).resolve().parent.parent / "data" / "config.json"
    data = json.loads(cfg_path.read_text(encoding="utf-8"))
    dev = data["devices"][0]
    return dev["ip"], dev["token"]


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate miIO commands against a mijia device")
    parser.add_argument("--ip")
    parser.add_argument("--token")
    args = parser.parse_args()

    ip, token = args.ip, args.token
    if not ip or not token:
        ip, token = load_default_config()

    print(f"device: {ip}")
    print(f"token:  {token}")

    dev = Device(ip, token)

    print("\n[1] miIO.info")
    info = dev.send("miIO.info")
    print("  model:", info.get("model"))
    print("  fw:   ", info.get("fw_ver"))

    print("\n[2] get_prop power")
    power = dev.send("get_prop", ["power"])
    print("  power:", power)

    print("\n[3] set_power off")
    r = dev.send("set_power", ["off"])
    print("  result:", r)

    print("\n[4] get_prop power")
    power = dev.send("get_prop", ["power"])
    print("  power:", power)

    print("\n[5] set_power on")
    r = dev.send("set_power", ["on"])
    print("  result:", r)

    print("\n[6] get_prop power")
    power = dev.send("get_prop", ["power"])
    print("  power:", power)

    print("\n[7] set_prop power true (应失败)")
    try:
        dev.send("set_prop", ["power", True])
        print("  unexpected success")
    except Exception as exc:
        print("  expected error:", exc)

    print("\nOK: 请使用 get_prop + set_power，不要用 set_prop 控制 power")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
