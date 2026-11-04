#include <Arduino.h>
#include <JAVI_inferencing.h>
#include <PDM.h>
#include <cstring>

// Configuracion.
static constexpr float UMBRAL_JAVI = 0.90f;
static constexpr unsigned long TIMEOUT_AUDIO_MS = 5000;

// Captura de audio.
static int16_t audio[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static volatile bool grabando = false;
static volatile size_t posicion = 0;

// Estado del LED y control de detecciones repetidas.
static bool ledEncendido = false;
static bool javiDetectadoAntes = false;


// ---------- LED ----------

void inicializarLed()
{
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(LED_BUILTIN, OUTPUT);
    ledEncendido = false;
}

void alternarLed()
{
    ledEncendido = !ledEncendido;

    digitalWrite(LED_BUILTIN, ledEncendido ? HIGH : LOW);

    Serial.println(
        ledEncendido ? "JAVI -> LED ENCENDIDO"
                     : "JAVI -> LED APAGADO"
    );
}


// ---------- Comunicacion ----------

void inicializarSerial()
{
    Serial.begin(115200);

    unsigned long inicio = millis();

    while (!Serial && millis() - inicio < 5000) {
        delay(10);
    }
}


// ---------- Microfono ----------

// Callback: recibe los datos del microfono.
void recibirAudio()
{
    int16_t bloque[256];

    while (PDM.available() > 0) {
        int bytes = PDM.available();

        if (bytes > static_cast<int>(sizeof(bloque))) {
            bytes = sizeof(bloque);
        }

        int leidos = PDM.read(bloque, bytes);

        if (leidos <= 0) {
            break;
        }

        int muestras = leidos / sizeof(int16_t);

        for (int i = 0; i < muestras; i++) {
            if (!grabando) {
                continue;
            }

            audio[posicion++] = bloque[i];

            if (posicion >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
                grabando = false;
            }
        }
    }
}

void inicializarMicrofono()
{
    PDM.onReceive(recibirAudio);
    PDM.setBufferSize(512);

    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        Serial.println("ERROR: no se pudo iniciar el microfono.");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Microfono preparado. LED apagado.");
    Serial.println("Di 'javi' para cambiar el estado del LED.");
}

bool grabarAudio()
{
    Serial.println("\nEscuchando...");

    noInterrupts();
    posicion = 0;
    grabando = true;
    interrupts();

    unsigned long inicio = millis();

    while (grabando) {
        if (millis() - inicio > TIMEOUT_AUDIO_MS) {
            grabando = false;
            Serial.println("ERROR: no llega suficiente audio.");
            delay(1000);
            return false;
        }

        delay(1);
    }

    return true;
}


// ---------- Clasificacion ----------

// Callback: entrega el audio grabado a Edge Impulse.
int obtenerAudio(size_t offset, size_t length, float *salida)
{
    for (size_t i = 0; i < length; i++) {
        salida[i] = static_cast<float>(audio[offset + i]);
    }

    return 0;
}

bool clasificarAudio(ei_impulse_result_t &resultado)
{
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = obtenerAudio;

    EI_IMPULSE_ERROR error =
        run_classifier(&signal, &resultado, false);

    if (error != EI_IMPULSE_OK) {
        Serial.print("ERROR del clasificador: ");
        Serial.println(static_cast<int>(error));
        delay(1000);
        return false;
    }

    return true;
}

float obtenerConfianzaJavi(const ei_impulse_result_t &resultado)
{
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (strcmp(resultado.classification[i].label, "javi") == 0) {
            return resultado.classification[i].value;
        }
    }

    return 0.0f;
}

void mostrarResultados(const ei_impulse_result_t &resultado)
{
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.print(resultado.classification[i].label);
        Serial.print(": ");
        Serial.print(resultado.classification[i].value * 100.0f, 1);
        Serial.println(" %");
    }
}

void gestionarDeteccion(const ei_impulse_result_t &resultado)
{
    float confianza = obtenerConfianzaJavi(resultado);
    bool javiDetectadoAhora = confianza > UMBRAL_JAVI;

    // Evita alternar varias veces por detecciones consecutivas.
    if (javiDetectadoAhora && !javiDetectadoAntes) {
        alternarLed();
    }

    javiDetectadoAntes = javiDetectadoAhora;
}


// ---------- Programa principal ----------

void setup()
{
    inicializarLed();
    inicializarSerial();
    inicializarMicrofono();
}

void loop()
{
    if (!grabarAudio()) {
        return;
    }

    ei_impulse_result_t resultado = {};

    if (!clasificarAudio(resultado)) {
        return;
    }

    mostrarResultados(resultado);
    gestionarDeteccion(resultado);
}