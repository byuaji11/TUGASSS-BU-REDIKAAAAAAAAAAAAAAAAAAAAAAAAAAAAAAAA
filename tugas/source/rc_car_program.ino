#include <WiFi.h>
#include <WebServer.h>

// ESP32 membuat access point sendiri. Hubungkan ponsel ke Wi-Fi ini,
// lalu buka http://192.168.4.1 untuk mengendalikan mobil.
const char* namaWiFi = "RC_Car_ESP32";
const char* passwordWiFi = "rcmobil123"; // Ganti dengan password unik (minimal 8 karakter).

WebServer server(80);

// Pin kontrol L298N (IN1-IN4); sesuaikan dengan wiring Anda.
const int motorKiriMaju = 25;
const int motorKiriMundur = 26;
const int motorKananMaju = 27;
const int motorKananMundur = 14;

unsigned long waktuPerintahTerakhir = 0;
const unsigned long batasWaktuPerintah = 700;
bool motorBergerak = false;

const char halamanKontrol[] PROGMEM = R"HTML(
<!doctype html><html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Kontrol RC Car ESP32</title>
<style>*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;padding:20px;font-family:Arial,sans-serif;color:#e5eefb;background:linear-gradient(145deg,#07111f,#17283d)}main{width:min(440px,100%);padding:26px;border:1px solid #334155;border-radius:22px;background:#0f172a;box-shadow:0 18px 45px #020c1b}h1{text-align:center}p{text-align:center;color:#a7b9d6}.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:24px 0}.pad button{min-height:76px;border:0;border-radius:16px;background:#67e8f9;color:#07111f;font-size:1.15rem;font-weight:bold;touch-action:none}.pad button:active{background:#c084fc}.stop{grid-column:2;background:#f87171!important}.hint{font-size:.9rem;line-height:1.5}</style></head><body><main><h1>🚗 RC Car ESP32</h1><p>Tekan dan tahan tombol untuk bergerak.</p>
<div class="pad"><span></span><button data-cmd="F">▲<br>Maju</button><span></span><button data-cmd="L">◀<br>Kiri</button><button class="stop" data-cmd="S">■<br>Stop</button><button data-cmd="R">▶<br>Kanan</button><span></span><button data-cmd="B">▼<br>Mundur</button><span></span></div>
<p class="hint">Hubungkan ponsel ke Wi-Fi RC_Car_ESP32 (kata sandi sesuai pengaturan sketch), lalu akses 192.168.4.1. Lepaskan tombol untuk berhenti.</p>
<script>const send=cmd=>fetch('/control?cmd='+cmd,{cache:'no-store'}).catch(()=>{});document.querySelectorAll('button[data-cmd]').forEach(button=>{const cmd=button.dataset.cmd;if(cmd==='S'){button.addEventListener('pointerdown',()=>send('S'));return;}button.addEventListener('pointerdown',event=>{event.preventDefault();button.setPointerCapture(event.pointerId);send(cmd);});for(const name of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(name,()=>send('S'));});</script></main></body></html>
)HTML";

void aturMotor(int kiriMaju, int kiriMundur, int kananMaju, int kananMundur) {
  digitalWrite(motorKiriMaju, kiriMaju);
  digitalWrite(motorKiriMundur, kiriMundur);
  digitalWrite(motorKananMaju, kananMaju);
  digitalWrite(motorKananMundur, kananMundur);
  motorBergerak = (kiriMaju || kiriMundur || kananMaju || kananMundur);
  waktuPerintahTerakhir = millis();
}

void matikanMotor() { aturMotor(LOW, LOW, LOW, LOW); }

void tanganiKontrol() {
  if (!server.hasArg("cmd") || server.arg("cmd").length() != 1) {
    server.send(400, "text/plain", "Perintah tidak valid");
    return;
  }
  const char perintah = server.arg("cmd")[0];
  switch (perintah) {
    case 'F': aturMotor(HIGH, LOW, HIGH, LOW); break;
    case 'B': aturMotor(LOW, HIGH, LOW, HIGH); break;
    case 'L': aturMotor(LOW, HIGH, HIGH, LOW); break;
    case 'R': aturMotor(HIGH, LOW, LOW, HIGH); break;
    case 'S': matikanMotor(); break;
    default: server.send(400, "text/plain", "Perintah tidak dikenal"); return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  pinMode(motorKiriMaju, OUTPUT);
  pinMode(motorKiriMundur, OUTPUT);
  pinMode(motorKananMaju, OUTPUT);
  pinMode(motorKananMundur, OUTPUT);
  matikanMotor();
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(namaWiFi, passwordWiFi)) {
    Serial.println("Gagal membuat access point Wi-Fi.");
    return;
  }
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", halamanKontrol); });
  server.on("/control", HTTP_GET, tanganiKontrol);
  server.begin();
  Serial.print("Wi-Fi: "); Serial.println(namaWiFi);
  Serial.print("Alamat kontrol: http://"); Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  if (motorBergerak && millis() - waktuPerintahTerakhir > batasWaktuPerintah) matikanMotor();
}
