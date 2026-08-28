// ============================================================
//  Monitor Calidad del Aire - ESP32
//  NovaPM (SDS011) + DHT22 + LCD 16x2 Animaciones Suaves + LED RGB (ICAP)
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---- Pines Sensores ----
#define DHT_PIN    4        // GPIO 4 (seguro para bootloader)
#define DHT_TYPE   DHT22

#define PM_RX_PIN  16       // GPIO RX2 ← TX del sensor NovaPM
#define PM_TX_PIN  17       // GPIO TX2 → RX del sensor NovaPM

// ---- Pines I2C (LCD) ----
#define I2C_SDA    21       // GPIO 21
#define I2C_SCL    22       // GPIO 22

// ---- Pines LED RGB (Cátodo común) ----
#define PIN_R      25       // GPIO 25
#define PIN_G      26       // GPIO 26
#define PIN_B      27       // GPIO 27

// ---- Credenciales del AP ----
const char* AP_SSID = "SensorAire";
const char* AP_PASS = "12345678";

// ---- Instancias ----
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- Variables de lectura ----
float currentTemp = 0.0f, currentHum = 0.0f;
float currentPM25 = 0.0f, currentPM10 = 0.0f;

unsigned long lastDHTread = 0;
const unsigned long DHT_INTERVAL = 2000;

// Variables de rotación y animación fluida LCD
unsigned long lastScreenChange = 0;
unsigned long lastAnimTick = 0;
int currentScreen = 0;
int typewriterIndex = 0;
int animStep = 0;

// Caracteres personalizados
byte wifiSignalChar[8] = {
  0b00000, 0b00001, 0b00001, 0b00101,
  0b00101, 0b10101, 0b10101, 0b00000
};

// ---- Parser NovaPM SDS011 ----
static uint8_t pmBuf[10];
static uint8_t pmIdx = 0;

void processPMByte(uint8_t b) {
  if (pmIdx == 0 && b != 0xAA) return;
  pmBuf[pmIdx++] = b;
  if (pmIdx < 10) return;
  pmIdx = 0;

  if (pmBuf[1] != 0xC0 || pmBuf[9] != 0xAB) return;

  uint8_t sum = 0;
  for (uint8_t i = 2; i <= 7; i++) sum += pmBuf[i];
  if (sum != pmBuf[8]) return;

  currentPM25 = (pmBuf[3] * 256 + pmBuf[2]) / 10.0f;
  currentPM10 = (pmBuf[5] * 256 + pmBuf[4]) / 10.0f;
}

// ---- Control de LED RGB ----
void setRGB(int r, int g, int b) {
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
}

int getNivelICAP(float pm25, float pm10) {
  int lvl25 = 0, lvl10 = 0;
  if      (pm25 <= 25)  lvl25 = 0;
  else if (pm25 <= 50)  lvl25 = 1;
  else if (pm25 <= 110) lvl25 = 2;
  else if (pm25 <= 170) lvl25 = 3;
  else                  lvl25 = 4;

  if      (pm10 <= 75)  lvl10 = 0;
  else if (pm10 <= 150) lvl10 = 1;
  else if (pm10 <= 250) lvl10 = 2;
  else if (pm10 <= 330) lvl10 = 3;
  else                  lvl10 = 4;

  return max(lvl25, lvl10);
}

void actualizarSemaforoRGB(int nivel) {
  switch (nivel) {
    case 0: setRGB(0, 255, 0);   break; // Verde: Buena
    case 1: setRGB(255, 200, 0); break; // Amarillo: Regular
    case 2: setRGB(255, 60, 0);  break; // Naranja: Alerta
    case 3: setRGB(255, 0, 0);   break; // Rojo: Pre-emergencia
    case 4: setRGB(180, 0, 255); break; // Violeta/Morado: Emergencia
  }
}

// ---- Renderizado del LCD con Efectos ----
void renderLCD() {
  int connectedClients = WiFi.softAPgetStationNum();

  switch (currentScreen) {
    case 0: {
      // Pantalla 0: Letra por letra SANTO (arriba) y TOMAS (abajo)
      const String txt1 = "SANTO";
      const String txt2 = "TOMAS";
      
      lcd.setCursor(5, 0);
      if (typewriterIndex <= txt1.length()) {
        lcd.print(txt1.substring(0, typewriterIndex));
      } else {
        lcd.print(txt1);
      }

      lcd.setCursor(5, 1);
      if (typewriterIndex > txt1.length()) {
        int idx2 = typewriterIndex - txt1.length();
        lcd.print(txt2.substring(0, min(idx2, (int)txt2.length())));
      }
      break;
    }

    case 1: {
      // Pantalla 1: INFORMATICA (letra por letra) + ESTACION AIRE animado
      const String txtInfo = "INFORMATICA";
      lcd.setCursor(2, 0);
      lcd.print(txtInfo.substring(0, min(typewriterIndex, (int)txtInfo.length())));

      // Animación inferior para "ESTACION AIRE" con efecto de pulsación
      lcd.setCursor(0, 1);
      if ((animStep % 4) == 0) {
        lcd.print("< ESTACION AIRE>");
      } else if ((animStep % 4) == 2) {
        lcd.print("> ESTACION AIRE<");
      } else {
        lcd.print("  ESTACION AIRE ");
      }
      break;
    }

    case 2:
      // Pantalla 2: Material Particulado
      lcd.setCursor(0, 0);
      lcd.print("PM2.5: " + String(currentPM25, 1) + " ug ");
      lcd.setCursor(0, 1);
      lcd.print("PM10 : " + String(currentPM10, 1) + " ug ");
      break;

    case 3:
      // Pantalla 3: Temperatura y Humedad
      lcd.setCursor(0, 0);
      lcd.print("Temp: " + String(currentTemp, 1) + " " + (char)223 + "C    ");
      lcd.setCursor(0, 1);
      lcd.print("Hum : " + String(currentHum, 1) + " %     ");
      break;

    case 4:
      // Pantalla 4: Credenciales WiFi
      lcd.setCursor(0, 0);
      lcd.print("RED : SensorAire");
      lcd.setCursor(0, 1);
      lcd.print("PASS: 12345678  ");
      break;

    case 5:
      // Pantalla 5: IP y Conectados
      lcd.setCursor(0, 0);
      lcd.print("IP: 192.168.4.1 ");
      lcd.setCursor(0, 1);
      lcd.print("Clientes: ");
      lcd.print(connectedClients);
      lcd.print("  ");
      if ((animStep % 2) == 0) {
        lcd.write(1); // Ícono WiFi
      } else {
        lcd.print(" ");
      }
      break;
  }
}

// ============================================================
//  Página web autocontenida
// ============================================================
static const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monitor Calidad del Aire</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',sans-serif;background:#0f1117;color:#e0e0e0;min-height:100vh}
header{background:#1a1d27;padding:18px;text-align:center;border-bottom:1px solid #2a2d3e}
header h1{font-size:1.5rem;color:#7eb8f7}
header p{font-size:.8rem;color:#666;margin-top:4px}
.cards{display:flex;flex-wrap:wrap;gap:14px;padding:20px;justify-content:center}
.card{background:#1a1d27;border-radius:12px;padding:18px 26px;min-width:140px;text-align:center;border:1px solid #2a2d3e}
.lbl{font-size:.7rem;color:#888;text-transform:uppercase;letter-spacing:1px}
.val{font-size:2.2rem;font-weight:700;margin:8px 0 2px}
.unt{font-size:.8rem;color:#888}
.ct{color:#ff7043}.ch{color:#42a5f5}.cp2{color:#ab47bc}.cp1{color:#26a69a}
.charts{display:flex;flex-direction:column;gap:20px;padding:0 20px 20px}
.cbox{background:#1a1d27;border-radius:12px;padding:16px;border:1px solid #2a2d3e}
.cbox h2{font-size:.9rem;color:#aaa;margin-bottom:10px}
canvas{display:block;width:100%;height:200px}
#st{text-align:center;padding:10px 0 20px;font-size:.75rem;color:#555}
.aqi{margin:0 20px 20px;border-radius:12px;padding:16px 22px;border:1px solid #2a2d3e;transition:background .6s,border-color .6s}
.aqi-top{display:flex;align-items:center;gap:14px}
.aqi-dot{width:18px;height:18px;border-radius:50%;flex-shrink:0;transition:background .6s}
.aqi-label{font-size:1.2rem;font-weight:700;transition:color .6s}
.aqi-title{font-size:.7rem;color:#888;text-transform:uppercase;letter-spacing:1px;margin-bottom:6px}
.aqi-desc{font-size:.8rem;color:#bbb;margin-top:8px;line-height:1.5}
</style>
</head>
<body>
<header>
  <h1>Monitor Calidad del Aire</h1>
  <p>Santo Tomás &middot; NovaPM 5006 &middot; DHT22</p>
</header>
<div class="cards">
  <div class="card"><div class="lbl">Temperatura</div><div class="val ct" id="T">--</div><div class="unt">°C</div></div>
  <div class="card"><div class="lbl">Humedad</div><div class="val ch" id="H">--</div><div class="unt">%</div></div>
  <div class="card"><div class="lbl">PM 2.5</div><div class="val cp2" id="P2">--</div><div class="unt">μg/m³</div></div>
  <div class="card"><div class="lbl">PM 10</div><div class="val cp1" id="P1">--</div><div class="unt">μg/m³</div></div>
</div>
<div class="aqi" id="aqiBox">
  <div class="aqi-title">Indice de Calidad del Aire (ICAP Chile)</div>
  <div class="aqi-top">
    <div class="aqi-dot" id="aqiDot"></div>
    <div class="aqi-label" id="aqiLabel">Esperando datos...</div>
  </div>
  <div class="aqi-desc" id="aqiDesc">PM2.5 y PM10 aun no disponibles.</div>
</div>
<div class="charts">
  <div class="cbox"><h2>Temperatura &amp; Humedad</h2><canvas id="cTH"></canvas></div>
  <div class="cbox"><h2>Material Particulado</h2><canvas id="cPM"></canvas></div>
</div>
<div id="st">Conectando...</div>

<script>
function MiniChart(id, series) {
  var cv = document.getElementById(id);
  var cx = cv.getContext('2d');
  var MAX = 60;
  var data = series.map(function(){ return []; });
  var lb   = [];

  function resize() {
    var r = cv.getBoundingClientRect();
    cv.width  = r.width  || 320;
    cv.height = r.height || 200;
  }

  this.push = function(values) {
    resize();
    lb.push(new Date().toLocaleTimeString('es-CL'));
    values.forEach(function(v, i){ data[i].push(v); });
    if (lb.length > MAX) { lb.shift(); data.forEach(function(d){ d.shift(); }); }
    draw();
  };

  function draw() {
    var W = cv.width, H = cv.height;
    var PL = 46, PR = 12, PT = 28, PB = 28;
    cx.clearRect(0, 0, W, H);

    var all = [];
    data.forEach(function(d){ d.forEach(function(v){ if(isFinite(v)) all.push(v); }); });
    if (!all.length) return;

    var mn = Math.min.apply(null, all);
    var mx = Math.max.apply(null, all);
    if (mn === mx) { mn -= 1; mx += 1; }
    var rng = mx - mn;
    var N   = lb.length;
    var pw  = W - PL - PR, ph = H - PT - PB;

    function tx(i){ return PL + (N > 1 ? i / (N - 1) : 0.5) * pw; }
    function ty(v){ return PT + (1 - (v - mn) / rng) * ph; }

    cx.fillStyle = '#1a1d27';
    cx.fillRect(0, 0, W, H);

    for (var k = 0; k <= 4; k++) {
      var gy = PT + k / 4 * ph;
      cx.strokeStyle = '#2a2d3e'; cx.lineWidth = 1;
      cx.beginPath(); cx.moveTo(PL, gy); cx.lineTo(W - PR, gy); cx.stroke();
      cx.fillStyle = '#666'; cx.font = '10px sans-serif'; cx.textAlign = 'right';
      cx.fillText((mx - k / 4 * rng).toFixed(1), PL - 3, gy + 4);
    }

    cx.fillStyle = '#555'; cx.font = '9px sans-serif'; cx.textAlign = 'center';
    [0, 0.33, 0.66, 1].forEach(function(f){
      var i = Math.round(f * (N - 1));
      if (lb[i]) cx.fillText(lb[i], tx(i), H - 6);
    });

    series.forEach(function(s, si){
      if (data[si].length < 2) return;
      cx.strokeStyle = s.color; cx.lineWidth = 2;
      cx.beginPath();
      data[si].forEach(function(v, i){
        i === 0 ? cx.moveTo(tx(i), ty(v)) : cx.lineTo(tx(i), ty(v));
      });
      cx.stroke();
      var lx = PL + si * 160;
      cx.fillStyle = s.color;
      cx.fillRect(lx, 6, 12, 10);
      cx.fillStyle = '#aaa'; cx.textAlign = 'left'; cx.font = '10px sans-serif';
      cx.fillText(s.label, lx + 16, 15);
    });
  }
}

var cTH = new MiniChart('cTH', [
  { color: '#ff7043', label: 'Temp (°C)' },
  { color: '#42a5f5', label: 'Humedad (%)' }
]);
var cPM = new MiniChart('cPM', [
  { color: '#ab47bc', label: 'PM2.5 μg/m³' },
  { color: '#26a69a', label: 'PM10 μg/m³'  }
]);

function tick() {
  fetch('/data')
    .then(function(r){ return r.json(); })
    .then(function(j){
      document.getElementById('T').textContent  = j.t.toFixed(1);
      document.getElementById('H').textContent  = j.h.toFixed(1);
      document.getElementById('P2').textContent = j.p2.toFixed(1);
      document.getElementById('P1').textContent = j.p1.toFixed(1);
      cTH.push([j.t, j.h]);
      cPM.push([j.p2, j.p1]);
      updateICAP(j.p2, j.p1);
      document.getElementById('st').textContent =
        'Última actualización: ' + new Date().toLocaleTimeString('es-CL');
    })
    .catch(function(){
      document.getElementById('st').textContent = 'Error al obtener datos';
    });
}

var ICAP = [
  { label: 'Buena',          dot: '#43a047', color: '#66bb6a', bg: '#0d1f0e', border: '#2e7d32',
    desc: 'Calidad del aire satisfactoria. No representa riesgo para la salud de la poblacion general.' },
  { label: 'Regular',        dot: '#fdd835', color: '#ffee58', bg: '#1f1a00', border: '#f9a825',
    desc: 'Calidad aceptable. Personas muy sensibles (asmaticos, adultos mayores) podrian presentar leve malestar.' },
  { label: 'Alerta',         dot: '#fb8c00', color: '#ffa726', bg: '#1f0e00', border: '#e65100',
    desc: 'Grupos sensibles pueden experimentar efectos respiratorios. Se recomienda reducir actividad fisica al aire libre.' },
  { label: 'Pre-emergencia', dot: '#e53935', color: '#ef5350', bg: '#1f0505', border: '#c62828',
    desc: 'Toda la poblacion puede experimentar efectos. Evitar actividad fisica al aire libre. Grupos de riesgo deben permanecer en interiores.' },
  { label: 'Emergencia',     dot: '#8e24aa', color: '#ab47bc', bg: '#150520', border: '#6a1b9a',
    desc: 'Alerta maxima sanitaria. Toda la poblacion debe evitar cualquier exposicion al exterior. Siga las instrucciones de la autoridad.' }
];

function getICAP(pm25, pm10) {
  var lvl25, lvl10;
  if      (pm25 <= 25)  lvl25 = 0;
  else if (pm25 <= 50)  lvl25 = 1;
  else if (pm25 <= 110) lvl25 = 2;
  else if (pm25 <= 170) lvl25 = 3;
  else                  lvl25 = 4;

  if      (pm10 <= 75)  lvl10 = 0;
  else if (pm10 <= 150) lvl10 = 1;
  else if (pm10 <= 250) lvl10 = 2;
  else if (pm10 <= 330) lvl10 = 3;
  else                  lvl10 = 4;

  return ICAP[Math.max(lvl25, lvl10)];
}

function updateICAP(pm25, pm10) {
  var nivel = getICAP(pm25, pm10);
  var box   = document.getElementById('aqiBox');
  var dot   = document.getElementById('aqiDot');
  var lbl   = document.getElementById('aqiLabel');
  var desc  = document.getElementById('aqiDesc');
  box.style.background   = nivel.bg;
  box.style.borderColor = nivel.border;
  dot.style.background   = nivel.dot;
  lbl.style.color        = nivel.color;
  lbl.textContent        = nivel.label;
  desc.textContent       = 'PM2.5: ' + pm25.toFixed(1) + ' ug/m3  |  PM10: ' + pm10.toFixed(1) + ' ug/m3  —  ' + nivel.desc;
}

tick();
setInterval(tick, 2000);
</script>
</body>
</html>
)rawliteral";

// ============================================================
//  Manejadores HTTP
// ============================================================
void handleRoot() {
  server.send_P(200, "text/html", HTML);
}

void handleData() {
  String j = F("{\"t\":");
  j += String(currentTemp, 1);
  j += F(",\"h\":");
  j += String(currentHum, 1);
  j += F(",\"p2\":");
  j += String(currentPM25, 1);
  j += F(",\"p1\":");
  j += String(currentPM10, 1);
  j += '}';
  server.send(200, F("application/json"), j);
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);

  // Inicializar pines del LED RGB
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  setRGB(0, 0, 0);

  // Inicializar bus I2C y LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.createChar(1, wifiSignalChar);

  // Serial2 → sensor NovaPM a 9600 baud
  Serial2.begin(9600, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);

  dht.begin();

  // Crear Access Point WiFi
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print(F("AP creado. IP: "));
  Serial.println(WiFi.softAPIP());

  server.on("/",     HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.begin();
  Serial.println(F("Servidor HTTP iniciado"));

  actualizarSemaforoRGB(0);
}

// ============================================================
//  Loop
// ============================================================
void loop() {
  unsigned long now = millis();

  // 1. Parser NovaPM
  while (Serial2.available()) {
    processPMByte((uint8_t)Serial2.read());
  }

  // 2. Servidor Web
  server.handleClient();

  // 3. Lectura periódica DHT22
  if (now - lastDHTread >= DHT_INTERVAL) {
    lastDHTread = now;
    float t = dht.readTemperature(false);
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t;
    if (!isnan(h)) currentHum  = h;

    int nivel = getNivelICAP(currentPM25, currentPM10);
    actualizarSemaforoRGB(nivel);
  }

  // 4. Temporizador de animación suave (cada 150 ms añade una letra o paso)
  if (now - lastAnimTick >= 150) {
    lastAnimTick = now;
    typewriterIndex++;
    animStep++;
    renderLCD();
  }

  // 5. Rotación de pantallas principales cada 5 segundos (5000 ms)
  if (now - lastScreenChange >= 5000) {
    lastScreenChange = now;
    currentScreen = (currentScreen + 1) % 6; // Rota entre 0 y 5
    typewriterIndex = 0;                     // Reiniciar el contador de máquina de escribir
    lcd.clear();
  }
}
