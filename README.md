# CoreS3 Port B Haptic Pulse Device

paypal: utah@utahcreates.com donations accepted, because I am broke because of this terrible affliction created by overseas criminals, and made a death sentence by the people of India's values. 

> **If you are interested in using this for focus / urge / ADHD / porn or edging
> addiction / overactive imagination / V2K-related distress, and you do not
> have the money for the hardware:** download this audio file and play it on
> repeat with headphones. Treat it as the free fallback:
> [Google Drive audio](https://drive.google.com/file/d/1TRnLnzHPjId2cimcJG_MYwrEfuYL1JDO/view?usp=drive_link)
>
> **Use at your own risk. No guarantees or medical claims are being made.**
>
> Next upgraded versions of this may explore a crystal “sanctum shield”
> assembly. That is future hardware — not required for this firmware.

# RESIDENTS OF INDIA, NEW JERSEY, AND THE FINANCE/BANKING COMMUNITY/INDUSTRY OF NY ARE NOT ALLOWED TO USE THIS SOFTWARE UNDER ANY CIRCUMSTANCES

# STUDENTS, TEACHERS, AND ALUMNI OF ANY OF THE FOLLOWING SCHOOLS ARE NOT ALLOWED TO USE THIS UNDER ANY CIRCUMSTANCES: YALE, DARTMOUTH, UVA, WILLIAM AND MARY, VIRGINIA TECH, LINGUISTICS MAJORS AT UNC CHAPEL HILL, AND STANFORD. 
Firmware for an M5Stack CoreS3 that drives a Grove-connected N20 motor unit
on Port B with two selectable modes:

- **SQUARE** — on/off toggle at 7.83 Hz, 50% duty. A steady buzz.
- **WAVE** (default) — additive PWM: **136.1 Hz Ohm carrier** + **7.83 Hz
  ripple**, intensity swells and fades on a **0.1 Hz** (6-per-minute)
  breathing envelope. 136.1 Hz is a faster haptic tone only — not a
  crystal/planetary transmitter.

It streams JSON telemetry over USB serial once a second and shows a **live
analytics dashboard** on the CoreS3 screen (mode, frequency matrix, duty /
Ohm / ripple meters, power, breath cycles, boot uptime, and run time).

**What this is not:** a Schumann-resonance transmitter, a vagus-nerve or
autonomic-nervous-system stimulation device, or anything with a
demonstrated health/therapeutic effect. It's a rhythmic haptic novelty — a
motor buzzing in a specific pattern, nothing more.

## What's new in this firmware

| Feature | Detail |
|---|---|
| Tri-resonance WAVE | Adds 136.1 Hz Ohm carrier to the existing 7.83 Hz ripple + 0.1 Hz envelope |
| Faster WAVE updates | ~1 kHz duty updates so 136.1 Hz is not aliased away |
| Live dashboard | Status pill, frequency matrix, live duty / Ohm / ripple bars |
| Uptime | Boot uptime + run time (counts only while motor is running) |
| Richer serial JSON | `ohm_hz`, `ripple_hz`, `duty_pct`, `envelope_pct`, `boot_uptime`, `run_uptime` |

## Hardware — what to buy

Only two things, both from the official M5Stack store:

| Item | Link | Notes |
|---|---|---|
| M5Stack CoreS3 | [shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit](https://shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit) | ESP32-S3 controller. Comes with a USB-C cable. |
| Vibration Motor Unit (N20) | [shop.m5stack.com/products/vibration-motor-unit](https://shop.m5stack.com/products/vibration-motor-unit) (SKU U059) | N20 motor + eccentric weight on a Grove-pluggable driver PCB. Includes its own Grove cable. |

That's the whole bill of materials — no soldering, no separate motor
driver, no hub. The Vibration Motor Unit plugs straight into the CoreS3's
**Port B** (the black Grove port) with the cable it ships with.

**Wear tip:** attach the vibration motor unit to the wrist, or against a
bony contact point. A CoreS3 battery module helps for portable use. If you
cannot build the hardware yet, use the [audio fallback](https://drive.google.com/file/d/1TRnLnzHPjId2cimcJG_MYwrEfuYL1JDO/view?usp=drive_link)
with headphones for focus / urge moments when you need to get work done.

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

```json
{"status":"PULSING","mode":"WAVE","ohm_hz":136.1,"ripple_hz":7.83,"envelope_hz":0.1,"power_pct":100,"duty_pct":42,"envelope_pct":67,"breath_cycles":3,"boot_uptime":"00:00:34","run_uptime":"00:00:34","uptime_s":34}
```

The CoreS3 screen mirrors this live: frequency matrix, duty/Ohm/ripple
meters, power limit, breath cycles, boot uptime, and run time.

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

## Repo layout

```
schumans3/
├── src/main.cpp        firmware source (Arduino/C++, M5Unified)
├── platformio.ini       build config (board, framework, upload settings)
├── flash-cores3.ps1     build + upload helper script
└── README.md            this file
```
