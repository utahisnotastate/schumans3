#include <Arduino.h>
#include <M5Unified.h>
#include <math.h>

// ---------------------------------------------------------------------------
// CoreS3 + Port B DC motor haptic pulse device.
//
// Two modes:
//  - SQUARE: on/off toggle at 7.83 Hz, 50% duty. A steady buzz.
//  - WAVE:   smooth PWM breathing-pace pattern - a 7.83 Hz ripple whose
//            intensity swells and fades on a 0.1 Hz (6-per-minute) envelope.
//            6 cycles/min paced breathing is a real, evidence-informed
//            relaxation technique used in HRV biofeedback practice. This
//            firmware borrows that *timing* honestly.
//
// What this does NOT do, in either mode: "broadcast" or recreate the actual
// Schumann resonance (a planetary-scale ionospheric phenomenon), stimulate
// the vagus nerve, or "force" heart-rate-variability coherence. There is no
// mainstream scientific mechanism by which a motor vibrating against skin
// does that. Treat this as a rhythmic haptic novelty / breathing-pace cue,
// not a medical or nerve-stimulation device. If you're using this to manage
// something more serious than everyday stress, please talk to an actual
// clinician - this is not a substitute for that.
// ---------------------------------------------------------------------------

static const int MOTOR_PIN = 8;             // Port B pin 1 (G8) on CoreS3
static const int MOTOR_PIN2 = 9;            // Port B pin 2 (G9) - held low; wire here too if your driver needs a DIR/second input
static const double PULSE_HZ = 7.83;
static const uint32_t HALF_PERIOD_US =
    (uint32_t)((1000000.0 / PULSE_HZ) / 2.0);  // ~63857 us

// PWM drive instead of a flat digitalWrite HIGH:
//  - Intensity is adjustable (POWER command) instead of fixed at 100%,
//    which matters for a device meant to run continuously - full drive
//    indefinitely draws more current and runs the motor/driver hotter
//    than needed.
//  - The carrier is ultrasonic (20 kHz, above hearing range) so raising
//    intensity doesn't add an audible whine on top of the pulse itself.
static const int PWM_RES_BITS = 8;          // 0-255 duty range
static const int PWM_CARRIER_HZ = 20000;    // inaudible
static const int PWM_CHANNEL = 0;           // older esp32-arduino core: channel-based LEDC API
uint8_t powerLevel = 255;                   // 0-255; starts at max per your stated priority

enum PulseMode : uint8_t { MODE_SQUARE = 0, MODE_WAVE = 1 };
PulseMode mode = MODE_WAVE;

// WAVE mode timing
static const double WAVE_ENVELOPE_HZ = 0.1;      // 6 cycles/min paced-breathing envelope
static const float WAVE_MAX_FRACTION = 0.8f;     // cap duty to avoid harsh mechanical bottoming-out
static const uint32_t WAVE_UPDATE_INTERVAL_US = 10000;  // 100 Hz duty updates -> smooth
uint32_t waveStartUs = 0;
uint32_t lastWaveUpdateUs = 0;
uint32_t breathCycles = 0;

bool motorOn = false;
bool running = true;
uint32_t lastToggle = 0;
uint32_t pulseCount = 0;
uint32_t lastTelemetry = 0;

void applyMotorState(bool on) {
    motorOn = on;
    ledcWrite(PWM_CHANNEL, on ? powerLevel : 0);
}

void drawStatus() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println(running ? "MOTOR: ON" : "MOTOR: OFF");
    if (mode == MODE_SQUARE) {
        M5.Lcd.printf("SQUARE %.2f Hz\n", PULSE_HZ);
    } else {
        M5.Lcd.println("WAVE mode:");
        M5.Lcd.printf("%.2fHz in 6cpm swell\n", PULSE_HZ);
    }
    M5.Lcd.printf("Power: %d%%\n", (int)(powerLevel * 100 / 255));
    M5.Lcd.println("Driving Port B (G8)");
    M5.Lcd.println();
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Serial: START STOP");
    M5.Lcd.println("POWER <0-100>");
    M5.Lcd.println("MODE SQUARE / MODE WAVE");
    M5.Lcd.println();
    M5.Lcd.println("Haptic novelty device.");
    M5.Lcd.println("No nerve/health claims.");
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
    waveStartUs = micros();
    drawStatus();

    lastToggle = micros();
    lastTelemetry = millis();
    Serial.println("{\"status\":\"BOOT\",\"pin\":8,\"freq_hz\":7.83,\"mode\":\"WAVE\"}");
}

void loop() {
    M5.update();

    // --- Serial control ---
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();
        if (cmd == "STOP") {
            running = false;
            applyMotorState(false);
            drawStatus();
        } else if (cmd == "START") {
            running = true;
            lastToggle = micros();
            waveStartUs = micros();
            drawStatus();
        } else if (cmd == "MODE SQUARE") {
            mode = MODE_SQUARE;
            lastToggle = micros();
            drawStatus();
        } else if (cmd == "MODE WAVE") {
            mode = MODE_WAVE;
            waveStartUs = micros();
            lastWaveUpdateUs = 0;
            drawStatus();
        } else if (cmd.startsWith("POWER ")) {
            int pct = cmd.substring(6).toInt();
            pct = constrain(pct, 0, 100);
            powerLevel = (uint8_t)(pct * 255 / 100);
            drawStatus();
        }
    }

    // --- Touch screen to toggle on/off ---
    if (M5.Touch.getCount()) {
        auto t = M5.Touch.getDetail(0);
        if (t.wasPressed()) {
            running = !running;
            if (!running) {
                applyMotorState(false);
            } else {
                lastToggle = micros();
                waveStartUs = micros();
            }
            drawStatus();
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
            }
        } else {  // MODE_WAVE
            uint32_t now = micros();
            if (now - lastWaveUpdateUs >= WAVE_UPDATE_INTERVAL_US) {
                lastWaveUpdateUs = now;
                float t = (now - waveStartUs) / 1000000.0f;
                float ripple = sinf(2.0f * PI * (float)PULSE_HZ * t);
                float envelope = sinf(2.0f * PI * (float)WAVE_ENVELOPE_HZ * t);
                float normRipple = ripple * 0.5f + 0.5f;
                float normEnvelope = envelope * 0.5f + 0.5f;
                float duty = normRipple * normEnvelope * WAVE_MAX_FRACTION *
                             (powerLevel / 255.0f) * 255.0f;
                ledcWrite(PWM_CHANNEL, (uint8_t)duty);
                motorOn = duty > 0.0f;
                uint32_t cyclesNow = (uint32_t)(t * WAVE_ENVELOPE_HZ);
                if (cyclesNow > breathCycles) breathCycles = cyclesNow;
            }
        }
    }

    // --- 1 Hz JSON telemetry over USB serial ---
    uint32_t nowMs = millis();
    if (nowMs - lastTelemetry >= 1000) {
        lastTelemetry = nowMs;
        if (mode == MODE_SQUARE) {
            Serial.printf(
                "{\"status\":\"%s\",\"mode\":\"SQUARE\",\"freq_hz\":7.83,"
                "\"power_pct\":%d,\"pulses\":%lu,\"uptime_s\":%lu}\n",
                running ? "PULSING" : "STOPPED", (int)(powerLevel * 100 / 255),
                (unsigned long)pulseCount, (unsigned long)(nowMs / 1000));
        } else {
            Serial.printf(
                "{\"status\":\"%s\",\"mode\":\"WAVE\",\"carrier_hz\":7.83,"
                "\"envelope_hz\":0.1,\"power_pct\":%d,\"breath_cycles\":%lu,"
                "\"uptime_s\":%lu}\n",
                running ? "PULSING" : "STOPPED", (int)(powerLevel * 100 / 255),
                (unsigned long)breathCycles, (unsigned long)(nowMs / 1000));
        }
    }
}
