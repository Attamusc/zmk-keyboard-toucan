> [!WARNING]
> **Historical Toucan v1 research only. Do not use this document to configure Toucan2.**
> This analysis applies to the legacy Cirque/Pinnacle trackpad. Toucan2 uses an Azoteq TPS43, so the drivers, power behavior, settings, and recommendations below do not apply. See the repository README and `boards/shields/toucan/toucan_right.overlay` for current Toucan2 guidance.

# ZMK Cirque Pinnacle Battery Life – Right-Half Drain Analysis

> **Researched:** 2026-06-05  
> **Scope:** geeksville/cirque-input-module `toucan` branch, Seeed XIAO BLE (nRF52840), ZMK v0.3 / Zephyr v3.5  
> **Your config file locations:** `boards/shields/toucan/toucan_right.conf` · `boards/shields/toucan/toucan_right.overlay`

---

## Executive Summary

**Top 3 most-likely culprits in your specific build:**

| # | Culprit | Evidence |
|---|---------|----------|
| 1 | **`K_FOREVER` blocking the system workqueue** — under BLE congestion, `input_report_*(..., K_FOREVER)` blocks forever, preventing PM sleep and idle transitions | toucan branch still uses `K_FOREVER` (open PRs [#3](https://github.com/geeksville/cirque-input-module/pull/3)/[#4](https://github.com/geeksville/cirque-input-module/pull/4) unmerged); confirmed cause of "keyboard goes dark" on Toucan hardware |
| 2 | **`sleep;` DT property is silenced by `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y`** — idle sleeper calls `pinnacle_set_sleep(false)` whenever ZMK is ACTIVE, stranding the Pinnacle at 1.7 mA idle instead of 40 µA standby between touches | Both enabled simultaneously in your config; original toucan intentionally disabled the idle sleeper to avoid this |
| 3 | **DR GPIO interrupt unmasked on early-return paths** — SPI errors / `0xFF` STATUS1 / SW_DR not asserted all return without calling `set_int(dev, true)`, permanently masking the level-triggered interrupt | PRs [#5](https://github.com/geeksville/cirque-input-module/pull/5) (open) and [#3](https://github.com/geeksville/cirque-input-module/pull/3) (closed as split) identify exactly this; toucan branch is unpatched |

**Top 3 cheapest fixes:**

| # | Fix | Effort |
|---|-----|--------|
| 1 | Cherry-pick / apply the [K_NO_WAIT patch (PR #4)](https://github.com/geeksville/cirque-input-module/pull/4) + [DR re-arm patch (PR #5)](https://github.com/geeksville/cirque-input-module/pull/5) into your cirque-input-module fork | ~10 lines, no hardware change |
| 2 | In `toucan_right.conf`: comment out `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y` (revert to original toucan intent), and rely solely on the DTS `sleep;` property for between-touch standby savings | One line, rebuild |
| 3 | In `toucan_right.overlay`: set `SLEEP_TIMER` explicitly so the Pinnacle enters standby quickly (within ~400 ms of last touch); keep `sleep;` enabled | Add one property to glidepoint node |

---

## 1. Root Causes

### 1.1 Pinnacle ASIC Power States

The Pinnacle TM040040 1CA027 ASIC has four distinct power states.  
Numbers from the Pinnacle TM035035 spec (same ASIC) and confirmed by multiple ZMK contributors:

| Mode | Current @ 3.3 V | How to enter | How to exit |
|------|-----------------|--------------|-------------|
| **Active tracking** (finger on pad, data streaming) | **2.9 mA** | Default when feed enabled | — |
| **Idle** (feed enabled, no touch detected) | **1.7 mA** | Auto after touch ends | Next touch |
| **Sleep / Standby** (`EN_SLEEP` bit set in SysConfig1 reg 0x03 bit[2]) | **~40 µA** | Auto after SLEEP_TIMER × SLEEP_INTERVAL ms of no touch | Touch detected (SLEEP_INTERVAL polling period = wake latency) |
| **Shutdown** (`SHUTDOWN` bit set in SysConfig1 reg 0x03 bit[1]) | **~0.23 µA** | Driver writes bit via SPI | SPI transaction (dummy read) + clear bit |

Sources: [beekeeb toucan PR #5](https://github.com/beekeeb/zmk-keyboard-toucan/pull/5), [geeksville cirque-input-module PR #1 optimization plan](https://github.com/geeksville/cirque-input-module/pull/1), [petejohanson cirque-input-module PR #7](https://github.com/petejohanson/cirque-input-module/pull/7).

**Which mode the driver actually uses:**

- **Boot**: `sleep;` DT property → `pinnacle_set_sleep(true)` during `pinnacle_init` → Standby mode active from the start.  
- **ZMK ACTIVE** (any key/touch): if `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y`, idle_sleeper fires `pinnacle_set_sleep(false)` → **drops back to Idle (1.7 mA)**. See §1.2.  
- **ZMK IDLE** (30 s no activity): idle_sleeper fires `pinnacle_set_sleep(true)` → Standby (~40 µA).  
- **ZMK SLEEP** (60 min timeout): `zmk_pm_suspend_devices()` → `pinnacle_pm_action(PM_DEVICE_ACTION_SUSPEND)` → `pinnacle_set_shutdown(true)` → **Shutdown (~0.23 µA)**.  
- **ZMK deep sleep (SYSTEM_OFF)**: DR interrupt is disabled before `sys_poweroff()`; nRF52840 enters SYSTEM_OFF (~1–2 µA SoC); total system ~2–5 µA.

**Relevant code path:**
```c
// input_pinnacle.c – PM_DEVICE_ACTION_SUSPEND
case PM_DEVICE_ACTION_SUSPEND:
    pinnacle_set_shutdown(dev, true);  // ← correctly reached during ZMK_SLEEP
    return 0;
```
Source: [`geeksville/cirque-input-module` toucan `drivers/input/input_pinnacle.c` lines 697–706](https://github.com/geeksville/cirque-input-module/blob/toucan/drivers/input/input_pinnacle.c)

---

### 1.2 Idle Sleeper vs. DTS `sleep;` Property Conflict ⚠️ THIS LIKELY APPLIES TO YOU

**Your `toucan_right.conf`:**
```
CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y   ← enabled (you changed from original)
```

**Your `toucan_right.overlay`:**
```c
glidepoint: glidepoint@0 {
    sleep;   ← DT property: enables Pinnacle EN_SLEEP bit on boot
};
```

**The problem:**  
`zmk_pinnacle_idle_sleeper.c` subscribes to `zmk_activity_state_changed` and calls:
```c
bool sleep = state_ev->state == ZMK_ACTIVITY_ACTIVE ? 0 : 1;
pinnacle_set_sleep(pinnacle_devs[i], sleep);
```
Source: [`geeksville/cirque-input-module` toucan `drivers/input/zmk_pinnacle_idle_sleeper.c`](https://github.com/geeksville/cirque-input-module/blob/toucan/drivers/input/zmk_pinnacle_idle_sleeper.c)

When state → `ZMK_ACTIVITY_ACTIVE` (any touch or keypress): `pinnacle_set_sleep(dev, false)` **disables** the EN_SLEEP bit. The Pinnacle exits standby and returns to 1.7 mA idle mode. It stays there until ZMK transitions to IDLE again (30 s later).

**Timeline during typical use:**
```
T=0s:   user moves mouse → ZMK=ACTIVE → idle_sleeper: pinnacle_set_sleep(false)
T=0s:   Pinnacle: 1.7 mA (idle, no touch) during pauses between gestures
T=5s:   user stops touching
T=35s:  ZMK → IDLE → idle_sleeper: pinnacle_set_sleep(true)
T=35s:  Pinnacle: 40 µA (standby)
...
T=65s:  user touches again → ZMK → ACTIVE → disable sleep → back to 1.7 mA
```

Between T=5s and T=35s: Pinnacle draws **1.7 mA** instead of **40 µA** (42× more).

**Original intent (beekeeb / geeksville original toucan):**  
The idle sleeper was **deliberately disabled** in `toucan_right.conf`:
```
# CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y   ← commented out
```
See: [beekeeb toucan PR #5](https://github.com/beekeeb/zmk-keyboard-toucan/pull/5) — "This saves lots of power during use but IMO the user experience feels not so great because quite often when you go to use the touchpad you need to wait ~300ms for it to wake up."

The `sleep;` DT property alone was intended to handle between-touch standby. The 300 ms delay applies after every touch in this mode, but with ZMK_IDLE_TIMEOUT=30 s it only affects the initial touch after 30 s of inactivity.

---

### 1.3 SPI Bus Power State in Idle

**Default state:** The nRF SPIM driver keeps SPI lines in their `spi0_default` pinctrl state when no transaction is in progress (SCK idle-low, MOSI idle-low, MISO as input). CS is driven high via `(GPIO_ACTIVE_LOW | GPIO_PULL_UP)` on gpio0 3 — this keeps the Pinnacle's SPI interface deasserted. ✓

**Sleep pinctrl (`spi0_sleep`):** Applied during `PM_DEVICE_ACTION_SUSPEND` on the SPI bus, which happens when ZMK enters deep sleep (60 min). `CONFIG_PINCTRL_KEEP_SLEEP_STATE` defaults to `y` when `PM_DEVICE` is on, so the `bias-pull-down; low-power-enable;` state IS applied.  
Source: [Zephyr `drivers/pinctrl/Kconfig`](https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/drivers/pinctrl/Kconfig): `default y if PM || PM_DEVICE || DEVICE_DEINIT_SUPPORT`

**During regular idle (30 s–60 min): NOT applied.** The SPI bus is only suspended during deep sleep. During regular idle the pins remain in `spi0_default`. With CS pulled high, MISO is tristated by the Pinnacle, so there is no leakage path through the SPI lines. ✓

---

### 1.4 DR GPIO Interrupt – Level vs. Edge, Wakeup Source

**Your driver uses level-triggered interrupt (`GPIO_INT_LEVEL_ACTIVE`):**
```c
// geeksville toucan – input_pinnacle.c line ~126
gpio_pin_interrupt_configure_dt(&config->dr, en ? GPIO_INT_LEVEL_ACTIVE : GPIO_INT_DISABLE);
```

**Upstream petejohanson uses edge-triggered (`GPIO_INT_EDGE_TO_ACTIVE`):**
```c
// petejohanson/main – same line
gpio_pin_interrupt_configure_dt(&config->dr, en ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_DISABLE);
```

The level-triggered approach was chosen to avoid missed ZIDLEs (geeksville comment: "the current irq system is edge triggered probably isn't great for this reason"). However it introduces a critical latent bug: ⚠️

**DR IRQ unmasked on early-return paths (PR #5, UNMERGED):**

In `pinnacle_report_data_rel` (the active code path in your relative-mode config):
```c
static void pinnacle_report_data_rel(const struct device *dev) {
    ...
    ret = pinnacle_seq_read(dev, PINNACLE_STATUS1, packet, 1);
    if (ret < 0) {
        return;    // ← MISSING set_int(dev, true)!
    }

    if (packet[0] == 0xFF || !(packet[0] & PINNACLE_STATUS1_SW_DR)) {
        return;    // ← MISSING set_int(dev, true)!
    }
    ret = pinnacle_seq_read(dev, PINNACLE_2_2_PACKET0, packet, 3);
    if (ret < 0) {
        return;    // ← MISSING set_int(dev, true)!
    }
    ...
    // set_int(dev, true) only called inside pinnacle_send_rel
}
```

Any SPI error, transient `0xFF` status, or packet read with SW_DR not set → interrupt stays masked indefinitely. Trackpad goes silent. See: [PR #5 description](https://github.com/geeksville/cirque-input-module/pull/5).

**DR as wakeup source:**  
The `dr-gpios` spec does NOT include `GPIO_WAKEUP` and the node has no `wakeup-source` DT property. The kscan0 has `wakeup-source`. This is correct: the trackpad cannot accidentally wake the device from SYSTEM_OFF deep sleep, only key presses can. ✓

**Does it let the SoC sleep?** When `set_int(dev, false)` is called (ISR disabling interrupt before work submission), the nRF GPIO GPIOTE channel is reconfigured to `GPIO_INT_DISABLE`, clearing the SENSE field. The SoC can sleep during work processing. ✓ When work is done and `set_int(dev, true)` is called, if DR is already high (more data), the level-triggered interrupt fires immediately — this is by design to drain all pending data.

---

### 1.5 BLE Peripheral Role and Touch Traffic

**Connection parameters** (from `zmk/app/src/split/bluetooth/Kconfig`):
```
CONFIG_ZMK_SPLIT_BLE_PREF_INT=6      # 6 × 1.25 ms = 7.5 ms connection interval
CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY=30 # peripheral can skip 30 events
```

**With no data to send:** peripheral radio wakes every 7.5 ms × (1+30) = 232.5 ms. Very power-efficient.  
**With touch data:** peripheral must send in every connection event → 7.5 ms interval → ~133 radio wakeups/sec, each ~3–5 ms of TX/RX at ~15 mA peak. Estimated average: ~1.5–2 mA additional from BLE radio during active tracking.

Every input event from the trackpad (including ZIDLE packets from `NUM_ZIDLE_PAD=2`) is immediately forwarded by the peripheral split handler:
```c
// zmk/app/src/pointing/input_split.c – peripheral side
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_INST_PHANDLE(n, device)), split_input_handler_##n, NULL);
```
Source: [zmk `app/src/pointing/input_split.c`](https://github.com/zmkfirmware/zmk/blob/main/app/src/pointing/input_split.c)

Each input event also calls `note_activity()` which resets the 30-second idle timer:
```c
// zmk/app/src/activity.c
INPUT_CALLBACK_DEFINE(NULL, activity_input_listener, NULL);  // resets idle on every event
```
Source: [zmk `app/src/activity.c`](https://github.com/zmkfirmware/zmk/blob/main/app/src/activity.c)

**Net effect:** While the user is actively using the trackpad, the right half's BLE radio is awake at 7.5 ms intervals, driving ~1.5–2 mA. This is unavoidable when sending pointer data in real time. The left half (keys only) sleeps 232 ms between events in typical use.

---

### 1.6 Battery Level Fetching

`CONFIG_ZMK_BATTERY_REPORTING=y` on both halves. The battery timer fires every **60 seconds** (default `CONFIG_ZMK_BATTERY_REPORT_INTERVAL=60`). This reads the ADC and sends a BAS GATT notification. Cost: ~1–2 ms of radio time every 60 s. **Negligible contribution** to battery drain.

`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` is on the LEFT (central) only in your current config — it subscribes to the right side's BAS notification but does not cause extra polling. ✓

---

### 1.7 Power Regulator / Breakout Board Hardware

**Potential issue depending on which Cirque breakout you use:**

- **Cirque official dev kit board** (with TM040040 + on-board components): contains a 3.3 V LDO regulator with ~50–100 µA quiescent current, AND I2C pull-up resistors (typically 10 kΩ on SDA/SCL). In SPI mode, those pull-up resistors have current flowing if the I2C lines are driven. At 3.3 V with 10 kΩ: 330 µA per resistor × 2 = 660 µA baseline drain from unused I2C pull-ups.

- **"Asia" bare-flex breakouts** (most DIY builds): typically no on-board LDO; use a MOSFET or directly connected to the MCU 3V3 rail. Pull-ups vary.

**Recommendation:** If your breakout has I2C pull-ups, cut them (they're unused in SPI mode). If it has an LDO, check its quiescent current spec — you may save 50–200 µA by bypassing it and driving the Pinnacle directly from the nRF52840's 3V3 output.

---

### 1.8 The `K_FOREVER` System Workqueue Deadlock ⚠️ THIS LIKELY APPLIES TO YOU

The toucan branch reports all input events with `K_FOREVER`:
```c
// input_pinnacle.c – current toucan branch
input_report_key(dev, INPUT_BTN_TOUCH, ..., false, K_FOREVER);
input_report_rel(dev, INPUT_REL_X, dx, false, K_FOREVER);
input_report_rel(dev, INPUT_REL_Y, dy, true, K_FOREVER);
```

`pinnacle_work_cb` runs on the **system workqueue**. `input_report_*(..., K_FOREVER)` posts to Zephyr's input message queue (with `CONFIG_INPUT_MODE_THREAD`). Under ZMK, this queue feeds BLE split forwarding. When the BLE link is congested (busy period, retransmit, or rapid movement), the queue fills, and `K_FOREVER` **blocks the system workqueue thread indefinitely**.

**Power consequence:** When deadlocked:
- System workqueue frozen → `activity_work` can't fire → idle timer can't advance → ZMK stays ACTIVE forever
- Pinnacle stuck at 2.9 mA (active tracking, DR interrupt firing but work blocked)
- BLE events queued but not processed → radio stays active
- Deep sleep (60-minute timeout) **never fires** until power cycle

This was reproduced on hardware in [PR #3](https://github.com/geeksville/cirque-input-module/pull/3): "80+ min of heavy trackpad use with the heartbeat advancing every second and zero freezes (the unpatched build wedged within ~15 min)."

The fix (proposed PR #4, not merged): change all `K_FOREVER` → `K_NO_WAIT` in `input_report_*` calls.

---

## 2. Driver / Fork Landscape

### 2.1 Fork Comparison

| Fork | Branch | Interrupt | K_NO_WAIT | DR re-arm | PM Shutdown | Idle Sleeper | Notes |
|------|--------|-----------|-----------|-----------|-------------|--------------|-------|
| [petejohanson/cirque-input-module](https://github.com/petejohanson/cirque-input-module) | `main` | `EDGE_TO_ACTIVE` | No (K_FOREVER?) | No explicit re-arm | ✓ | ✓ | Upstream; last commit adds logging |
| [geeksville/cirque-input-module](https://github.com/geeksville/cirque-input-module) | `main` | `LEVEL_ACTIVE` | No (K_FOREVER) | No | ✓ | ✓ | Adds abs/rel mode; Z-mask fix (PR #2) |
| [geeksville/cirque-input-module](https://github.com/geeksville/cirque-input-module) | `toucan` (**yours**) | `LEVEL_ACTIVE` | **No (K_FOREVER)** | **No (buggy)** | ✓ | ✓ | Toucan production fork; `NUM_ZIDLE_PAD` added |
| [alee0729 fork (PR #1)](https://github.com/geeksville/cirque-input-module/pull/1) | `main`-based | `EDGE_TO_ACTIVE` (fixed) | Yes | Yes | ✓ | ✓ | Supply GPIO, adaptive sample rate, no-glide-extend |

**Notable open PRs on geeksville/cirque-input-module:**

- **[PR #1: Enhanced power savings](https://github.com/geeksville/cirque-input-module/pull/1)** (open, by alee0729): adds `supply-gpios` for power gating, adaptive sample rate, `no-glide-extend`, configurable `sleep-interval`/`sleep-timer`, switches back to `EDGE_TO_ACTIVE`.  
  *Status: open as of 2026-06-05. Contains the most comprehensive power improvements.*

- **[PR #4: K_NO_WAIT fix](https://github.com/geeksville/cirque-input-module/pull/4)** (open): fixes system workqueue deadlock.  
  *Status: open, not merged into toucan.*

- **[PR #5: DR IRQ re-arm fix](https://github.com/geeksville/cirque-input-module/pull/5)** (open): fixes silent trackpad death on SPI error.  
  *Status: open, not merged into toucan.*

- **[PR #3: K_NO_WAIT + re-arm combined](https://github.com/geeksville/cirque-input-module/pull/3)** (closed, split into #4 and #5): original combined fix, confirmed working on hardware.

- **[alee0729/zmk-keyboard-toucan PR #42](https://github.com/alee0729/zmk-keyboard-toucan/pull/42)**: enables `sleep;`, `no-glide-extend;`, `adaptive-sample-rate;`, and `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y` in concert.

---

## 3. ZMK Documentation & Community Reports

### 3.1 ZMK Documentation

**Low Power States** ([zmk.dev/docs/features/low-power-states](https://zmk.dev/docs/features/low-power-states)):
- **Idle**: displays/lighting off, BLE connected, 30 s default timeout
- **Deep Sleep** (`CONFIG_ZMK_SLEEP=y`): BLE disconnected, RAM cleared, `sys_poweroff()`. Requires `wakeup-source` on kscan nodes.
- **Soft Off** (`CONFIG_ZMK_PM_SOFT_OFF=y`): same SYSTEM_OFF state but triggered explicitly, not by timeout

**For proper deep sleep with an external SPI device:**
1. Device must have a `PM_DEVICE_DT_INST_DEFINE` with a `PM_DEVICE_ACTION_SUSPEND` handler ✓ (present in toucan driver)
2. The device must NOT be a wakeup source (or must be explicitly disabled in the PM suspend path) ✓ (DR interrupt disabled in `pinnacle_set_shutdown`)
3. SPI bus must have `pinctrl-1 = <&spiN_sleep>` and `CONFIG_PINCTRL_KEEP_SLEEP_STATE=y` ✓ (present; enabled by default when PM_DEVICE is on)

**Power Management Config** ([zmk.dev/docs/config/power](https://zmk.dev/docs/config/power)):
- `CONFIG_ZMK_IDLE_TIMEOUT=30000` (30 s): time before entering idle state  
- `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000` (60 min): time before entering deep sleep  
- `CONFIG_ZMK_PM_SOFT_OFF=y`: enables soft off; implies PM_DEVICE and ZMK_PM_DEVICE_SUSPEND_RESUME

### 3.2 Key GitHub Issues & PRs

| Source | URL | Summary |
|--------|-----|---------|
| beekeeb/toucan PR #5 | https://github.com/beekeeb/zmk-keyboard-toucan/pull/5 | First to add PM shutdown support, split conf files, add CS pull-up. Measured: 1.7 mA → 0.23 µA in deep sleep. |
| geeksville PR #7 (petejohanson) | https://github.com/petejohanson/cirque-input-module/pull/7 | "transitions from 1.7 mA to 0.23 µA – about 5000× savings". Notes programmatic sleep exit may be unreliable. |
| geeksville PR #1 | https://github.com/geeksville/cirque-input-module/pull/1 | Full power optimization plan with current numbers, supply GPIO, adaptive rate |
| geeksville PR #3 | https://github.com/geeksville/cirque-input-module/pull/3 | K_NO_WAIT + re-arm fix; confirmed working on Toucan hardware; "system wedged within ~15 min unpatched" |
| geeksville PR #5 | https://github.com/geeksville/cirque-input-module/pull/5 | DR IRQ re-arm fix only; diagnosed: "input-event counter froze mid-session" |
| alee0729/toucan PR #42 | https://github.com/alee0729/zmk-keyboard-toucan/pull/42 | Combines sleep + no-glide-extend + adaptive-sample-rate + idle_sleeper: "trackpad idle power drops from ~1.7 mA → ~40 µA during ZMK idle periods" |

### 3.3 Community Measurements

From PR discussions and commit messages:

| State | Right half (w/ Pinnacle) | Left half (keys only) | Ratio |
|-------|--------------------------|----------------------|-------|
| Active tracking (finger moving) | ~4–6 mA | ~1–1.5 mA | 4–5× |
| Active, no touch (ZMK=ACTIVE) | ~3–4 mA (Pinnacle 1.7 mA + BLE + MCU) | ~1–1.5 mA | 3× |
| ZMK IDLE (Pinnacle in standby) | ~1–2 mA (Pinnacle 40 µA + BLE ~1 mA) | ~0.5–0.8 mA | 2–3× |
| Deep sleep (SYSTEM_OFF) | ~2–5 µA | ~1–2 µA | ~2× |

geeksville comment: "only 1.7 mA idle vs 2.9 mA active" (in ZMK_IDLE_TIMEOUT context)  
beekeeb PR #5 comment: "cuts power while not in use from 1.7 mA to 0.23 µA (a 1000× decrease)"

---

## 4. Diagnostic Approach

### 4.1 Nordic PPK2 / Joulescope Inline Measurement

**Setup:**
1. Cut the right half's battery positive lead. Insert PPK2 (in AMPERE meter mode) or Joulescope in series between battery positive terminal and the board's BAT+ pin.
2. Use PPK2's "Measure" app at 100 kHz sample rate to capture waveforms.
3. Avoid USB connection during measurement (it may mask SYSTEM_OFF / change BLE state).

**Measurement protocol:**
1. Connect both halves over BLE
2. Observe baseline at rest (ZMK ACTIVE, nothing pressed): expect ~3–5 mA
3. Touch trackpad: expect brief current spikes (2.9 mA Pinnacle + BLE radio)
4. Wait 35+ seconds without input: expect visible drop to ~1–2 mA (ZMK→IDLE, idle sleeper engages)
5. Wait 65+ minutes: expect dramatic drop to <10 µA (deep sleep)

**Key pattern to look for:**
- If **step 4 drop never happens** → system workqueue deadlock (K_FOREVER bug)
- If **step 5 drop never happens** → something preventing `sys_poweroff()` (wakeup source left active, device suspend failed)
- If **step 4 drops but not to <1 mA** → idle sleeper working but something keeping power high (BLE, LDO)

### 4.2 ZMK USB Logging

Enable in right `toucan_right.conf`:
```
CONFIG_ZMK_USB_LOGGING=y
CONFIG_LOG_BACKEND_UART=n
CONFIG_ZMK_LOG_LEVEL_DBG=y
```

Then connect via USB UART and watch for:
```
[zmk] activity: Setting state to idle     ← ZMK idle after 30s
[zmk] activity: Setting state to sleep    ← ZMK sleep after 60min
[pinnacle] Setting sleep: on              ← idle_sleeper engaging
[pinnacle] Setting sleep: off             ← idle_sleeper disabling sleep (ACTIVE)
[pinnacle] Setting shutdown: on           ← PM suspend on deep sleep
```

**To detect K_FOREVER deadlock:**  
Add a self-rescheduling heartbeat work item (as done in PR #3 diagnosis):
```c
static void heartbeat_cb(struct k_work *w) {
    LOG_INF("SYS workqueue alive @ %lld", k_uptime_get());
    k_work_reschedule(&heartbeat_work, K_SECONDS(1));
}
K_WORK_DELAYABLE_DEFINE(heartbeat_work, heartbeat_cb);
```
If the heartbeat stops but the keyboard still "works" (BLE still connected from left), the system workqueue is deadlocked.

### 4.3 Expected Current Numbers per State

| State | Expected Total (right half) | Pinnacle contribution | nRF52840+BLE |
|-------|-----------------------------|-----------------------|--------------|
| ZMK ACTIVE, touching pad | **4–6 mA** | 2.9 mA | ~1.5–2 mA |
| ZMK ACTIVE, not touching (idle_sleeper disabled) | **3–4 mA** | 1.7 mA | ~1.5–2 mA |
| ZMK ACTIVE, not touching (idle_sleeper OFF, sleep DT ON) | **1.5–2 mA** | ~40 µA | ~1.5–2 mA |
| ZMK IDLE (30 s), Pinnacle standby | **1–2 mA** | ~40 µA | ~1 mA |
| ZMK SLEEP (60 min, SYSTEM_OFF) | **2–5 µA** | ~0.23 µA | ~1–2 µA |

---

## 5. Candidate Fixes / Mitigations (Ranked by Expected Impact)

### 5.1 Apply K_NO_WAIT + DR Re-Arm Fix (HIGHEST IMPACT for reliability/power)

Cherry-pick or manually apply the two open PRs:
- [PR #4](https://github.com/geeksville/cirque-input-module/pull/4): Replace `K_FOREVER` with `K_NO_WAIT` in all `input_report_*` calls in `input_pinnacle.c`
- [PR #5](https://github.com/geeksville/cirque-input-module/pull/5): Re-arm DR interrupt on every early-return path in `pinnacle_report_data_rel` and `pinnacle_report_data_abs`

**What to change in `input_pinnacle.c`:**

For `pinnacle_report_data_rel`:
```c
static void pinnacle_report_data_rel(const struct device *dev) {
    ...
    if (ret < 0) {
        LOG_ERR("read status: %d", ret);
        pinnacle_clear_status(dev);  // ← add
        set_int(dev, true);          // ← add
        return;
    }
    if (packet[0] == 0xFF || !(packet[0] & PINNACLE_STATUS1_SW_DR)) {
        set_int(dev, true);          // ← add
        return;
    }
    if (ret < 0) {
        LOG_ERR("read packet: %d", ret);
        pinnacle_clear_status(dev);  // ← add
        set_int(dev, true);          // ← add
        return;
    }
    ...
}
```

For `pinnacle_send_rel` / `pinnacle_send_abs`: change all `K_FOREVER` → `K_NO_WAIT`.

**Expected impact:** Prevents indefinite system workqueue block; ensures idle and deep-sleep timers can fire; trackpad no longer goes "dark" silently.

---

### 5.2 Choose ONE Power-Saving Strategy for Between-Touch Periods (HIGH IMPACT)

Your current config has conflicting modes. Pick one:

**Option A: Idle-sleeper only (recommended for better UX)**  
Comment out `sleep;` in the overlay (prevents DT enabling sleep at boot), keep `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y`.  
- Pinnacle stays fully active during ZMK ACTIVE period (good responsiveness)
- After 30 s idle: Pinnacle goes to standby (~40 µA)
- 300 ms wake delay only after 30 s of inactivity (acceptable)

**Option B: DT sleep only (original toucan design, lowest active-period power)**  
Keep `sleep;` in overlay, comment out `CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y`.  
- Pinnacle auto-sleeps ~400 ms after every touch
- 300 ms wake delay on EVERY gesture start (after any pause)
- Best power during active use; worse UX for rapid trackpad use

**Option C: Both (your current state) – avoid**  
The idle sleeper negates the DT sleep property during active periods. No benefit over Option A plus extra code complexity.

**Change for Option A in `toucan_right.overlay`:**
```c
// Remove or comment out the 'sleep;' line
glidepoint: glidepoint@0 {
    // sleep;   ← remove if using idle_sleeper only
    ...
};
```

---

### 5.3 Tune SLEEP_TIMER to Fast Entry (MEDIUM IMPACT)

The Pinnacle SLEEP_TIMER (reg 0x0D) defaults to 39 (factory: ~390 ms before entering sleep). The current driver does NOT set SLEEP_TIMER explicitly. Combined with SLEEP_INTERVAL set to 255 (which the driver does set), the time before entering sleep may be much longer.

If using Option B (DT sleep without idle_sleeper), tune the sleep entry timing:

**In `input_pinnacle.c`, after setting SLEEP_INTERVAL:**
```c
ret = pinnacle_write(dev, PINNACLE_SLEEP_INTERVAL, 255);  // existing
// Add SLEEP_TIMER: enter sleep after ~400ms of no touch
ret = pinnacle_write(dev, PINNACLE_SLEEP_TIMER, 2);       // ← add: 2 × (SLEEP_INTERVAL units) ≈ short
```

Note: the exact SLEEP_TIMER-to-time formula requires the Pinnacle register guide. The optimization plan (PR #1) suggests `default 39` matches factory. A lower value (2–5) enters sleep faster.

---

### 5.4 Disable GlideExtend (LOW-MEDIUM IMPACT)

GlideExtend is a Pinnacle firmware feature that extends coasting gesture tracking after finger lift. It keeps the analog front-end active for ~200 ms after each lift, generating extra interrupt events and MCU wakeups.

The alee0729 fork (PR #1) adds `no-glide-extend;` DT property:
```c
// In overlay, glidepoint node:
no-glide-extend;
```
And in `pinnacle_init`:
```c
if (config->no_glide_extend) {
    feed_cfg2 |= PINNACLE_FEED_CFG2_DIS_GE;
}
```

**Expected impact:** Reduces post-lift IRQ count, reduces brief active-mode current after each touch, reduces BLE forwarding events.

---

### 5.5 Reduce BLE Scan Rate with Adaptive Sample Rate (LOW IMPACT during active use)

The alee0729 fork (PR #1) also adds `adaptive-sample-rate;` which reduces the Pinnacle scan rate from 100 Hz to 25 Hz when the pointer is stationary. During slow/stop periods, fewer DR interrupts fire, fewer BLE events are forwarded, and the BLE radio can take more latency slots.

This is available in the alee0729 fork but not the geeksville/toucan branch.

---

### 5.6 Hardware Power Gating (HARDWARE MOD, OPTIONAL)

If the breakout board has an LDO with significant quiescent current (>50 µA), or I2C pull-up resistors drawing current in SPI mode, hardware modifications can help:

1. **Cut I2C pull-up resistors** on the Cirque dev kit board (the 10 kΩ resistors on SDA/SCL lines) – saves 660 µA baseline.
2. **Bypass the onboard LDO** and drive Pinnacle VDD directly from the XIAO BLE's 3V3 pin – saves 50–200 µA quiescent.
3. **Add a MOSFET/load-switch** controlled by a GPIO to power gate the entire Pinnacle during deep sleep – achieves true 0 µA draw (add `supply-gpios` DT property from PR #1).

The alee0729 fork (PR #1) adds `supply-gpios` support:
```c
// In overlay:
supply-gpios = <&gpio0 XX GPIO_ACTIVE_HIGH>;  // XX = your control pin
```
```c
// PM_DEVICE_ACTION_SUSPEND: cuts VDD via GPIO
// PM_DEVICE_ACTION_RESUME: restores VDD, performs soft reset
```

---

### 5.7 Kconfig Summary of Recommended Changes

In `toucan_right.conf` (recommended configuration):
```
# ✓ Keep as-is:
CONFIG_ZMK_POINTING=y
CONFIG_ZMK_MOUSE=y
CONFIG_ZMK_BATTERY_REPORTING=y
CONFIG_ZMK_SLEEP=y
CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=3600000
CONFIG_ZMK_IDLE_TIMEOUT=30000
CONFIG_ZMK_PM_SOFT_OFF=y

# Choose ONE:
# Option A: idle_sleeper only (better UX, standby after 30s idle)
CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y

# Option B: DT sleep only (original toucan, standby immediately after touch)
# CONFIG_ZMK_INPUT_PINNACLE_IDLE_SLEEPER=y  ← comment out

# Remove from right conf (only relevant on central):
# CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y  ← already commented out ✓
# CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY=y     ← already commented out ✓
```

In `toucan_right.overlay` (recommended additions):
```c
glidepoint: glidepoint@0 {
    ...
    sleep;              // keep (needed for either option)
    // no-glide-extend;  // add when PR #1 is merged/cherry-picked
    ...
};
```

---

## Sources

| # | URL | Description |
|---|-----|-------------|
| 1 | https://github.com/geeksville/cirque-input-module/blob/toucan/drivers/input/input_pinnacle.c | Primary driver source (your firmware) |
| 2 | https://github.com/geeksville/cirque-input-module/blob/toucan/drivers/input/zmk_pinnacle_idle_sleeper.c | Idle sleeper source |
| 3 | https://github.com/geeksville/cirque-input-module/pull/1 | Power optimization plan; current numbers table (Active 2.9 mA, Idle 1.7 mA, Sleep 40 µA, Shutdown 0.23 µA) |
| 4 | https://github.com/geeksville/cirque-input-module/pull/3 | K_NO_WAIT + DR re-arm fix (combined); hardware test results |
| 5 | https://github.com/geeksville/cirque-input-module/pull/4 | K_NO_WAIT fix (open) |
| 6 | https://github.com/geeksville/cirque-input-module/pull/5 | DR IRQ re-arm fix (open) |
| 7 | https://github.com/beekeeb/zmk-keyboard-toucan/pull/5 | Original toucan PM shutdown + CS pull-up addition; "1000× savings" quote; idle_sleeper commentary |
| 8 | https://github.com/petejohanson/cirque-input-module/pull/7 | PM shutdown upstream PR; "0.23 µA" measurement; notes on sleep exit reliability |
| 9 | https://github.com/alee0729/zmk-keyboard-toucan/pull/42 | Toucan right power config enabling sleep + no-glide-extend + adaptive-sample-rate |
| 10 | https://github.com/zmkfirmware/zmk/blob/main/app/src/activity.c | ZMK activity.c; idle timer, INPUT_CALLBACK_DEFINE |
| 11 | https://github.com/zmkfirmware/zmk/blob/main/app/src/pm.c | ZMK pm.c; zmk_pm_suspend_devices(), zmk_pm_soft_off() |
| 12 | https://github.com/zmkfirmware/zmk/blob/main/app/src/pointing/input_split.c | Input split peripheral handler; every event forwarded via BLE |
| 13 | https://github.com/zmkfirmware/zmk/blob/main/app/src/split/bluetooth/Kconfig | BLE split connection parameters (INT=6, LATENCY=30) |
| 14 | https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/drivers/spi/spi_nrfx_spim_common.c | nRF SPIM PM action; PINCTRL_KEEP_SLEEP_STATE applies sleep pinctrl on suspend |
| 15 | https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/drivers/pinctrl/Kconfig | `CONFIG_PINCTRL_KEEP_SLEEP_STATE` defaults to y when PM_DEVICE enabled |
| 16 | https://zmk.dev/docs/features/low-power-states | ZMK low power states documentation |
| 17 | https://zmk.dev/docs/config/power | ZMK power management configuration reference |
| 18 | https://github.com/kalbasit/zmk-config/pull/219 | Another Toucan user hitting same K_NO_WAIT lock-up |
