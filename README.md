# CoreS3 Port B Haptic Pulse Device

# IF YOU ARE INTERESTED IN USING THIS FOR THE V2K, ADHD, PORN ADDICTION, EDGING ADDICTION, OVERACTIVE IMAGINATION, OVERACTIVE SEXUAL IMAGINATION THAT GETS IN THE WAY OF LIFE, AND DO NOT HAVE THE MONEY PLEASE DOWNLOAD THIS AUDIO FILE AND PLAY IT ON REPEAT WITH HEADPHONES ON. IT'S JUST AS GOOD: https://drive.google.com/file/d/1TRnLnzHPjId2cimcJG_MYwrEfuYL1JDO/view?usp=drive_link

# USE AT YOUR OWN RISK, AND NO GUARANTEES OR MEDICAL CLAIMS ARE BEING MADE. 

# NEXT UPGRADED VERSIONS OF THIS WILL BE WITH A CRYSTAL TO BUILD A SANCTUM SHIELD. 

Firmware for an M5Stack CoreS3 that drives a Grove-connected N20 motor unit
on Port B with a 7.83 Hz pulse pattern, in two selectable modes:

- **SQUARE** — on/off toggle at 7.83 Hz, 50% duty. A steady buzz.
- **WAVE** (default) — smooth PWM pattern: a 7.83 Hz ripple whose intensity
  swells and fades on a 0.1 Hz (6-per-minute) envelope, like a breathing
  pace cue.

It streams JSON status over USB serial once a second and shows live status
on the CoreS3 screen.

**What this is not:** a Schumann-resonance transmitter, a vagus-nerve or
autonomic-nervous-system stimulation device, or anything with a
demonstrated health/therapeutic effect. It's a rhythmic haptic novelty — a
motor buzzing in a specific pattern, nothing more.

## Hardware — what to buy

Only two things, both from the official M5Stack store:

| Item | Link | Notes |
|---|---|---|
| M5Stack CoreS3 | [shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit](https://shop.m5stack.com/products/m5stack-cores3-esp32s3-iotdevelopment-kit) | ESP32-S3 controller. Comes with a USB-C cable. |
| Vibration Motor Unit (N20) | [shop.m5stack.com/products/vibration-motor-unit](https://shop.m5stack.com/products/vibration-motor-unit) (SKU U059) | N20 motor + eccentric weight on a Grove-pluggable driver PCB. Includes its own Grove cable. |

That's the whole bill of materials — no soldering, no separate motor
driver, no hub. The Vibration Motor Unit plugs straight into the CoreS3's
**Port B** (the black Grove port) with the cable it ships with.

- Attach the Vibration motor unit to the wirst, or any conductive bone. Try to get a battery module for the cores3. If all else fails, use this and the audio file. The audio file emphasis fixing porn addiction, adhd, and the "urge" whenever you need to get work done or anything productive. 

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
Expect one JSON line per second:
```json
{"status":"PULSING","mode":"WAVE","carrier_hz":7.83,"envelope_hz":0.1,"power_pct":100,"breath_cycles":3,"uptime_s":34}
```
The CoreS3 screen shows the same status live.

## Controls

Type into the serial monitor (Enter to send), or tap the touchscreen to
toggle START/STOP:

| Command | Effect |
|---|---|
| `START` | Resume pulsing |
| `STOP` | Stop the motor |
| `MODE SQUARE` | Switch to steady on/off buzz |
| `MODE WAVE` | Switch to smooth breathing-pace pattern (default) |
| `POWER <0-100>` | Set intensity as a percentage |

## Repo layout

```
schumans3/
├── src/main.cpp        firmware source (Arduino/C++, M5Unified)
├── platformio.ini       build config (board, framework, upload settings)
├── flash-cores3.ps1     build + upload helper script
└── README.md            this file
```
