# CoreS3 Port B Haptic Pulse Device

**PayPal donations:** [utah@utahcreates.com](mailto:utah@utahcreates.com) —
donations accepted, because I am broke because of this terrible affliction
created by overseas criminals, and made a death sentence by the people of
India's values.

> **If you are interested in using this for focus / urge / ADHD / porn or edging
> addiction / overactive imagination / V2K-related distress, and you do not
> have the money for the hardware:** download this audio file and play it on
> repeat with STEREO headphones. Treat it as the free fallback:
> [Google Drive audio](https://drive.google.com/file/d/1TRnLnzHPjId2cimcJG_MYwrEfuYL1JDO/view?usp=drive_link)
>
> **Use at your own risk. No guarantees or medical claims are being made.**
> **DO NOT BUILD MULTIPLE OF THESE TYPES OF DEVICES TO USE AT THE SAME TIME FOR ADDED PROTECTION. JUST 1 CORES3 AND THE AUDIO FILE FOR SEVERE CASES.**
> Next upgraded versions of this may explore a crystal “sanctum shield”
> assembly. That is future hardware — not required for this firmware.

> **Usage restrictions:** Residents of the Not Poor parts of India, New Jersey, and the
> finance/banking community/industry of NY are not allowed to use this
> software under any circumstances.
>
> Students, teachers, and alumni of any of the following schools are not
> allowed to use this under any circumstances: Yale, Dartmouth, UVA, William
> and Mary, Virginia Tech, linguistics majors at UNC Chapel Hill, and
> Stanford.

Firmware for an M5Stack CoreS3 that drives a Grove-connected N20 motor unit
on Port B with two selectable modes:

- **SQUARE** — on/off toggle at 7.83 Hz, 50% duty. A steady buzz.
- **WAVE** (default) — additive PWM: **136.1 Hz Ohm carrier** + **7.83 Hz
  ripple**, intensity swells and fades on a **0.1 Hz** (6-per-minute)
  breathing envelope. 136.1 Hz is a faster haptic tone only — not a
  crystal/planetary transmitter.

It streams JSON telemetry over USB serial once a second (when a PC is
attached) and shows a **live analytics dashboard** on the CoreS3 screen
(mode, frequency matrix, duty / Ohm / ripple meters, power, breath cycles,
boot uptime, and run time).

**Standalone power:** flash once from a PC, then you can unplug and run from
any 5 V USB-C wall adapter, power bank, or M5Stack battery module. The motor
and UI auto-start in WAVE mode — no computer required after flashing.

**Optional desk / ambient mode:** plug an [ENV III](https://shop.m5stack.com/products/env-iii-unit-with-temperature-humidity-air-pressure-sensor-sht30-qmp6988)
into **Port A** (red). Firmware then shows live pressure / temp / humidity and
can micro-adjust the Ohm carrier (~134–138 Hz) from barometric pressure.
This is novelty desk tuning so you can leave the unit on a workstation — it
does **not** create a room-scale “resonance bubble” or replace body contact
haptics with real atmospheric impedance matching.

**What this is not:** a Schumann-resonance transmitter, a vagus-nerve or
autonomic-nervous-system stimulation device, or anything with a
demonstrated health/therapeutic effect. It's a rhythmic haptic novelty — a
motor buzzing in a specific pattern, nothing more.

## What's new in this firmware

| Feature | Detail |
|---|---|
| Dual-core FreeRTOS | Wave synthesis pinned to **Core 1** with `vTaskDelayUntil` (1 ms); UI/serial on Core 0 |
| Hardware LEDC PWM | 20 kHz silent carrier on the ESP32-S3 LEDC peripheral (10-bit duty) |
| Tri-resonance WAVE | 136.1 Hz Ohm + 7.83 Hz ripple + 0.1 Hz envelope |
| Optional ENV III ambient | Port A pressure/temp can detune Ohm in 134–138 Hz; works without the sensor |
| Live dashboard | Status pill, frequency matrix, live meters, ENV III readouts when present |
| Uptime | Boot uptime + run time (counts only while motor is running) |
| Standalone boot | Runs on battery / wall USB with no PC attached |
| Richer serial JSON | `ohm_hz`, `env_iii`, `pressure_hPa`, `temp_c`, `ambient`, uptimes |

## Hardware — what to buy

### Required

| Item | Link | Notes |
|---|---|---|
| M5Stack CoreS3 | [shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit](https://shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit) | ESP32-S3 controller. Comes with a USB-C cable. |
| Vibration Motor Unit (N20) | [shop.m5stack.com/products/vibration-motor-unit](https://shop.m5stack.com/products/vibration-motor-unit) (SKU U059) | N20 motor + eccentric weight on a Grove-pluggable driver PCB. Includes its own Grove cable. |

That's the required bill of materials — no soldering, no separate motor
driver, no hub. The Vibration Motor Unit plugs straight into the CoreS3's
**Port B** (the black Grove port) with the cable it ships with.

### Optional — ENV III environmental sensor (desk / ambient)

**Optional.** Not required for wearable / Port B motor use. Available on the
official M5Stack store. Plug into **Port A** (red Grove / I2C). Firmware
auto-detects it; if missing, Ohm stays fixed at 136.1 Hz and everything else
still works.

| Item | Link | Notes |
|---|---|---|
| ENV III Unit (SHT30 + QMP6988) | [shop.m5stack.com/products/env-iii-unit-with-temperature-humidity-air-pressure-sensor-sht30-qmp6988](https://shop.m5stack.com/products/env-iii-unit-with-temperature-humidity-air-pressure-sensor-sht30-qmp6988) (SKU U001-C) | Temp, humidity, barometric pressure. Grove cable included. Port A only. |

With ENV III attached you can leave the CoreS3 on a desk: the UI shows live
environment stats, and `AMBIENT ON` (default when the sensor is found)
nudges the Ohm carrier from pressure/temperature. Use `AMBIENT OFF` for a
fixed 136.1 Hz carrier.

### Optional (highly recommended) — neodymium magnet

**Optional, but very worth it.** Attach a small neodymium disc magnet to the
motor’s moving mass / shaft area so the permanent magnet physically
oscillates with each pulse. That increases the local kinetic feel and the
localized magnetic flux swing from the vibration (a mechanical amplification
hack — not a medical field emitter).

| Item | Link | Notes |
|---|---|---|
| DIYMAG small rare-earth magnets (40 pack, 5 sizes) | [amazon.com/dp/B09TVXL3DZ](https://www.amazon.com/dp/B09TVXL3DZ) | Pick a small disc that fits the N20 weight without throwing the rotor too far off balance. |

**Mount tip:** secure the magnet firmly (tape, heat-shrink, or adhesive) so it
cannot fly off at speed. Start with a smaller disc from the pack; larger
magnets hit harder but stress the motor more. Keep magnets away from credit
cards, pacemakers, and hard drives.

**Wear tip:** attach the vibration motor unit to the wrist, or against a
bony contact point. A CoreS3 battery module helps for portable use. If you
cannot build the hardware yet, use the [audio fallback](https://drive.google.com/file/d/1TRnLnzHPjId2cimcJG_MYwrEfuYL1JDO/view?usp=drive_link)
with headphones for focus / urge moments when you need to get work done.

## Architecture (why this is not the MicroPython paste)

| Idea from the ambient / ENV III paste | What this repo actually does |
|---|---|
| ENV III on Port A (I2C) | Yes — optional auto-detect; never blocks boot if absent |
| Pressure-tuned Ohm carrier | Yes — small 134–138 Hz novelty detune on Core 0 every 2 s |
| FreeRTOS Core 1 wave + Core 0 sensors | Yes — keeps 1 ms wave / 20 kHz LEDC (rejects 5 ms / 5 kHz paste) |
| Live dashboard + JSON | Yes — ENV fields when present; full analytics retained |
| Downgrade M5Unified / 1.5 Mbaud upload | No — keeps proven CoreS3 USB-CDC flash settings |

## One-time software setup

1. **Install Python 3** (3.9+): https://python.org
2. **Install PlatformIO Core:**
   ```bash
   pip install platformio
   ```
   Verify:
   ```bash
   python -m platformio --version
   ```

## Flashing

1. Clone this repo:
   ```bash
   git clone https://github.com/utahisnotastate/schumans3.git
   cd schumans3
   ```
2. Plug the CoreS3 into your PC via USB-C.
3. Find its COM port (Windows):
   ```powershell
   Get-PnpDevice -Class Ports -PresentOnly | Format-Table FriendlyName, InstanceId -AutoSize
   ```
   Look for a `USB Serial Device` with hardware ID containing `VID_303A`
   (Espressif) — that's the CoreS3.
4. Flash it:
   ```powershell
   $env:M5_UPLOAD_PORT = "COM8"   # replace COM8 with your port
   .\flash-cores3.ps1
   ```
   Takes about 20-30 seconds.

   Or without the helper script (any OS):
   ```bash
   python -m platformio run -e m5stack-cores3 -t upload --upload-port COM8
   ```

5. After a successful flash you can unplug from the PC and power the CoreS3
   from a wall adapter, power bank, or battery module — WAVE starts on boot.

### If the upload fails

- **"No serial data received"** — hold the CoreS3's **LEFT** button, tap
  **RESET**, release both, then re-run within ~10 seconds.
- **Port busy / locked** — close any other serial monitor, Arduino IDE, or
  terminal holding the port, then retry.
- **Wrong port auto-detected** — set `$env:M5_UPLOAD_PORT` explicitly before
  running the script.

## Verifying it's alive

```bash
python -m platformio device monitor -e m5stack-cores3 --port COM8
```

Expect one JSON line per second (WAVE mode):

Without ENV III:

```json
{"status":"PULSING","mode":"WAVE","ohm_hz":136.10,"ripple_hz":7.83,"envelope_hz":0.1,"power_pct":100,"duty_pct":42,"envelope_pct":67,"breath_cycles":3,"wave_core":1,"env_iii":false,"ambient":false,"boot_uptime":"00:00:34","run_uptime":"00:00:34","uptime_s":34,"standalone":true}
```

With ENV III on Port A, JSON also includes `pressure_hPa`, `temp_c`,
`humidity_pct`, `altitude_m`, and `ambient`. `wave_core` should be `1`.

## Controls

Type into the serial monitor (Enter to send), or tap the touchscreen to
toggle START/STOP:

| Command | Effect |
|---|---|
| `START` | Resume pulsing |
| `STOP` | Stop the motor |
| `MODE SQUARE` | Switch to steady on/off buzz |
| `MODE WAVE` | Switch to Ohm + ripple + breathing envelope (default) |
| `POWER <0-100>` | Set intensity as a percentage |
| `AMBIENT ON` | Enable ENV III Ohm detune (needs ENV III on Port A) |
| `AMBIENT OFF` | Lock Ohm at 136.1 Hz even if ENV III is present |

## Repo layout

```
schumans3/
├── src/main.cpp        firmware (dual-core FreeRTOS + LEDC + dashboard)
├── platformio.ini       build config (board, framework, upload settings)
├── flash-cores3.ps1     build + upload helper script
└── README.md            this file
```
