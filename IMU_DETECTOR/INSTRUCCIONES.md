# IMU detector: PlatformIO + Edge Impulse

Abrir esta carpeta IMU_DETECTOR desde PlatformIO (Open Project).
Placa: Arduino Nano 33 BLE Sense original, con IMU LSM9DS1. La Sense Rev2 utiliza otra IMU y requiere adaptar la libreria.

## Archivos del proyecto

- platformio.ini: placa, framework Arduino y dependencias.
- src/main.cpp: lectura de IMU y ejecucion de la red; imprime las probabilidades por USB.
- include/config.h: puerto serie y conversion de aceleracion.
- IMU_TEST_inferencing/: red exportada de Edge Impulse, enlazada como libreria local sin moverla ni modificarla. Contiene pesos, procesamiento de senal, SDK y ejemplos.
- herramientas/firmware-adquisicion/: firmware binario y scripts de carga para conectar la placa con Edge Impulse y recoger datos.
- herramientas/firmware-fuentes/: fuentes de ese firmware de adquisicion. No forman parte de esta compilacion.

## Compilar, cargar y ver resultados

Desde una terminal con PlatformIO disponible:

```powershell
pio run
pio run --target upload
pio device monitor --baud 115200
```

Tambien puedes usar los botones Build, Upload y Monitor de PlatformIO en VS Code.
Si el puerto no aparece, pulsa RESET dos veces y vuelve a cargar. Cierra el monitor serie y el daemon/data forwarder antes de cargar para liberar el puerto.

## Entrada de la red actual

El modelo requiere 200 muestras de 9 ejes a 100 Hz (2 segundos):
accX, accY, accZ, gyrX, gyrY, gyrZ, magX, magY, magZ.
Aceleracion en m/s2, limitada primero a +/-2 g; giroscopio en grados/s; magnetometro en uT, siguiendo el ejemplo de fusion exportado.
Se conserva la ultima lectura disponible de cada sensor: el magnetometro no entrega datos nuevos a 100 Hz.
Comprueba que estas unidades, limites y montaje coinciden con los usados al recoger tus datos.

La plantilla captura una ventana y despues ejecuta la red. Hay una pausa de adquisicion durante la inferencia; no implementa reconocimiento continuo sin huecos.
Las salidas son puntuaciones por clase; todavia no se aplican umbrales ni acciones.

Para actualizar el modelo, sustituye la carpeta IMU_TEST_inferencing con otra exportacion del mismo nombre. Si cambian los ejes o su orden, adapta main.cpp. La frecuencia y longitud de ventana se toman de los metadatos del modelo.
Cargar este proyecto sustituye en la placa el firmware de recogida de datos por la aplicacion de inferencia.
