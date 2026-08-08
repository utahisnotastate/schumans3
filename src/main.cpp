#include <Arduino.h>
#include <M5Unified.h>
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// CoreS3 + Port B DC motor haptic pulse device.
//
// Two modes:
//  - SQUARE: on/off toggle at 7.83 Hz, 50% duty. A steady buzz.
//  - WAVE:   additive synthesis — 136.1 Hz "Ohm" carrier + 7.83 Hz ripple,
//            both amplitude-scaled by a 0.1 Hz (6-per-minute) envelope.
//            6 cycles/min paced breathing is a real, evidence-informed
//            relaxation technique used in HRV biofeedback practice. This
//            firmware borrows that *timing* honestly. 136.1 Hz is just a
//            higher-rate haptic tone some people associate with "Ohm";
//            it is not an orbital or planetary broadcast.
//
// What this does NOT do, in either mode: "broadcast" or recreate the actual
// Schumann resonance (a planetary-scale ionospheric phenomenon), stimulate
// the vagus nerve, or "force" heart-rate-variability coherence. There is no
// mainstream scientific mechanism by which a motor vibrating against skin
// does that. An N20 eccentric motor also cannot cleanly reproduce a pure
// 136.1 Hz acoustic "note" — rotor inertia averages rapid duty changes.
// Treat this as a rhythmic haptic novelty / breathing-pace cue, not a
// medical or nerve-stimulation device. If you're using this to manage
// something more serious than everyday stress, please talk to an actual
// clinician - this is not a substitute for that.
// ---------------------------------------------------------------------------

static const int MOTOR_PIN = 8;             // Port B pin 1 (G8) on CoreS3
static const int MOTOR_PIN2 = 9;            // Port B pin 2 (G9) - held low
static const double PULSE_HZ = 7.83;        // slow ripple / square rate
static const double OHM_HZ = 136.1;         // faster haptic carrier (WAVE only)
static const uint32_t HALF_PERIOD_US =
    (uint32_t)((1000000.0 / PULSE_HZ) / 2.0);  // ~63857 us

static const int PWM_RES_BITS = 8;          // 0-255 duty range
static const int PWM_CARRIER_HZ = 20000;    // inaudible
static const int PWM_CHANNEL = 0;
uint8_t powerLevel = 255;

enum PulseMode : uint8_t { MODE_SQUARE = 0, MODE_WAVE = 1 };
PulseMode mode = MODE_WAVE;

static const double WAVE_ENVELOPE_HZ = 0.1;
static const float WAVE_MAX_FRACTION = 0.85f;
static const uint32_t WAVE_UPDATE_INTERVAL_US = 1000;
uint32_t waveStartUs = 0;
uint32_t lastWaveUpdateUs = 0;
uint32_t breathCycles = 0;

bool motorOn = false;
bool running = true;
uint32_t lastToggle = 0;
uint32_t pulseCount = 0;
uint32_t lastTelemetry = 0;
uint32_t lastUiMs = 0;
uint32_t bootMs = 0;
uint32_t runSessionStartMs = 0;   // wall time when current RUN began
uint32_t runAccumMs = 0;         // total time spent running across sessions

// Live analytics sampled from the drive loop
uint8_t liveDuty = 0;
float liveEnvelope = 0.0f;       // 0..1 breathing envelope
float liveOhm = 0.0f;            // -1..1
float liveRipple = 0.0f;         // -1..1

// UI layout (CoreS3 320x240)
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_PANEL = 0x1082;     // dark slate
static const uint16_t COL_ACCENT = 0x07FF;    // cyan
static const uint16_t COL_OK = 0x07E0;        // green
static const uint16_t COL_STOP = 0xF800;      // red
static const uint16_t COL_MUTED = 0x8410;     // grey
static const uint16_t COL_TEXT = TFT_WHITE;
static const uint16_t COL_BAR_BG = 0x2104;
static const uint16_t COL_BAR_FILL = 0x05FF;
static const uint16_t COL_BAR_ENV = 0xFFE0;   // yellow envelope marker

void applyMotorState(bool on) {
    motorOn = on;
    liveDuty = on ? powerLevel : 0;
    ledcWrite(PWM_CHANNEL, liveDuty);
}

void formatUptime(char *buf, size_t n, uint32_t sec) {
    uint32_t h = sec / 3600UL;
    uint32_t m = (sec % 3600UL) / 60UL;
    uint32_t s = sec % 60UL;
    snprintf(buf, n, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m,
             (unsigned long)s);
}

uint32_t currentRunMs(uint32_t nowMs) {
    if (!running) return runAccumMs;
    return runAccumMs + (nowMs - runSessionStartMs);
}

void drawBar(int x, int y, int w, int h, float frac, uint16_t fill,
             float marker = -1.0f) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    M5.Lcd.fillRect(x, y, w, h, COL_BAR_BG);
    int fw = (int)(frac * w + 0.5f);
    if (fw > 0) M5.Lcd.fillRect(x, y, fw, h, fill);
    M5.Lcd.drawRect(x, y, w, h, COL_MUTED);
    if (marker >= 0.0f) {
        if (marker > 1.0f) marker = 1.0f;
        int mx = x + (int)(marker * (w - 1));
        M5.Lcd.drawFastVLine(mx, y - 1, h + 2, COL_BAR_ENV);
    }
}

void drawDashboard(bool fullRedraw) {
    const uint32_t nowMs = millis();
    const int powerPct = (int)(powerLevel * 100 / 255);
    const int dutyPct = (int)(liveDuty * 100 / 255);
    char bootUp[16];
    char runUp[16];
    formatUptime(bootUp, sizeof(bootUp), (nowMs - bootMs) / 1000UL);
    formatUptime(runUp, sizeof(runUp), currentRunMs(nowMs) / 1000UL);

    if (fullRedraw) {
        M5.Lcd.fillScreen(COL_BG);

        // Header bar
        M5.Lcd.fillRect(0, 0, SCREEN_W, 28, COL_PANEL);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(COL_ACCENT, COL_PANEL);
        M5.Lcd.setCursor(6, 6);
        M5.Lcd.print("SCHUMAN S3");

        // Static labels / help footer
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(COL_MUTED, COL_BG);
        M5.Lcd.setCursor(6, 226);
        M5.Lcd.print("Tap screen: start/stop   Serial: START STOP POWER MODE");
    }

    // Status pill (right of header)
    M5.Lcd.fillRoundRect(200, 4, 114, 20, 4,
                         running ? (motorOn ? COL_OK : COL_ACCENT) : COL_STOP);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_BLACK,
                         running ? (motorOn ? COL_OK : COL_ACCENT) : COL_STOP);
    M5.Lcd.setCursor(210, 10);
    if (!running) {
        M5.Lcd.print("  STOPPED  ");
    } else if (mode == MODE_WAVE) {
        M5.Lcd.print(motorOn ? "  EMITTING " : "  SWELLING ");
    } else {
        M5.Lcd.print(motorOn ? "  PULSING  " : "  IDLE GAP ");
    }

    // Mode + engine line
    M5.Lcd.fillRect(0, 30, SCREEN_W, 18, COL_BG);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(COL_TEXT, COL_BG);
    M5.Lcd.setCursor(6, 34);
    if (mode == MODE_SQUARE) {
        M5.Lcd.printf("MODE  SQUARE   engine  %.2f Hz square @ Port B G8",
                      PULSE_HZ);
    } else {
        M5.Lcd.printf("MODE  WAVE     engine  Ohm+ripple+envelope @ Port B G8");
    }

    // Frequency panel
    M5.Lcd.fillRect(4, 52, 198, 78, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 58);
    M5.Lcd.print("FREQUENCY MATRIX");
    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(10, 74);
    if (mode == MODE_WAVE) {
        M5.Lcd.printf("Ohm carrier   %6.1f Hz", OHM_HZ);
        M5.Lcd.setCursor(10, 88);
        M5.Lcd.printf("Ripple        %6.2f Hz", PULSE_HZ);
        M5.Lcd.setCursor(10, 102);
        M5.Lcd.printf("Envelope      %6.2f Hz  (6 cpm)", WAVE_ENVELOPE_HZ);
        M5.Lcd.setCursor(10, 116);
        M5.Lcd.printf("PWM carrier   %5d Hz  silent", PWM_CARRIER_HZ);
    } else {
        M5.Lcd.printf("Pulse         %6.2f Hz", PULSE_HZ);
        M5.Lcd.setCursor(10, 88);
        M5.Lcd.print("Ohm carrier        off");
        M5.Lcd.setCursor(10, 102);
        M5.Lcd.print("Envelope           off");
        M5.Lcd.setCursor(10, 116);
        M5.Lcd.printf("PWM carrier   %5d Hz  silent", PWM_CARRIER_HZ);
    }

    // Live signal meters (right panel)
    M5.Lcd.fillRect(206, 52, 110, 78, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(212, 58);
    M5.Lcd.print("LIVE SIGNAL");
    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(212, 74);
    M5.Lcd.printf("Duty  %3d%%", dutyPct);
    drawBar(212, 88, 92, 10, liveDuty / 255.0f, COL_BAR_FILL,
            mode == MODE_WAVE ? liveEnvelope : -1.0f);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(212, 104);
    M5.Lcd.print("Ohm");
    drawBar(236, 104, 68, 8, (liveOhm * 0.5f) + 0.5f, COL_ACCENT);
    M5.Lcd.setCursor(212, 118);
    M5.Lcd.print("Rip");
    drawBar(236, 118, 68, 8, (liveRipple * 0.5f) + 0.5f, COL_OK);

    // Power / analytics row
    M5.Lcd.fillRect(4, 134, 312, 86, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.print("ANALYTICS");

    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(10, 156);
    M5.Lcd.printf("Power limit   %3d%%", powerPct);
    drawBar(120, 156, 180, 10, powerPct / 100.0f, COL_ACCENT);

    M5.Lcd.setCursor(10, 174);
    M5.Lcd.printf("Breath cycles %4lu", (unsigned long)breathCycles);
    M5.Lcd.setCursor(160, 174);
    M5.Lcd.printf("Square pulses %6lu", (unsigned long)pulseCount);

    M5.Lcd.setCursor(10, 192);
    M5.Lcd.printf("Boot uptime   %s", bootUp);
    M5.Lcd.setCursor(160, 192);
    M5.Lcd.printf("Run time  %s", runUp);

    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 208);
    M5.Lcd.printf("Env phase %.0f%%   pin G8/G9   max duty cap %.0f%%",
                  liveEnvelope * 100.0f, WAVE_MAX_FRACTION * 100.0f);
}

void markRunning(bool on) {
    uint32_t now = millis();
    if (on && !running) {
        runSessionStartMs = now;
        running = true;
    } else if (!on && running) {
        runAccumMs += now - runSessionStartMs;
        running = false;
        applyMotorState(false);
        liveOhm = 0.0f;
        liveRipple = 0.0f;
        liveEnvelope = 0.0f;
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    pinMode(MOTOR_PIN2, OUTPUT);
    digitalWrite(MOTOR_PIN2, LOW);
    ledcSetup(PWM_CHANNEL, PWM_CARRIER_HZ, PWM_RES_BITS);
    ledcAttachPin(MOTOR_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Serial.begin(115200);
    bootMs = millis();
    runSessionStartMs = bootMs;
    waveStartUs = micros();
    drawDashboard(true);

    lastToggle = micros();
    lastTelemetry = millis();
    lastUiMs = millis();
    Serial.println(
        "{\"status\":\"BOOT\",\"pin\":8,\"ohm_hz\":136.1,\"ripple_hz\":7.83,"
        "\"envelope_hz\":0.1,\"mode\":\"WAVE\"}");
}

void loop() {
    M5.update();

    // --- Serial control ---
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();
        if (cmd == "STOP") {
            markRunning(false);
            drawDashboard(true);
        } else if (cmd == "START") {
            markRunning(true);
            lastToggle = micros();
            waveStartUs = micros();
            drawDashboard(true);
        } else if (cmd == "MODE SQUARE") {
            mode = MODE_SQUARE;
            lastToggle = micros();
            drawDashboard(true);
        } else if (cmd == "MODE WAVE") {
            mode = MODE_WAVE;
            waveStartUs = micros();
            lastWaveUpdateUs = 0;
            drawDashboard(true);
        } else if (cmd.startsWith("POWER ")) {
            int pct = cmd.substring(6).toInt();
            pct = constrain(pct, 0, 100);
            powerLevel = (uint8_t)(pct * 255 / 100);
            drawDashboard(true);
        }
    }

    // --- Touch screen to toggle on/off ---
    if (M5.Touch.getCount()) {
        auto t = M5.Touch.getDetail(0);
        if (t.wasPressed()) {
            if (running) {
                markRunning(false);
            } else {
                markRunning(true);
                lastToggle = micros();
                waveStartUs = micros();
            }
            drawDashboard(true);
        }
    }

    // --- Drive the motor ---
    if (running) {
        if (mode == MODE_SQUARE) {
            uint32_t now = micros();
            if (now - lastToggle >= HALF_PERIOD_US) {
                lastToggle = now;
                applyMotorState(!motorOn);
                if (motorOn) pulseCount++;
                liveOhm = 0.0f;
                liveRipple = motorOn ? 1.0f : -1.0f;
                liveEnvelope = 1.0f;
            }
        } else {
            uint32_t now = micros();
            if (now - lastWaveUpdateUs >= WAVE_UPDATE_INTERVAL_US) {
                lastWaveUpdateUs = now;
                float t = (now - waveStartUs) / 1000000.0f;
                float ohm = sinf(2.0f * PI * (float)OHM_HZ * t);
                float ripple = sinf(2.0f * PI * (float)PULSE_HZ * t);
                float envelope = sinf(2.0f * PI * (float)WAVE_ENVELOPE_HZ * t);
                float composite = (ohm + ripple) * 0.5f;
                float normPower = composite * 0.5f + 0.5f;
                float normEnvelope = envelope * 0.5f + 0.5f;
                float duty = normPower * normEnvelope * WAVE_MAX_FRACTION *
                             (powerLevel / 255.0f) * 255.0f;
                liveDuty = (uint8_t)duty;
                liveOhm = ohm;
                liveRipple = ripple;
                liveEnvelope = normEnvelope;
                ledcWrite(PWM_CHANNEL, liveDuty);
                motorOn = liveDuty > 0;
                uint32_t cyclesNow = (uint32_t)(t * WAVE_ENVELOPE_HZ);
                if (cyclesNow > breathCycles) breathCycles = cyclesNow;
            }
        }
    }

    // --- Live UI refresh (~8 Hz) + 1 Hz JSON telemetry ---
    uint32_t nowMs = millis();
    if (nowMs - lastUiMs >= 125) {
        lastUiMs = nowMs;
        drawDashboard(false);
    }
    if (nowMs - lastTelemetry >= 1000) {
        lastTelemetry = nowMs;
        char bootUp[16];
        char runUp[16];
        formatUptime(bootUp, sizeof(bootUp), (nowMs - bootMs) / 1000UL);
        formatUptime(runUp, sizeof(runUp), currentRunMs(nowMs) / 1000UL);
        if (mode == MODE_SQUARE) {
            Serial.printf(
                "{\"status\":\"%s\",\"mode\":\"SQUARE\",\"freq_hz\":7.83,"
                "\"power_pct\":%d,\"duty_pct\":%d,\"pulses\":%lu,"
                "\"boot_uptime\":\"%s\",\"run_uptime\":\"%s\","
                "\"uptime_s\":%lu}\n",
                running ? "PULSING" : "STOPPED", (int)(powerLevel * 100 / 255),
                (int)(liveDuty * 100 / 255), (unsigned long)pulseCount, bootUp,
                runUp, (unsigned long)((nowMs - bootMs) / 1000UL));
        } else {
            Serial.printf(
                "{\"status\":\"%s\",\"mode\":\"WAVE\",\"ohm_hz\":136.1,"
                "\"ripple_hz\":7.83,\"envelope_hz\":0.1,\"power_pct\":%d,"
                "\"duty_pct\":%d,\"envelope_pct\":%.0f,\"breath_cycles\":%lu,"
                "\"boot_uptime\":\"%s\",\"run_uptime\":\"%s\","
                "\"uptime_s\":%lu}\n",
                running ? "PULSING" : "STOPPED", (int)(powerLevel * 100 / 255),
                (int)(liveDuty * 100 / 255), liveEnvelope * 100.0f,
                (unsigned long)breathCycles, bootUp, runUp,
                (unsigned long)((nowMs - bootMs) / 1000UL));
        }
    }
}
