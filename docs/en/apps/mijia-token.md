# Get Mijia Device Token

Both LAN miIO and BLE devices need a `token` from Xiaomi Cloud (BLE also needs `ble.key`). Recommended open-source tool:

[Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)

Sign in with your Xiaomi Cloud account (email / Xiaomi ID + password, or QR scan), pick the server region (`cn` for mainland China, or leave empty to scan all regions), then list all devices under the account.

## Tool output example

```text
NAME:     Xiaomi Wireless Switch (Bluetooth)
ID:       blt.3.1fe2te3rokg00
BLE KEY:  8ac1c06d5989a8618c1158488a1b46b7
MAC:      18:C2:3C:28:45:44
IP:       112.232.158.153
TOKEN:    9d2d1a7c337fb00ddcb1f60c
MODEL:    lumi.remote.mcn001
```

## Field mapping

| Extractor | `config.json` `devices[]` | Notes |
|-----------|---------------------------|------|
| `NAME` | `name` / `name_zh` | Rename to your preferred English / Chinese label |
| `ID` | `id` | Copy as-is; BLE is often `blt.3.…` |
| `TOKEN` | `token` | Required |
| `MODEL` | `model` | Required; firmware classifies control by this |
| `MAC` | `mac` | Recommended; required for BLE |
| `BLE KEY` | `ble.key` | BLE devices only |
| `IP` | `ip` | Wi-Fi devices: **LAN** IP; pure BLE can leave empty |

Optional: `hotkey` (`a`–`z` / `0`–`9`, `q` reserved) for quick jump in [Mijia](./mijia).

## Convert to firmware format

You can have an AI do this step — it is straightforward.

### BLE device (has `BLE KEY`)

From the example above:

```json
{
  "name": "Switch",
  "name_zh": "蓝牙无线开关",
  "ip": "",
  "token": "9d2d1a7c337fb00ddcb1f60c",
  "model": "lumi.remote.mcn001",
  "id": "blt.3.1fe2te3rokg00",
  "mac": "18:C2:3C:28:45:44",
  "ble": {
    "key": "8ac1c06d5989a8618c1158488a1b46b7"
  }
}
```

Notes:

- `BLE KEY` → `ble.key` (nested object, not a top-level string)
- Extractor `IP` is often a public / cloud address, useless for local BLE scan — leave `ip` empty
- Keep `MAC` as `AA:BB:CC:DD:EE:FF`

### Wi-Fi device (no `BLE KEY`)

```json
{
  "name": "DeskLamp",
  "name_zh": "台灯",
  "ip": "192.168.31.85",
  "token": "28398e0747a0adb0dca2b5c028c039c3",
  "model": "yeelink.light.lamp2",
  "id": "434412341",
  "mac": "B4:60:ED:03:2E:8A",
  "hotkey": "l"
}
```

Notes:

- Do not copy the Extractor's public `IP`; use the LAN address from your router (same subnet as Cardputer)
- Without `BLE KEY`, omit the `ble` field

## Write to config

Put the object in the `devices` array of `config.json`, or add / edit via the [Config](./config) Web page. After save, reboot or re-enter [Mijia](./mijia).

## FAQ

- **Login failed**: Use a Mijia / Xiaomi Cloud account, not Roborock or other third-party apps; try QR login; watch region rate limits (~3–5 2FA attempts)
- **Lists but control fails**: Check `token` / `model` typos; for Wi-Fi verify LAN `ip`; for BLE verify `mac` and `ble.key`
- **How to run**: On Windows, use `token_extractor.exe` from the release; other platforms see the repo [README](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)
