#include <Arduino.h>
#include <Arduino_LSM9DS1.h>
#include <IMU_TEST_inferencing.h>
#include <cstring>
#include "config.h"

static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 9,
              "Este programa requiere los 9 ejes de la IMU.");

namespace {
float frame[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
float sample[9] = {};
bool ready = false;
constexpr uint32_t intervalUs = static_cast<uint32_t>(EI_CLASSIFIER_INTERVAL_MS * 1000);

// El magnetometro actualiza mas despacio que el modelo: conservar su ultimo valor,
// como hace el ejemplo de sensor fusion incluido con la exportacion.
void readImu() {
    if (IMU.accelerationAvailable()) {
        float x, y, z;
        if (IMU.readAcceleration(x, y, z)) {
            const float acceleration[] = {x, y, z};
            for (size_t i = 0; i < 3; ++i) {
                sample[i] = constrain(acceleration[i], -Config::accelerationRangeG,
                                      Config::accelerationRangeG) * Config::gravity;
            }
        }
    }
    if (IMU.gyroscopeAvailable()) {
        IMU.readGyroscope(sample[3], sample[4], sample[5]); // grados/s
    }
    if (IMU.magneticFieldAvailable()) {
        IMU.readMagneticField(sample[6], sample[7], sample[8]); // uT
    }
}
}

void setup() {
    Serial.begin(Config::serialBaud);
    const uint32_t started = millis();
    while (!Serial && millis() - started < Config::serialWaitMs) {
        delay(10);
    }
    pinMode(LED_BUILTIN, OUTPUT);
    if (strcmp(EI_CLASSIFIER_FUSION_AXES_STRING,
               "accX + accY + accZ + gyrX + gyrY + gyrZ + magX + magY + magZ") != 0) {
        Serial.println("ERROR: el orden de ejes del modelo no coincide con el programa.");
        return;
    }
    if (!IMU.begin()) {
        Serial.println("ERROR: no se encuentra la LSM9DS1. Comprobar placa Sense original.");
        return;
    }
    // Esperar las primeras lecturas de los tres sensores (limite de 2 s).
    const uint32_t sensorStarted = millis();
    while (!(IMU.accelerationAvailable() && IMU.gyroscopeAvailable() &&
             IMU.magneticFieldAvailable())) {
        if (millis() - sensorStarted > 2000) {
            Serial.println("ERROR: tiempo de espera de la IMU agotado.");
            return;
        }
        delay(1);
    }
    readImu();
    ready = true;
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("IMU_TEST: inferencia con 9 ejes a 100 Hz. Ventanas de 2 s.");
}

void loop() {
    if (!ready) {
        delay(100);
        return;
    }
    uint32_t nextSample = micros();
    for (size_t offset = 0; offset < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; offset += 9) {
        while (static_cast<int32_t>(micros() - nextSample) < 0) {
            delayMicroseconds(50);
        }
        readImu();
        memcpy(frame + offset, sample, sizeof(sample));
        nextSample += intervalUs;
    }

    signal_t signal;
    int error = numpy::signal_from_buffer(frame, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (error != 0) {
        Serial.println("ERROR: no se pudo crear la ventana de entrada.");
        return;
    }
    ei_impulse_result_t result = {};
    error = run_classifier(&signal, &result, false);
    if (error != EI_IMPULSE_OK) {
        Serial.print("ERROR de inferencia: ");
        Serial.println(error);
        return;
    }
    Serial.println("--- Predicciones ---");
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
        Serial.print(result.classification[i].label);
        Serial.print(": ");
        Serial.println(result.classification[i].value, 4);
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    Serial.print("Anomalia: ");
    Serial.println(result.anomaly, 4);
#endif
    // La inferencia se ejecuta entre ventanas; esta plantilla no captura durante ella.
}
