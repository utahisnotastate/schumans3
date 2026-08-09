#include <Arduino.h>
#include <M5Unified.h>
#include <M5UnitENV.h>
#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// CoreS3 + Port B DC motor haptic pulse device.
//
// Architecture:
//  - Core 1: FreeRTOS wave task with vTaskDelayUntil (1 ms synthesis).
//            LEDC hardware owns the 20 kHz silent PWM carrier.
//  - Core 0: touch, serial, dashboard, optional ENV III (Port A) polling.
//
// Optional ENV III (Port A / red Grove): SHT30 + QMP6988. When present,
// ambient mode micro-adjusts the Ohm carrier from barometric pressure
// (and lightly from temperature) within 134–138 Hz. This is novelty
// "desk ambient" tuning — not atmospheric impedance matching, not a
// room-scale EM/acoustic therapy bubble, and not required to run.
//
// Modes: SQUARE (7.83 Hz) / WAVE (Ohm + ripple + 0.1 Hz envelope).
// Standalone: auto-starts WAVE with or without a PC / ENV III attached.
// ---------------------------------------------------------------------------

static const int MOTOR_PIN = 8;             // Port B G8
static const int MOTOR_PIN2 = 9;            // Port B G9 held low
static const int PORTA_SDA = 2;             // CoreS3 Port A
static const int PORTA_SCL = 1;

static const float PULSE_HZ = 7.83f;
static const float OHM_HZ_BASE = 136.1f;
static const float OHM_HZ_MIN = 134.0f;
static const float OHM_HZ_MAX = 138.0f;
static const float WAVE_ENVELOPE_HZ = 0.1f;
static const float WAVE_MAX_FRACTION = 0.85f;
static const float SEA_LEVEL_PA = 101325.0f;

static const int PWM_RES_BITS = 10;
static const int PWM_MAX_DUTY = (1 << PWM_RES_BITS) - 1;
static const int PWM_CARRIER_HZ = 20000;
static const int PWM_CHANNEL = 0;

static const TickType_t WAVE_PERIOD_TICKS = pdMS_TO_TICKS(1);
static const uint32_t SQUARE_HALF_PERIOD_US =
    (uint32_t)((1000000.0f / PULSE_HZ) / 2.0f);
static const uint32_t ENV_POLL_MS = 2000;

enum PulseMode : uint8_t { MODE_SQUARE = 0, MODE_WAVE = 1 };

// --- Shared control / live meters ---
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_running = true;
volatile PulseMode g_mode = MODE_WAVE;
volatile uint16_t g_powerLevel = PWM_MAX_DUTY;
volatile bool g_restartWave = true;
volatile float g_ohmHz = OHM_HZ_BASE;
volatile bool g_ambientTune = true;  // used only when ENV III is present

volatile bool g_motorOn = false;
volatile uint16_t g_liveDuty = 0;
volatile float g_liveEnvelope = 0.0f;
volatile float g_liveOhm = 0.0f;
volatile float g_liveRipple = 0.0f;
volatile uint32_t g_breathCycles = 0;
volatile uint32_t g_pulseCount = 0;
volatile uint32_t g_waveCore = 0;

// ENV III snapshot (Core 0 only writes)
bool g_envPresent = false;
bool g_envPressureOk = false;
bool g_envTempOk = false;
float g_pressureHpa = 0.0f;
float g_tempC = 0.0f;
float g_humidity = 0.0f;
float g_altitudeM = 0.0f;

SHT3X g_sht30;
QMP6988 g_qmp6988;
TaskHandle_t g_waveTask = nullptr;

uint32_t lastTelemetry = 0;
uint32_t lastUiMs = 0;
uint32_t lastEnvMs = 0;
uint32_t bootMs = 0;
uint32_t runSessionStartMs = 0;
uint32_t runAccumMs = 0;

static const int SCREEN_W = 320;
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_PANEL = 0x1082;
static const uint16_t COL_ACCENT = 0x07FF;
static const uint16_t COL_OK = 0x07E0;
static const uint16_t COL_STOP = 0xF800;
static const uint16_t COL_MUTED = 0x8410;
static const uint16_t COL_TEXT = TFT_WHITE;
static const uint16_t COL_BAR_BG = 0x2104;
static const uint16_t COL_BAR_FILL = 0x05FF;
static const uint16_t COL_BAR_ENV = 0xFFE0;

static inline uint16_t snapU16(volatile uint16_t *p) {
    portENTER_CRITICAL(&g_mux);
    uint16_t v = *p;
    portEXIT_CRITICAL(&g_mux);
    return v;
}

static inline float snapF(volatile float *p) {
    portENTER_CRITICAL(&g_mux);
    float v = *p;
    portEXIT_CRITICAL(&g_mux);
    return v;
}

static inline void setMotorDuty(uint16_t duty) {
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;
    ledcWrite(PWM_CHANNEL, duty);
    g_liveDuty = duty;
    g_motorOn = duty > 0;
}

void formatUptime(char *buf, size_t n, uint32_t sec) {
    uint32_t h = sec / 3600UL;
    uint32_t m = (sec % 3600UL) / 60UL;
    uint32_t s = sec % 60UL;
    snprintf(buf, n, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m,
             (unsigned long)s);
}

uint32_t currentRunMs(uint32_t nowMs) {
    if (!g_running) return runAccumMs;
    return runAccumMs + (nowMs - runSessionStartMs);
}

float clampOhm(float hz) {
    if (hz < OHM_HZ_MIN) return OHM_HZ_MIN;
    if (hz > OHM_HZ_MAX) return OHM_HZ_MAX;
    return hz;
}

// Novelty ambient detune from pressure (+ light temp). Not physics therapy.
float computeAmbientOhm(float pressurePa, float tempC, bool haveTemp) {
    float pressureVariance = (SEA_LEVEL_PA - pressurePa) / 1000.0f;
    float tempVariance = haveTemp ? ((25.0f - tempC) / 10.0f) : 0.0f;
    return clampOhm(OHM_HZ_BASE + pressureVariance * 0.05f +
                    tempVariance * 0.02f);
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
    const bool running = g_running;
    const bool motorOn = g_motorOn;
    const PulseMode mode = g_mode;
    const uint16_t powerLevel = snapU16(&g_powerLevel);
    const uint16_t liveDuty = snapU16(&g_liveDuty);
    const float liveEnvelope = g_liveEnvelope;
    const float liveOhm = g_liveOhm;
    const float liveRipple = g_liveRipple;
    const float ohmHz = snapF(&g_ohmHz);
    const uint32_t breathCycles = g_breathCycles;
    const uint32_t pulseCount = g_pulseCount;
    const uint32_t waveCore = g_waveCore;
    const bool ambient = g_envPresent && g_ambientTune;

    const int powerPct = (int)(powerLevel * 100UL / PWM_MAX_DUTY);
    const int dutyPct = (int)(liveDuty * 100UL / PWM_MAX_DUTY);
    char bootUp[16];
    char runUp[16];
    formatUptime(bootUp, sizeof(bootUp), (nowMs - bootMs) / 1000UL);
    formatUptime(runUp, sizeof(runUp), currentRunMs(nowMs) / 1000UL);

    if (fullRedraw) {
        M5.Lcd.fillScreen(COL_BG);
        M5.Lcd.fillRect(0, 0, SCREEN_W, 28, COL_PANEL);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(COL_ACCENT, COL_PANEL);
        M5.Lcd.setCursor(6, 6);
        M5.Lcd.print("SCHUMAN S3");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(COL_MUTED, COL_BG);
        M5.Lcd.setCursor(6, 226);
        M5.Lcd.print("Tap start/stop  Serial: START STOP POWER MODE AMBIENT");
    }

    M5.Lcd.fillRoundRect(200, 4, 114, 20, 4,
                         running ? (motorOn ? COL_OK : COL_ACCENT) : COL_STOP);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_BLACK,
                         running ? (motorOn ? COL_OK : COL_ACCENT) : COL_STOP);
    M5.Lcd.setCursor(210, 10);
    if (!running) {
        M5.Lcd.print("  STOPPED  ");
    } else if (mode == MODE_WAVE) {
        M5.Lcd.print(ambient ? "  AMBIENT  " : "  EMITTING ");
    } else {
        M5.Lcd.print(motorOn ? "  PULSING  " : "  IDLE GAP ");
    }

    M5.Lcd.fillRect(0, 30, SCREEN_W, 18, COL_BG);
    M5.Lcd.setTextColor(COL_TEXT, COL_BG);
    M5.Lcd.setCursor(6, 34);
    if (mode == MODE_SQUARE) {
        M5.Lcd.printf("MODE SQUARE  Core%lu  %.2f Hz @ Port B",
                      (unsigned long)waveCore, PULSE_HZ);
    } else {
        M5.Lcd.printf("MODE WAVE    Core%lu  Ohm %.2f Hz %s",
                      (unsigned long)waveCore, ohmHz,
                      ambient ? "(env-tuned)" : "(fixed)");
    }

    M5.Lcd.fillRect(4, 52, 198, 78, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 58);
    M5.Lcd.print("FREQUENCY MATRIX");
    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(10, 74);
    if (mode == MODE_WAVE) {
        M5.Lcd.printf("Ohm carrier   %6.2f Hz", ohmHz);
        M5.Lcd.setCursor(10, 88);
        M5.Lcd.printf("Ripple        %6.2f Hz", PULSE_HZ);
        M5.Lcd.setCursor(10, 102);
        M5.Lcd.printf("Envelope      %6.2f Hz  (6 cpm)", WAVE_ENVELOPE_HZ);
        M5.Lcd.setCursor(10, 116);
        M5.Lcd.printf("PWM carrier   %5d Hz  HW LEDC", PWM_CARRIER_HZ);
    } else {
        M5.Lcd.printf("Pulse         %6.2f Hz", PULSE_HZ);
        M5.Lcd.setCursor(10, 88);
        M5.Lcd.print("Ohm carrier        off");
        M5.Lcd.setCursor(10, 102);
        M5.Lcd.print("Envelope           off");
        M5.Lcd.setCursor(10, 116);
        M5.Lcd.printf("PWM carrier   %5d Hz  HW LEDC", PWM_CARRIER_HZ);
    }

    M5.Lcd.fillRect(206, 52, 110, 78, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(212, 58);
    M5.Lcd.print("LIVE SIGNAL");
    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(212, 74);
    M5.Lcd.printf("Duty  %3d%%", dutyPct);
    drawBar(212, 88, 92, 10, liveDuty / (float)PWM_MAX_DUTY, COL_BAR_FILL,
            mode == MODE_WAVE ? liveEnvelope : -1.0f);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(212, 104);
    M5.Lcd.print("Ohm");
    drawBar(236, 104, 68, 8, (liveOhm * 0.5f) + 0.5f, COL_ACCENT);
    M5.Lcd.setCursor(212, 118);
    M5.Lcd.print("Rip");
    drawBar(236, 118, 68, 8, (liveRipple * 0.5f) + 0.5f, COL_OK);

    M5.Lcd.fillRect(4, 134, 312, 86, COL_PANEL);
    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.print(g_envPresent ? "ANALYTICS + ENV III (Port A)" : "ANALYTICS");
    M5.Lcd.setTextColor(COL_TEXT, COL_PANEL);
    M5.Lcd.setCursor(10, 156);
    M5.Lcd.printf("Power %3d%%", powerPct);
    drawBar(78, 156, 100, 10, powerPct / 100.0f, COL_ACCENT);
    M5.Lcd.setCursor(188, 156);
    M5.Lcd.printf("Breath %lu  Puls %lu", (unsigned long)breathCycles,
                  (unsigned long)pulseCount);

    M5.Lcd.setCursor(10, 174);
    M5.Lcd.printf("Boot %s   Run %s", bootUp, runUp);

    M5.Lcd.setTextColor(COL_MUTED, COL_PANEL);
    M5.Lcd.setCursor(10, 192);
    if (g_envPresent) {
        M5.Lcd.printf("P %.1f hPa  T %.1fC  RH %.0f%%  alt %.0fm",
                      g_pressureHpa, g_tempC, g_humidity, g_altitudeM);
        M5.Lcd.setCursor(10, 208);
        M5.Lcd.printf("Ambient %s  Ohm->%.2f  desk OK without body contact",
                      g_ambientTune ? "ON " : "OFF", ohmHz);
    } else {
        M5.Lcd.print("ENV III: not connected (optional Port A)");
        M5.Lcd.setCursor(10, 208);
        M5.Lcd.printf("Fixed Ohm %.1f Hz  wearable or desk still works",
                      OHM_HZ_BASE);
    }
}

void markRunning(bool on) {
    uint32_t now = millis();
    if (on && !g_running) {
        runSessionStartMs = now;
        g_running = true;
        g_restartWave = true;
    } else if (!on && g_running) {
        runAccumMs += now - runSessionStartMs;
        g_running = false;
        setMotorDuty(0);
        g_liveOhm = 0.0f;
        g_liveRipple = 0.0f;
        g_liveEnvelope = 0.0f;
    }
}

void pollEnvIII() {
    if (!g_envPresent) return;

    bool gotP = g_qmp6988.update();
    bool gotT = g_sht30.update();
    g_envPressureOk = gotP;
    g_envTempOk = gotT;

    if (gotP) {
        g_pressureHpa = g_qmp6988.pressure / 100.0f;
        g_altitudeM = g_qmp6988.altitude;
    }
    if (gotT) {
        g_tempC = g_sht30.cTemp;
        g_humidity = g_sht30.humidity;
    }

    if (g_ambientTune && gotP) {
        g_ohmHz = computeAmbientOhm(g_qmp6988.pressure, g_tempC, gotT);
    } else if (!g_ambientTune) {
        g_ohmHz = OHM_HZ_BASE;
    }
}

void WaveformTask(void *pvParameters) {
    (void)pvParameters;
    g_waveCore = xPortGetCoreID();
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t waveStartUs = micros();
    uint32_t lastSquareToggleUs = micros();
    bool squareOn = false;

    for (;;) {
        if (g_restartWave) {
            g_restartWave = false;
            waveStartUs = micros();
            lastSquareToggleUs = waveStartUs;
            squareOn = false;
        }

        if (!g_running) {
            setMotorDuty(0);
            g_liveOhm = 0.0f;
            g_liveRipple = 0.0f;
            g_liveEnvelope = 0.0f;
            vTaskDelayUntil(&xLastWakeTime, WAVE_PERIOD_TICKS);
            continue;
        }

        const PulseMode mode = g_mode;
        const uint16_t powerLevel = snapU16(&g_powerLevel);
        const float ohmHz = snapF(&g_ohmHz);
        const uint32_t nowUs = micros();

        if (mode == MODE_SQUARE) {
            if (nowUs - lastSquareToggleUs >= SQUARE_HALF_PERIOD_US) {
                lastSquareToggleUs = nowUs;
                squareOn = !squareOn;
                if (squareOn) g_pulseCount++;
            }
            setMotorDuty(squareOn ? powerLevel : 0);
            g_liveOhm = 0.0f;
            g_liveRipple = squareOn ? 1.0f : -1.0f;
            g_liveEnvelope = 1.0f;
        } else {
            float t = (nowUs - waveStartUs) / 1000000.0f;
            float ohm = sinf(2.0f * PI * ohmHz * t);
            float ripple = sinf(2.0f * PI * PULSE_HZ * t);
            float envelope = sinf(2.0f * PI * WAVE_ENVELOPE_HZ * t);
            float composite = (ohm + ripple) * 0.5f;
            float normPower = composite * 0.5f + 0.5f;
            float normEnvelope = envelope * 0.5f + 0.5f;
            float dutyF = normPower * normEnvelope * WAVE_MAX_FRACTION *
                          (powerLevel / (float)PWM_MAX_DUTY) * PWM_MAX_DUTY;
            setMotorDuty((uint16_t)dutyF);
            g_liveOhm = ohm;
            g_liveRipple = ripple;
            g_liveEnvelope = normEnvelope;
            uint32_t cyclesNow = (uint32_t)(t * WAVE_ENVELOPE_HZ);
            if (cyclesNow > g_breathCycles) g_breathCycles = cyclesNow;
        }

        vTaskDelayUntil(&xLastWakeTime, WAVE_PERIOD_TICKS);
    }
}

bool initEnvIII() {
    // Port A I2C — do not hang if the unit is absent.
    bool pressure = g_qmp6988.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, PORTA_SDA,
                                    PORTA_SCL, 400000U);
    bool temp = g_sht30.begin(&Wire, SHT3X_I2C_ADDR, PORTA_SDA, PORTA_SCL,
                              400000U);
    if (pressure || temp) {
        g_envPresent = true;
        g_envPressureOk = pressure;
        g_envTempOk = temp;
        return true;
    }
    g_envPresent = false;
    g_ohmHz = OHM_HZ_BASE;
    return false;
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    pinMode(MOTOR_PIN2, OUTPUT);
    digitalWrite(MOTOR_PIN2, LOW);
    ledcSetup(PWM_CHANNEL, PWM_CARRIER_HZ, PWM_RES_BITS);
    ledcAttachPin(MOTOR_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Serial.begin(115200);
    delay(50);

    bool envOk = initEnvIII();
    if (envOk) {
        pollEnvIII();
        Serial.println(
            "{\"status\":\"BOOT\",\"env_iii\":true,\"port_a\":true,"
            "\"ambient_default\":true}");
    } else {
        Serial.println(
            "{\"status\":\"BOOT\",\"env_iii\":false,\"ohm_hz\":136.1,"
            "\"note\":\"ENV III optional on Port A\"}");
    }

    bootMs = millis();
    runSessionStartMs = bootMs;
    g_running = true;
    g_mode = MODE_WAVE;
    g_restartWave = true;
    lastEnvMs = millis();

    drawDashboard(true);
    lastTelemetry = millis();
    lastUiMs = millis();

    xTaskCreatePinnedToCore(WaveformTask, "WaveTask", 4096, nullptr, 2,
                            &g_waveTask, 1);

    Serial.printf(
        "{\"status\":\"READY\",\"mode\":\"WAVE\",\"ohm_hz\":%.2f,"
        "\"ripple_hz\":7.83,\"envelope_hz\":0.1,\"pwm_hz\":20000,"
        "\"pwm_bits\":10,\"env_iii\":%s,\"standalone\":true}\n",
        snapF(&g_ohmHz), g_envPresent ? "true" : "false");
}

void loop() {
    M5.update();

    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();
        if (cmd == "STOP") {
            markRunning(false);
            drawDashboard(true);
        } else if (cmd == "START") {
            markRunning(true);
            drawDashboard(true);
        } else if (cmd == "MODE SQUARE") {
            g_mode = MODE_SQUARE;
            g_restartWave = true;
            drawDashboard(true);
        } else if (cmd == "MODE WAVE") {
            g_mode = MODE_WAVE;
            g_restartWave = true;
            drawDashboard(true);
        } else if (cmd == "AMBIENT ON") {
            g_ambientTune = true;
            if (!g_envPresent) {
                Serial.println(
                    "{\"warn\":\"ENV III not connected\",\"ambient\":false}");
            }
            drawDashboard(true);
        } else if (cmd == "AMBIENT OFF") {
            g_ambientTune = false;
            g_ohmHz = OHM_HZ_BASE;
            drawDashboard(true);
        } else if (cmd.startsWith("POWER ")) {
            int pct = cmd.substring(6).toInt();
            pct = constrain(pct, 0, 100);
            g_powerLevel = (uint16_t)(pct * PWM_MAX_DUTY / 100);
            drawDashboard(true);
        }
    }

    if (M5.Touch.getCount()) {
        auto t = M5.Touch.getDetail(0);
        if (t.wasPressed()) {
            markRunning(!g_running);
            drawDashboard(true);
        }
    }

    uint32_t nowMs = millis();
    if (g_envPresent && (nowMs - lastEnvMs >= ENV_POLL_MS)) {
        lastEnvMs = nowMs;
        pollEnvIII();
    }

    if (nowMs - lastUiMs >= 125) {
        lastUiMs = nowMs;
        drawDashboard(false);
    }

    if (nowMs - lastTelemetry >= 1000) {
        lastTelemetry = nowMs;
        if (Serial) {
            char bootUp[16];
            char runUp[16];
            formatUptime(bootUp, sizeof(bootUp), (nowMs - bootMs) / 1000UL);
            formatUptime(runUp, sizeof(runUp), currentRunMs(nowMs) / 1000UL);
            const int powerPct =
                (int)(snapU16(&g_powerLevel) * 100UL / PWM_MAX_DUTY);
            const int dutyPct =
                (int)(snapU16(&g_liveDuty) * 100UL / PWM_MAX_DUTY);
            const float ohmHz = snapF(&g_ohmHz);

            if (g_mode == MODE_SQUARE) {
                Serial.printf(
                    "{\"status\":\"%s\",\"mode\":\"SQUARE\",\"freq_hz\":7.83,"
                    "\"power_pct\":%d,\"duty_pct\":%d,\"pulses\":%lu,"
                    "\"wave_core\":%lu,\"env_iii\":%s,\"boot_uptime\":\"%s\","
                    "\"run_uptime\":\"%s\",\"uptime_s\":%lu,"
                    "\"standalone\":true}\n",
                    g_running ? "PULSING" : "STOPPED", powerPct, dutyPct,
                    (unsigned long)g_pulseCount, (unsigned long)g_waveCore,
                    g_envPresent ? "true" : "false", bootUp, runUp,
                    (unsigned long)((nowMs - bootMs) / 1000UL));
            } else if (g_envPresent) {
                Serial.printf(
                    "{\"status\":\"%s\",\"mode\":\"WAVE\",\"ohm_hz\":%.2f,"
                    "\"ripple_hz\":7.83,\"envelope_hz\":0.1,\"power_pct\":%d,"
                    "\"duty_pct\":%d,\"envelope_pct\":%.0f,"
                    "\"breath_cycles\":%lu,\"wave_core\":%lu,"
                    "\"env_iii\":true,\"ambient\":%s,\"pressure_hPa\":%.2f,"
                    "\"temp_c\":%.2f,\"humidity_pct\":%.1f,"
                    "\"altitude_m\":%.1f,\"boot_uptime\":\"%s\","
                    "\"run_uptime\":\"%s\",\"uptime_s\":%lu,"
                    "\"standalone\":true}\n",
                    g_running ? "PULSING" : "STOPPED", ohmHz, powerPct, dutyPct,
                    g_liveEnvelope * 100.0f, (unsigned long)g_breathCycles,
                    (unsigned long)g_waveCore,
                    g_ambientTune ? "true" : "false", g_pressureHpa, g_tempC,
                    g_humidity, g_altitudeM, bootUp, runUp,
                    (unsigned long)((nowMs - bootMs) / 1000UL));
            } else {
                Serial.printf(
                    "{\"status\":\"%s\",\"mode\":\"WAVE\",\"ohm_hz\":%.2f,"
                    "\"ripple_hz\":7.83,\"envelope_hz\":0.1,\"power_pct\":%d,"
                    "\"duty_pct\":%d,\"envelope_pct\":%.0f,"
                    "\"breath_cycles\":%lu,\"wave_core\":%lu,"
                    "\"env_iii\":false,\"ambient\":false,"
                    "\"boot_uptime\":\"%s\",\"run_uptime\":\"%s\","
                    "\"uptime_s\":%lu,\"standalone\":true}\n",
                    g_running ? "PULSING" : "STOPPED", ohmHz, powerPct, dutyPct,
                    g_liveEnvelope * 100.0f, (unsigned long)g_breathCycles,
                    (unsigned long)g_waveCore, bootUp, runUp,
                    (unsigned long)((nowMs - bootMs) / 1000UL));
            }
        }
    }
}
