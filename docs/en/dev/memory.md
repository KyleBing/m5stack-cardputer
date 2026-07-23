# Memory notes

Cardputer (ESP32-S3) has limited runtime memory. HTTPS, BLE, large JSON, full-image buffers, and similar all consume **internal Heap**. With fragmentation, “enough free total, not enough contiguous” fails the same way. This page covers memory regions in this firmware, allocation habits, common low-memory cases, and how to read values on device.

> Numbers vary with firmware version, open App, and WiFi/BLE state. Thresholds below follow source constants — empirical gates, not hardware hard limits.

## Memory regions (what you see on device)

| Type | What it is | Main uses |
|------|--------|----------|
| **Heap** | On-chip SRAM dynamic heap | `malloc` / `new` / Arduino `String` / TLS / WiFi / BLE / FreeRTOS off-stack buffers |
| **PSRAM** | External pseudo-SRAM (if present) | Large buffers can go here; most firmware paths still use internal Heap |
| **Sketch** | Firmware partition size in Flash | Built `.bin` size; unrelated to runtime Heap |
| **LittleFS** | Flash filesystem partition | `config.json`, icons, logs, etc.; full disk ≠ Heap OOM |

Info's Memory page shows these as bars, plus **Max Alloc** / **Min Free** (below).

### Heap vs fragmentation

- **Free Heap** (`ESP.getFreeHeap()`): total free bytes now.
- **Max Alloc** (`ESP.getMaxAllocHeap()`): largest **contiguous** block a single `malloc` can succeed with now.

TLS / mbedTLS, larger JSON, and full-image RGB565 often need one contiguous block. So:

```
Free large + Max Alloc tiny  → classic fragmentation; HTTPS / large allocs still fail
```

**Min Free** (`ESP.getMinFreeHeap()`) is the lowest free heap since boot — useful to see if you ever approached the danger zone.

## Allocation habits in firmware

This repo has no unified “memory pool framework”; strategies are chosen per case:

### 1. Stack / static / global

Prefer stack or `static`/`BSS` for short-lived, fixed-size small objects — avoid repeated `malloc`.

### 2. `malloc` on demand, `free` on leave

Allocate large non-resident resources when entering an App; free on exit so Heap is not held:

| Scenario | Approach |
|------|------|
| IR AC icon cache | `malloc` on enter IR, `free` on leave (`app_ir.cpp`) |
| Device icon scale | Temporary full-image buffer; 1:1 push by row without full-image RAM |
| Screenshot row buffer | Row-level `malloc`; on failure return `oom heap=…` |
| Web file list | `FmEntry` array `malloc` up to a cap; on failure show “out of memory” |

### 3. Streaming / chunking — avoid one giant buffer

| Scenario | Approach |
|------|------|
| Cursor charts | `pageSize=200` + streaming parse, lower large-JSON OOM risk |
| Config Web HTML | Chunked send, no huge `String` assembly |

### 4. Check Heap / Max Alloc before expensive work

Some features probe before “expensive” ops; if low, skip and log instead of looking like a network error:

| Feature | Approx threshold (source constant) | When low |
|------|----------------------|------------|
| Cursor HTTPS | Free ≥ **48 KB** and Max Alloc ≥ **24 KB** | Skip request; log `http skip lowmem` |
| Cursor fetch task | Free ≥ **90 KB** (~32 KB stack) | UI `auth lowmem` |
| Config open AP | Free ≥ **40 KB** and Max Alloc ≥ **20 KB** | Skip SoftAP; serial `[web] ap skip lowmem` |
| Mijia BLE scan | Free ≥ **28 KB** | Do not start scan |

### 5. Flash vs RAM roles

Icons prefer **`.rgb565` push from LittleFS**, reducing decode temp heap; a few hot icons may RAM-cache on App enter. See [Image processing and baking](./images).

## When memory runs short

### User-visible symptoms

| Symptom | Common cause |
|------|----------|
| Cursor shows **`auth lowmem`** | Heap / Max Alloc below HTTPS or task threshold; not necessarily a bad token |
| Cursor `auth -1` / connect-class failure | Sometimes TLS alloc failure misreported; firmware prefers lowmem check when memory is tight |
| Config Web won't open / can't start AP | Free or contiguous block too small for SoftAP + HTTP |
| Mijia scan empty, occasional hitch | BLE + WiFi stacked; scan aborted if Heap too low beforehand |
| Screenshot fails `oom` | Row-buffer `malloc` failed |
| IR icons occasionally slow | Cache `malloc` failed → fall back to PNG / Flash reads |
| Web file manager “out of memory” | Directory entry array alloc failed |

### Combinations that tighten Heap

1. **WiFi connected + HTTPS (Cursor / some config pages)**: TLS sessions are large and need contiguous blocks.
2. **BLE (HID / Mijia scan) and WiFi both active**: two stacks resident; free drops sharply.
3. **Large JSON / long String concatenation**: Arduino `String` growth fragments easily.
4. **Full-image temp buffers**: scale draws, large unbaked PNG decode.
5. **Light sleep without real release**: light sleep saves power but does **not** clear the heap like deep sleep reboot; Heap state largely continues after wake. Deep sleep wake reboots and resets the heap.

### Fragmentation vs “truly exhausted”

- **Truly exhausted**: Free Heap stays very low (e.g. long-term &lt; 30–40 KB).
- **Fragmented**: Free still looks like tens of KB, but Max Alloc is only ~10 KB or less → large allocs still fail.

Mitigation (usage): leave memory-heavy Apps → turn off unneeded BLE/WiFi scenarios → **deep sleep or reboot** to clear fragments if needed. Dev side: fewer large `String`s, allocate on demand, stream parse, `free` large blocks when done.

## How to inspect system memory

### On device: Info → Memory

1. Main menu **`i`** → [Info](/en/apps/info).
2. First page is **Memory** (page with `[` `]`).

Page fields:

| Row | Meaning | How to read |
|----|------|--------|
| **Heap** `used/total xx%` | On-chip heap used / total | Higher % = tighter; use with the next two rows |
| **PSRAM** | External RAM (hidden if none) | Only with hardware; most paths still lean on Heap |
| **Sketch** | Firmware used / partition space | Growing means fatter firmware; not a direct runtime OOM signal |
| **LittleFS** | FS used / capacity | Full → config/log write fails; not Heap OOM |
| **Max Alloc** | Largest contiguous allocable block now | **Check this first for HTTPS / large images** |
| **Min Free** | Historical lowest free heap | Very low dips mean a path peaked dangerously |

Bar colors get “tighter” as usage rises (see `memUsedBarColor` in `app_info.cpp`).

### Serial / logs

- Boot logs print `heap=…` (`main` boot).
- Cursor: LittleFS `/cursor.log`, error rail `/cursor.err`; viewable via Config Web; often include `heap=`, `max=`.
- Web / AP: `[web] ap skip lowmem heap=… max=…`.

During development, PlatformIO serial monitor (`115200`) is enough to correlate.

### Sampling in code (debug)

```cpp
ESP.getHeapSize();      // Heap total
ESP.getFreeHeap();      // Current free
ESP.getMaxAllocHeap();  // Largest contiguous block
ESP.getMinFreeHeap();   // Historical lowest free
ESP.getPsramSize();     // 0 = no PSRAM
ESP.getFreePsram();
```

Info page used amount: `used = total - free` (same APIs).

## Interpreting numbers (experience)

For **Heap / Max Alloc**, typical idle with WiFi up and no extra BLE:

| Metric | Comfortable | Watch | High risk |
|------|--------|--------|--------|
| Free Heap | ≳ 100 KB | 40–90 KB | ≲ 40 KB |
| Max Alloc | ≳ 40 KB | 20–40 KB | ≲ 24 KB (near HTTPS gate) |

Against this firmware's gates:

- **Cursor fetch**: aim Free ≳ **90 KB** (create task) and Max Alloc ≳ **24 KB**.
- **Config AP only**: Free ≳ **40 KB**, Max Alloc ≳ **20 KB**.
- **Mijia BLE scan**: Free ≳ **28 KB**.

High **Sketch / LittleFS** % means “can we flash a bigger firmware / write more files” — do not confuse with Heap OOM.

## Short triage flow

1. Before reproducing, open **Info → Memory** and note Heap%, **Max Alloc**, **Min Free**.
2. If Max Alloc ≪ Free, suspect **fragmentation**; leave Apps or reboot, then retry.
3. On Cursor / Web failures, check `/cursor.log` or serial for `lowmem` before blaming only token / routing.
4. When changing firmware: allocate large blocks on demand and free on leave; never build huge `String`s on hot paths; before ops needing contiguous large blocks, reuse the existing `getFreeHeap` / `getMaxAllocHeap` check pattern.

## Related entry points

- UI: [Info](/en/apps/info) (`i`)
- Implementation: `src/app_info.cpp` (sampling and Memory page)
- Low-memory guards: `src/app_cursor.cpp`, `src/app_web.cpp`, `src/mijia_ble.cpp`
- Assets and RAM strategy: [Image processing and baking](./images)
