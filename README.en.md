# <img src="/design/logo_no_padding.png" width="50px"> Sparks

[中文](./README.md) | **English**

Personal firmware for Cardputer-ADV, focused on Mi Home device control and Cursor usage info.

_The firmware UI is entirely in English, with many abbreviations — a solid English foundation helps a lot._


<img alt="2026-07-17  cardputer adv-24-2000x2000" src="https://github.com/user-attachments/assets/2e922a2a-303a-48e4-aa7e-3d736752aa22" />

<img alt="screenshots" src="https://github.com/user-attachments/assets/ab6fb8e1-0ffd-47ab-9a44-d97fbf7b8de9" />

## 1. Features

Built on M5Stack libraries. Available apps:

| App | Name | Shortcut | Description |
|-----|------|----------|-------------|
| Mijia | Mijia | `m` | Device status & control |
| Time | Time | `t` | Boot time, clock, stopwatch, countdown |
| Cursor | Cursor Dashboard | `c` | Cursor info, token balance, usage (24h / 7d / 30d) |
| Infrared | Infrared | `x` | TV & AC IR remote for major brands |
| Mic | Mic | `r` | Mic test & waveform view |
| IMU | IMU | `g` | BMI270 6-axis sensor readout |
| WiFi | WiFi | `w` | WiFi setup with multiple saved profiles |
| AP/LAN | Config | `u` | Web config server for editing `config.json` |
| Options | Options | `o` | Display, volume, preferences |
| Icons | Icons | `a` | Browse firmware icons |
| Font | Font | `f` | Preview built-in system fonts |
| Sleep | Sleep | `s` | Light / deep sleep without powering off |
| Morse | Morse | `j` | Keypress Morse code audio |
| Keyboard | HID Keyboard | `k` | Bluetooth & USB keyboard |
| LED | RGB LED | `l` | Onboard ESP32 LED test |
| BLE | BLE | `b` | Nearby Bluetooth device list |
| Display | Display | `d` | Screen test: backgrounds, lines, rectangles |


## 2. Documentation

Full firmware docs:

- English: [Docs](https://kylebing.github.io/m5stack-cardputer-sparks/en/)
- 中文：[在线文档](https://kylebing.github.io/m5stack-cardputer-sparks/)

## 3. Flashing

See the [Releases](https://github.com/KyleBing/m5stack-cardputer-sparks/releases) page.


## 4. Why Cardputer

I've always loved pixel displays — especially low-power monochrome ones, like old Nokias that rely on reflected light.  
For a while I wanted to build a small gadget with that kind of screen and some fun features. After doing the math, a watch would have been cheaper, so I dropped it.

The idea never really left. Chatting with Gemini pointed me toward M5Stack — first the [Stick](https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit).  
The downside of such tiny devices is few buttons: navigating menus with a d-pad (or less) on cheap keys is tedious and feels awkward.  
Then I found the [Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3) — lots of keys compared to the Stick. Perfect.  
I've long liked BlackBerry-style full keyboards and letter-launch shortcuts (e.g. O for settings). With this many keys, Cardputer works great as an app launcher: one letter, one app, quick multi-app switching.  
Once I had the device, it felt like a foundation for the ideas I'd been sitting on — so I put my favorite tools and experiments on it, and keep adding new ones.

ESP32 tools like Cardputer run a fixed firmware image, so boot is fast — faster than Android or Linux, which I love. This firmware is tuned for that; cold boot is about **1 second**.

I especially like using it to control Mi Home devices at home — a better stand-in than Xiaoai for the things I care about.
