#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// =================================================
// WIFI
// =================================================
const char* WIFI_NAME = "RobotESP32";
const char* WIFI_PASS = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =================================================
// TB6612FNG
// =================================================
const int PWMA = 18;
const int AIN1 = 26;
const int AIN2 = 27;

const int PWMB = 14;
const int BIN1 = 16;
const int BIN2 = 17;

const int STBY = 23;

// Ubah menjadi true jika arah motor terbalik
const bool REVERSE_LEFT = false;
const bool REVERSE_RIGHT = false;

// =================================================
// SENSOR IR
// =================================================
const int sensorPin[5] = {32, 33, 34, 35, 25};

// true = sensor membaca hitam sebagai LOW
const bool SENSOR_ACTIVE_LOW = true;

// =================================================
// VARIABEL KONTROL
// =================================================
volatile char currentCommand = 'S';
volatile int speedValue = 170;
volatile bool lineMode = false;

volatile unsigned long lastCommandTime = 0;

const unsigned long COMMAND_TIMEOUT = 400;

int lastLineDirection = 0;

// =================================================
// HALAMAN WEB
// =================================================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1,
maximum-scale=1, user-scalable=no">

<title>Robot ESP32</title>

<style>
* {
  box-sizing: border-box;
  -webkit-tap-highlight-color: transparent;
}

html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: #111827;
  color: white;
  font-family: Arial, sans-serif;
  touch-action: none;
}

.app {
  width: 100%;
  height: 100%;
  padding: 12px 20px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 12px;
}

/* Putar tampilan saat HP berdiri */
@media (orientation: portrait) {
  .app {
    position: fixed;
    top: 0;
    left: 100vw;
    width: 100vh;
    height: 100vw;
    transform: rotate(90deg);
    transform-origin: top left;
  }
}

.control-area {
  flex: 1;
  min-height: 0;

  display: grid;
  grid-template-columns: 1fr 85px 1fr;
  gap: 14px;
  align-items: center;
}

.control-box {
  height: 100%;
  max-height: 230px;
  padding: 12px;
  border-radius: 18px;
  background: #273449;

  display: flex;
  justify-content: center;
  align-items: center;
  gap: 12px;
}

.control-button {
  border: none;
  border-radius: 14px;
  background: #f3f4f6;
  color: #111827;

  font-size: clamp(25px, 5vw, 42px);
  font-weight: bold;

  cursor: pointer;
  touch-action: none;
  user-select: none;
  -webkit-user-select: none;
}

.control-button.active {
  background: #0ea5e9;
  color: white;
}

.left-box .control-button {
  width: 50%;
  height: 85%;
}

.right-box {
  flex-direction: column;
}

.right-box .control-button {
  width: 85%;
  height: 50%;
}

.mode-box {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.mode-button {
  height: 65px;
  border: none;
  border-radius: 12px;
  background: #374151;
  color: white;
  font-weight: bold;
  font-size: 14px;
}

.mode-button.selected {
  background: #2563eb;
}

.speed-box {
  flex-shrink: 0;
  background: #273449;
  padding: 12px 18px;
  border-radius: 14px;
}

.speed-label {
  display: flex;
  justify-content: space-between;
  font-size: 14px;
  margin-bottom: 8px;
}

input[type=range] {
  width: 100%;
  accent-color: #0ea5e9;
  touch-action: pan-y;
}

.status {
  text-align: center;
  font-size: 11px;
  color: #9ca3af;
}
</style>
</head>

<body>

<div class="app">

  <div class="control-area">

    <!-- KIRI DAN KANAN -->
    <div class="control-box left-box">

      <button class="control-button"
        id="leftButton">◀</button>

      <button class="control-button"
        id="rightButton">▶</button>

    </div>

    <!-- MODE -->
    <div class="mode-box">

      <button class="mode-button selected"
        id="rcButton">RC</button>

      <button class="mode-button"
        id="lineButton">LINE</button>

    </div>

    <!-- MAJU DAN MUNDUR -->
    <div class="control-box right-box">

      <button class="control-button"
        id="forwardButton">▲</button>

      <button class="control-button"
        id="backwardButton">▼</button>

    </div>

  </div>

  <!-- KECEPATAN -->
  <div class="speed-box">

    <div class="speed-label">
      <b>KECEPATAN</b>
      <b id="speedText">170</b>
    </div>

    <input type="range"
      id="speedSlider"
      min="80"
      max="255"
      value="170">

  </div>

  <div class="status" id="status">
    MENGHUBUNGKAN KE ROBOT...
  </div>

</div>

<script>

// =================================================
// WEBSOCKET
// =================================================
let socket = null;
let reconnectTimer = null;

let activeCommand = 'S';
let activeButton = null;
let heartbeat = null;

let mode = 'RC';
let speedValue = 170;

function connectRobot() {

  if (socket &&
      (socket.readyState === WebSocket.OPEN ||
       socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const protocol =
    location.protocol === 'https:' ? 'wss://' : 'ws://';

  socket = new WebSocket(protocol + location.host + '/ws');

  socket.onopen = () => {
    document.getElementById('status').innerText =
      'TERHUBUNG KE ROBOT';

    socket.send('V' + speedValue);
    socket.send(mode === 'RC' ? 'M0' : 'M1');
  };

  socket.onclose = () => {
    stopLocal();

    document.getElementById('status').innerText =
      'KONEKSI TERPUTUS - MENYAMBUNG...';

    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(connectRobot, 1000);
  };

  socket.onerror = () => {
    socket.close();
  };
}

function sendData(data) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(data);
  }
}

connectRobot();

// =================================================
// KONTROL TOMBOL
// =================================================
function startHeartbeat() {

  if (heartbeat) clearInterval(heartbeat);

  // Kirim perintah awal langsung
  sendData(activeCommand);

  // Heartbeat kecil untuk pengaman koneksi
  heartbeat = setInterval(() => {
    if (activeCommand !== 'S') {
      sendData(activeCommand);
    }
  }, 120);
}

function stopLocal() {

  activeCommand = 'S';

  if (heartbeat) {
    clearInterval(heartbeat);
    heartbeat = null;
  }

  if (activeButton) {
    activeButton.classList.remove('active');
    activeButton = null;
  }

  sendData('S');
}

function stopMove() {
  stopLocal();
}

function startMove(command, button) {

  if (mode !== 'RC') return;

  stopLocal();

  activeCommand = command;
  activeButton = button;

  button.classList.add('active');

  startHeartbeat();
}

function bindButton(id, command) {

  const button = document.getElementById(id);

  button.addEventListener('pointerdown', event => {

    event.preventDefault();

    if (mode !== 'RC') return;

    button.setPointerCapture(event.pointerId);

    startMove(command, button);
  });

  button.addEventListener('pointerup', stopMove);
  button.addEventListener('pointercancel', stopMove);
  button.addEventListener('lostpointercapture', stopMove);
}

bindButton('leftButton', 'L');
bindButton('rightButton', 'R');
bindButton('forwardButton', 'F');
bindButton('backwardButton', 'B');

// =================================================
// SLIDER KECEPATAN
// =================================================
const slider = document.getElementById('speedSlider');

slider.addEventListener('input', () => {

  speedValue = Number(slider.value);

  document.getElementById('speedText').innerText =
    speedValue;

  sendData('V' + speedValue);
});

// =================================================
// MODE RC DAN LINE
// =================================================
function setMode(newMode) {

  stopLocal();

  mode = newMode;

  document.getElementById('rcButton')
    .classList.toggle('selected', mode === 'RC');

  document.getElementById('lineButton')
    .classList.toggle('selected', mode === 'LINE');

  document.getElementById('status').innerText =
    'MODE ' + mode;

  sendData(mode === 'RC' ? 'M0' : 'M1');
}

document.getElementById('rcButton').onclick = () => {
  setMode('RC');
};

document.getElementById('lineButton').onclick = () => {
  setMode('LINE');
};

// =================================================
// PENGAMAN
// =================================================
window.addEventListener('blur', stopMove);

document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopMove();
});

// Berhenti jika halaman ditutup
window.addEventListener('pagehide', stopMove);

</script>
</body>
</html>
)rawliteral";

// =================================================
// MOTOR
// =================================================
void motorLeft(int speed) {

  speed = constrain(speed, -255, 255);

  if (REVERSE_LEFT) speed = -speed;

  if (speed > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  }
  else if (speed < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }

  analogWrite(PWMA, abs(speed));
}

void motorRight(int speed) {

  speed = constrain(speed, -255, 255);

  if (REVERSE_RIGHT) speed = -speed;

  if (speed > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  }
  else if (speed < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  }
  else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }

  analogWrite(PWMB, abs(speed));
}

void moveRobot(int leftSpeed, int rightSpeed) {

  digitalWrite(STBY, HIGH);

  motorLeft(leftSpeed);
  motorRight(rightSpeed);
}

void stopRobot() {
  motorLeft(0);
  motorRight(0);
}

// =================================================
// RC
// =================================================
void runRC() {

  int s = speedValue;
  char command = currentCommand;

  switch (command) {

    case 'F':
      moveRobot(s, s);
      break;

    case 'B':
      moveRobot(-s, -s);
      break;

    case 'L':
      moveRobot(-s, s);
      break;

    case 'R':
      moveRobot(s, -s);
      break;

    default:
      stopRobot();
      break;
  }
}

// =================================================
// LINE FOLLOWER
// =================================================
bool readBlack(int pin) {

  int value = digitalRead(pin);

  if (SENSOR_ACTIVE_LOW) {
    return value == LOW;
  }

  return value == HIGH;
}

void runLineFollower() {

  bool sensor[5];

  for (int i = 0; i < 5; i++) {
    sensor[i] = readBlack(sensorPin[i]);
  }

  const int weight[5] = {-4, -2, 0, 2, 4};

  int total = 0;
  int count = 0;

  for (int i = 0; i < 5; i++) {
    if (sensor[i]) {
      total += weight[i];
      count++;
    }
  }

  if (count == 0) {

    if (lastLineDirection < 0) {
      moveRobot(-100, 100);
    }
    else if (lastLineDirection > 0) {
      moveRobot(100, -100);
    }
    else {
      stopRobot();
    }

    return;
  }

  int error = total / count;

  if (error < 0) lastLineDirection = -1;
  if (error > 0) lastLineDirection = 1;

  int correction = error * 30;

  int leftSpeed = speedValue + correction;
  int rightSpeed = speedValue - correction;

  leftSpeed = constrain(leftSpeed, 0, 255);
  rightSpeed = constrain(rightSpeed, 0, 255);

  moveRobot(leftSpeed, rightSpeed);
}

// =================================================
// WEBSOCKET CALLBACK
// =================================================
void onWebSocketEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {

  if (type == WS_EVT_DISCONNECT) {
    currentCommand = 'S';
    lastCommandTime = millis();
    return;
  }

  if (type != WS_EVT_DATA || len == 0) {
    return;
  }

  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (!info->final ||
      info->index != 0 ||
      info->len != len ||
      info->opcode != WS_TEXT) {
    return;
  }

  char first = (char)data[0];

  // Perintah gerakan
  if (len == 1) {

    if (!lineMode) {

      if (first == 'F' || first == 'B' ||
          first == 'L' || first == 'R' ||
          first == 'S') {

        currentCommand = first;
        lastCommandTime = millis();
      }
    }

    return;
  }

  // Pengaturan kecepatan: V170
  if (first == 'V') {

    int value = 0;

    for (size_t i = 1; i < len; i++) {
      if (data[i] >= '0' && data[i] <= '9') {
        value = value * 10 + (data[i] - '0');
      }
    }

    speedValue = constrain(value, 0, 255);
    return;
  }

  // Mode RC = M0, LINE = M1
  if (first == 'M' && len >= 2) {

    char selected = (char)data[1];

    if (selected == '0' || selected == '1') {

      currentCommand = 'S';
      lastCommandTime = millis();

      lineMode = (selected == '1');

      stopRobot();
    }
  }
}

// =================================================
// SETUP
// =================================================
void setup() {

  Serial.begin(115200);

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);

  digitalWrite(STBY, HIGH);

  for (int i = 0; i < 5; i++) {
    pinMode(sensorPin[i], INPUT);
  }

  stopRobot();

  // Wi-Fi tanpa mode hemat daya
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  // Access point, maksimal satu HP
  WiFi.softAP(
    WIFI_NAME,
    WIFI_PASS,
    6,
    false,
    1
  );

  Serial.println();
  Serial.println("ROBOT ESP32 SIAP");
  Serial.print("Wi-Fi: ");
  Serial.println(WIFI_NAME);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // WebSocket
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Halaman kontrol
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", webpage);
  });

  server.begin();

  lastCommandTime = millis();
}

// =================================================
// LOOP
// =================================================
void loop() {

  ws.cleanupClients();

  // Pengaman koneksi dan perintah
  if (!lineMode) {

    if (millis() - lastCommandTime > COMMAND_TIMEOUT) {
      currentCommand = 'S';
    }

    runRC();
  }
  else {
    runLineFollower();
  }

  delay(2);
}