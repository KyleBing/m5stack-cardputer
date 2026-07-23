# WiFi

Main menu key: `w`

View the current connection, switch among saved profiles, scan nearby APs and connect with a password. Config supports up to **5** `wifis[]` entries; `wifi_active` selects the current profile.

## Screenshots

**Connected**

<div class="shot-row">

![wifi-connected](/shots/app_wifi_001.png)

</div>

**Saved profiles**

<div class="shot-row">

![wifi-saved](/shots/app_wifi_002.png)

</div>

**Help**

<div class="shot-row">

![wifi-help](/shots/app_wifi_003.png)

</div>

## Shortcuts

| State | Key | Action |
|-------|-----|--------|
| Connected | `r` | Refresh status |
| Connected | `c` | Change / pick profile |
| Connected | `s` | Scan |
| No profile / idle | `s` | Scan |
| Pick saved | `1`–`4` (and list items) · arrows | Select profile |
| Pick saved | `s` | Go to scan |
| Enter password | Alphanumeric · Enter | Connect |
| Enter password | Del / Backspace | Delete char |
| Failed | `r` | Retry |
| Failed | `c` | Change |
| Any | `h` | Help |

## Usage

1. **Connected**: shows SSID, IP, RSSI, etc.; `r` refreshes, `c` switches profile, `s` scans.
2. **Pick saved**: choose a saved entry as active and connect.
3. **Scan → Password**: pick an AP, enter password, Enter to connect; success may write config (same source as Config Web `/wifi`).
4. Other Apps use only the current **active** profile; they do not round-robin multiple SSIDs.

Legacy top-level `"wifi":{ssid,password}` is migrated to `wifis[]` on boot.
