# Monitor Calidad del Aire

Sistema embebido basado en ESP32 que mide temperatura, humedad y concentracion de material particulado (PM2.5 y PM10). Los datos se sirven en tiempo real a traves de un punto de acceso WiFi local mediante un dashboard web con graficos historicos e indicador de calidad del aire segun el estandar ICAP Chile. No requiere conexion a internet ni servidor externo.

---

## Indice

1. [Descripcion general](#descripcion-general)
2. [Hardware requerido](#hardware-requerido)
3. [Dependencias de software](#dependencias-de-software)
4. [Diagrama de conexiones](#diagrama-de-conexiones)
5. [Configuracion del proyecto](#configuracion-del-proyecto)
6. [Subir el codigo al ESP32](#subir-el-codigo-al-esp32)
7. [Uso del sistema](#uso-del-sistema)
8. [Endpoints HTTP](#endpoints-http)
9. [Indice de Calidad del Aire ICAP Chile](#indice-de-calidad-del-aire-icap-chile)
10. [Estructura del codigo](#estructura-del-codigo)
11. [Protocolo del sensor NovaPM (SDS011)](#protocolo-del-sensor-novapm-sds011)
12. [Limitaciones conocidas](#limitaciones-conocidas)
13. [Resolucion de problemas](#resolucion-de-problemas)

---

## Descripcion general

El firmware crea un Access Point WiFi con el nombre `SensorAire`. Al conectarse a esa red desde cualquier dispositivo (celular, tablet, PC) y abrir el navegador en `http://192.168.4.1`, se accede a un dashboard que muestra:

- Temperatura en grados Celsius (sensor DHT11)
- Humedad relativa en porcentaje (sensor DHT11)
- Concentracion de PM2.5 en microgramos por metro cubico (sensor NovaPM 5006)
- Concentracion de PM10 en microgramos por metro cubico (sensor NovaPM 5006)
- Indicador de calidad del aire con nivel ICAP y descripcion de riesgo sanitario

Los valores se actualizan automaticamente cada 2 segundos mediante peticiones AJAX al endpoint `/data`. La pagina mantiene un historial grafico de hasta 60 muestras (2 minutos) renderizado con canvas HTML5 puro, sin dependencias de librerias externas ni CDN.

---

## Hardware requerido

| Componente | Modelo | Cantidad |
|---|---|---|
| Microcontrolador | ESP32 XX | 1 |
| Sensor de temperatura y humedad | DHT11 | 1 |
| Sensor de material particulado | NovaPM 5006 (SDS011-compatible) | 1 |
| Cable USB | Con datos (no solo carga) | 1 |
| LED RGB | LED RGB | 1 |
| Fuente de alimentacion | 5 V / min. 500 mA | 1 |

### Notas sobre el hardware

**ESP32:** El firmware usa `Serial2` (pines GPIO 16 y GPIO 17 por defecto). Cualquier placa ESP32 con esos pines disponibles es compatible. Se ha probado con ESP32 Dev Module.

**DHT11:** Sensor de bajo costo con precision de +/-2 grados Celsius y rango de humedad del 20 al 80 %. Requiere resistencia pull-up externa de 10 kOhm entre el pin DATA y VCC si el modulo es el sensor suelto de 4 pines. Los breakouts de 3 pines ya incluyen la resistencia en la placa.

**NovaPM 5006:** Sensor laser de particulas en suspension. Utiliza comunicacion UART a 9600 baudios y emite tramas de 10 bytes en modo activo cada aproximadamente 1 segundo. Compatible con el protocolo SDS011 de Nova Fitness.

---

## Dependencias de software

### Arduino IDE

Version recomendada: 2.x o superior.

### Soporte para ESP32

1. Abrir **Archivo > Preferencias**.
2. En el campo "URLs adicionales de gestor de tarjetas" agregar:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

3. Ir a **Herramientas > Placa > Gestor de tarjetas**, buscar `esp32` e instalar el paquete de **Espressif Systems** (version 2.x o superior).

### Librerias

| Libreria | Autor | Como instalar |
|---|---|---|
| DHT sensor library | Adafruit | Administrador de bibliotecas: buscar "DHT sensor library" |
| Adafruit Unified Sensor | Adafruit | Se instala automaticamente como dependencia de la anterior |
| WiFi.h | Incluida en ESP32 Arduino Core | No requiere instalacion adicional |
| WebServer.h | Incluida en ESP32 Arduino Core | No requiere instalacion adicional |

---

## Diagrama de conexiones

### DHT11

```
ESP32 3V3  ----+---- VCC del DHT11
ESP32 GPIO 4 --+---- DATA del DHT11
ESP32 GND  --------- GND del DHT11
```

> Alimentar con 3.3 V. Si se alimenta con 5 V el nivel logico del pin DATA supera el maximo tolerado por los GPIO del ESP32 (3.3 V) y puede danarlo.

### NovaPM 5006

```
ESP32 VIN (5V) ---- VCC del NovaPM   (pin 1 / cable rojo)
ESP32 GND      ---- GND del NovaPM   (pin 2 / cable negro)
                    (pin 3 sin conexion)
ESP32 GPIO 16 o RX2  ---- TX del NovaPM    (pin 4 / cable verde)
ESP32 GPIO 17 o TX2 ---- RX del NovaPM    (pin 5 / cable azul)  <- opcional
```

> El sensor requiere 5 V para alimentar su ventilador interno y su laser. El pin VIN del ESP32 proporciona el voltaje USB sin pasar por el regulador interno.
>
> Aunque el sensor se alimenta con 5 V, la linea TX emite senales a nivel logico de 3.3 V, por lo que no es necesario un divisor de voltaje para conectarla al GPIO del ESP32.

### Resumen de pines

| Funcion | GPIO ESP32 |
|---|---|
| DHT11 DATA | 2 |
| NovaPM TX (entrada al ESP32) | 16 (RX2) |
| NovaPM RX (salida del ESP32) | 17 (TX2) |

---

## Configuracion del proyecto

Los parametros configurables se encuentran al inicio del archivo `main.ino`:

```cpp
// Pin y tipo de sensor de temperatura/humedad
#define DHT_PIN    4
#define DHT_TYPE   DHT11

// Pines UART para el sensor NovaPM
#define PM_RX_PIN  16
#define PM_TX_PIN  17

// Credenciales del Access Point WiFi
const char* AP_SSID = "SensorAire";
const char* AP_PASS = "12345678";

// Intervalo de lectura del DHT11 en milisegundos
const unsigned long DHT_INTERVAL = 2000;
```

Para cambiar el SSID o la contrasena del AP, modificar las constantes `AP_SSID` y `AP_PASS`. La contrasena debe tener un minimo de 8 caracteres.

---

## Subir el codigo al ESP32

1. Conectar el ESP32 al PC mediante cable USB con datos.
2. En Arduino IDE seleccionar la placa: **Herramientas > Placa > esp32 > ESP32 Dev Module**.
3. Seleccionar el puerto: **Herramientas > Puerto > COMx** (el numero varia segun el sistema).
4. Configurar la velocidad de subida: **Upload Speed: 921600**.
5. Hacer clic en el boton **Subir** (flecha derecha) o usar `Ctrl + U`.

### Si aparece "Connecting..." sin avanzar

Algunos modulos ESP32 no tienen circuito de reset automatico. En ese caso:

1. Hacer clic en **Subir**.
2. Cuando la consola muestre `Connecting......`, mantener presionado el boton **BOOT** (o **IO0**) del modulo.
3. Soltar el boton cuando comience la transferencia (`Writing at 0x00001000...`).

### Verificacion post-carga

Abrir **Herramientas > Monitor Serie** a **115200 baudios**. La salida esperada al arrancar es:

```
DHT11 OK -> T: 24.0C  H: 58.0%
AP creado. IP: 192.168.4.1
Servidor HTTP iniciado
```

Si el DHT muestra ERROR, revisar la seccion de resolucion de problemas.

---

## Uso del sistema

1. Encender el ESP32 (via USB o fuente externa).
2. Desde cualquier dispositivo con WiFi, buscar y conectarse a la red **SensorAire** con la contrasena **12345678**.
3. Abrir un navegador web y navegar a: `http://192.168.4.1`
4. El dashboard se carga y comienza a actualizar los datos cada 2 segundos automaticamente.

> Al estar conectado al AP del ESP32 no habra acceso a internet desde ese dispositivo. En moviles puede aparecer un aviso de "red sin internet"; ignorarlo y mantener la conexion.

---

## Endpoints HTTP

El servidor web expone dos rutas:

### `GET /`

Devuelve la pagina HTML del dashboard completa. Incluye estilos CSS y logica JavaScript embebidos. No depende de recursos externos.

**Respuesta:** `text/html`, codigo 200.

### `GET /data`

Devuelve la lectura actual de todos los sensores en formato JSON.

**Respuesta:** `application/json`, codigo 200.

```json
{
  "t": 24.0,
  "h": 58.0,
  "p2": 12.3,
  "p1": 18.7
}
```

| Campo | Descripcion | Unidad |
|---|---|---|
| `t` | Temperatura | grados Celsius |
| `h` | Humedad relativa | porcentaje |
| `p2` | Material particulado PM2.5 | microgramos/m3 |
| `p1` | Material particulado PM10 | microgramos/m3 |

---

## Indice de Calidad del Aire ICAP Chile

El dashboard incluye un indicador visual que clasifica la calidad del aire en tiempo real segun el **Indice de Calidad del Aire para Particulas (ICAP)** definido por el Ministerio del Medio Ambiente de Chile (DS N°59 MMA y actualizaciones SEREMI).

El indicador evalua simultaneamente PM2.5 y PM10, y muestra el nivel correspondiente al **contaminante en peor estado**. Cada nivel tiene fondo, borde y color de texto diferente para identificacion rapida.

### Niveles y umbrales

| Nivel | Color | PM2.5 (ug/m3) | PM10 (ug/m3) | Recomendacion sanitaria |
|---|---|---|---|---|
| Buena | Verde | 0 - 25 | 0 - 75 | Sin restricciones. Calidad satisfactoria para toda la poblacion. |
| Regular | Amarillo | 25.1 - 50 | 75.1 - 150 | Personas muy sensibles (asmaticos, adultos mayores) podrian presentar leve malestar. |
| Alerta | Naranja | 50.1 - 110 | 150.1 - 250 | Grupos sensibles pueden experimentar efectos. Reducir actividad fisica al aire libre. |
| Pre-emergencia | Rojo | 110.1 - 170 | 250.1 - 330 | Toda la poblacion puede verse afectada. Evitar exposicion exterior. Grupos de riesgo en interiores. |
| Emergencia | Morado | > 170 | > 330 | Alerta maxima sanitaria. Toda la poblacion debe permanecer en interiores y seguir instrucciones de la autoridad. |

### Informacion mostrada en el dashboard

Bajo el nombre del nivel, el indicador muestra en texto:

```
PM2.5: 12.3 ug/m3  |  PM10: 18.7 ug/m3  -  Calidad del aire satisfactoria...
```

### Logica de calculo (JavaScript)

La funcion `getICAP(pm25, pm10)` determina el nivel de cada contaminante por separado usando los umbrales de la tabla anterior, y retorna el nivel mas alto (peor estado) de los dos:

```javascript
function getICAP(pm25, pm10) {
  // Nivel por PM2.5
  if      (pm25 <= 25)  lvl25 = 0;   // Buena
  else if (pm25 <= 50)  lvl25 = 1;   // Regular
  else if (pm25 <= 110) lvl25 = 2;   // Alerta
  else if (pm25 <= 170) lvl25 = 3;   // Pre-emergencia
  else                  lvl25 = 4;   // Emergencia

  // Nivel por PM10
  if      (pm10 <= 75)  lvl10 = 0;
  else if (pm10 <= 150) lvl10 = 1;
  else if (pm10 <= 250) lvl10 = 2;
  else if (pm10 <= 330) lvl10 = 3;
  else                  lvl10 = 4;

  return ICAP[Math.max(lvl25, lvl10)];  // peor de los dos
}
```

La funcion `updateICAP(pm25, pm10)` aplica los estilos visuales (color de fondo, borde, punto indicador y texto) al elemento HTML `#aqiBox` en cada actualizacion de datos.

---

## Estructura del codigo

```
main.ino
|
+-- Includes y defines
|     Librerias, pines, credenciales WiFi
|
+-- Variables globales
|     Valores actuales de los sensores, temporizadores
|
+-- processPMByte(uint8_t b)
|     Parser no-bloqueante del protocolo SDS011.
|     Se llama en cada iteracion del loop con cada byte
|     disponible en Serial2. Valida cabecera, cola y checksum
|     antes de actualizar las variables globales de PM.
|
+-- HTML[] (PROGMEM)
|     Pagina web completa almacenada en flash.
|     Contiene CSS, HTML y JavaScript en un solo bloque.
|     El JavaScript implementa un graficador canvas propio
|     y realiza peticiones fetch a /data cada 2 segundos.
|
+-- handleRoot()
|     Manejador HTTP para GET /
|     Envia la pagina HTML desde PROGMEM.
|
+-- handleData()
|     Manejador HTTP para GET /data
|     Construye y envia el JSON con los valores actuales.
|
+-- setup()
|     Inicializa Serial (115200), Serial2 (9600, NovaPM),
|     DHT11, diagnostico de arranque, Access Point WiFi
|     y servidor HTTP.
|
+-- loop()
      Lee bytes de Serial2 (NovaPM, no bloqueante),
      atiende clientes HTTP y lee el DHT11 cada DHT_INTERVAL ms.
```

---

## Protocolo del sensor NovaPM (SDS011)

El sensor emite tramas de 10 bytes en modo activo a 9600 baudios, 8N1, aproximadamente una vez por segundo.

| Byte | Valor / Descripcion |
|---|---|
| 0 | `0xAA` — Cabecera |
| 1 | `0xC0` — Identificador de mensaje de datos |
| 2 | PM2.5 byte bajo |
| 3 | PM2.5 byte alto |
| 4 | PM10 byte bajo |
| 5 | PM10 byte alto |
| 6 | ID dispositivo byte bajo |
| 7 | ID dispositivo byte alto |
| 8 | Checksum (suma de bytes 2 al 7, modulo 256) |
| 9 | `0xAB` — Cola |

**Calculo del valor:**

```
PM2.5 (ug/m3) = (byte[3] * 256 + byte[2]) / 10.0
PM10  (ug/m3) = (byte[5] * 256 + byte[4]) / 10.0
```

El firmware valida cabecera (`0xAA`), identificador (`0xC0`), cola (`0xAB`) y checksum antes de actualizar los valores. Si alguna validacion falla, la trama se descarta silenciosamente.

---

## Limitaciones conocidas

- **Historial no persistente:** Los datos graficados se pierden al recargar la pagina o al reiniciar el ESP32. No hay almacenamiento en memoria flash ni tarjeta SD.
- **Un cliente a la vez:** El servidor HTTP es de un hilo. Multiples clientes simultaneos pueden causar lentitud en las respuestas.
- **DHT11:** La resolucion es de 1 grado Celsius y 1% de humedad. Para mayor precision se recomienda reemplazar por un DHT22 o SHT31, cambiando unicamente `DHT_TYPE` a `DHT22` en el codigo.
- **NovaPM tiempo de arranque:** El sensor tarda aproximadamente 10 segundos desde el encendido en estabilizar sus lecturas. Los primeros valores pueden no ser representativos.
- **Rango WiFi:** Al usar el ESP32 como AP en lugar de conectarse a un router, el alcance es limitado (tipicamente 10 a 20 metros en espacios abiertos).

---

## Resolucion de problemas

### DHT11 muestra ERROR en el Monitor Serie

- Verificar que el pin DATA este conectado a GPIO 2 o D2. 
- Verificar la resistencia pull-up de 10 kOhm entre DATA y VCC (si es sensor de 4 pines suelto).
- Verificar que la alimentacion sea de 3.3 V y no de 5 V.
- Reemplazar el sensor si el problema persiste.

### PM2.5 y PM10 muestran 0.0 permanentemente

- Verificar que el pin TX del sensor este conectado a GPIO 16 o RX2 del ESP32.
- Verificar que el pin RX del sensor este conectado a GPIO 17 o TX2 del ESP32.
- Verificar que el sensor este alimentado con 5 V (pin VIN del ESP32).
- Escuchar el ventilador interno del sensor al encender: si no gira, el problema es de alimentacion.
- Esperar al menos 15 segundos tras el arranque antes de descartar lecturas.

### La pagina web no carga o muestra "Error al obtener datos"

- Confirmar que el dispositivo esta conectado al AP `SensorAire` y no a otra red.
- En dispositivos moviles, ignorar el aviso de "red sin internet" y mantener la conexion WiFi activa.
- Verificar que la URL sea exactamente `http://192.168.4.1` (sin HTTPS).
- Reiniciar el ESP32 y volver a intentar.

### Error "Wrong boot mode detected" al subir

- Mantener presionado el boton BOOT del ESP32 cuando el IDE muestre `Connecting......`.
- Soltar el boton al iniciar la transferencia.

### No aparece el puerto COM en Arduino IDE

- Instalar el driver USB correspondiente al chip de la placa:
  - CP2102: disponible en el sitio de Silicon Labs.
  - CH340: buscar "CH340 driver" para Windows.
- Usar un cable USB que tenga lineas de datos (no solo carga).
