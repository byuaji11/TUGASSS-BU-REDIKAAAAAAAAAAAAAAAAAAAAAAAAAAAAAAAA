#include "BluetoothSerial.h"

// Kode ini memakai Bluetooth Classic SPP; gunakan ESP32 klasik seperti ESP32-WROOM-32.
// Hubungkan GND ESP32 dan GND driver motor secara bersama.
BluetoothSerial bluetooth;

// Pin kontrol L298N (IN1-IN4). Sesuaikan jika wiring Anda berbeda.
const int motorKiriMaju = 25;
const int motorKiriMundur = 26;
const int motorKananMaju = 27;
const int motorKananMundur = 14;

void setup() {
  Serial.begin(115200);

  pinMode(motorKiriMaju, OUTPUT);
  pinMode(motorKiriMundur, OUTPUT);
  pinMode(motorKananMaju, OUTPUT);
  pinMode(motorKananMundur, OUTPUT);

  matikanMotor();

  if (bluetooth.begin("RC_Car_ESP32")) {
    Serial.println("Bluetooth siap. Pasangkan dengan RC_Car_ESP32.");
  } else {
    Serial.println("Gagal memulai Bluetooth.");
  }
}

void loop() {
  if (bluetooth.available() > 0) {
    char dataDiterima = bluetooth.read();

    // Perintah aplikasi: F=maju, B=mundur, L=kiri, R=kanan, S=berhenti.
    switch (dataDiterima) {
      case 'F': maju(); break;
      case 'B': mundur(); break;
      case 'L': belokKiri(); break;
      case 'R': belokKanan(); break;
      case 'S': matikanMotor(); break;
      case '\r':
      case '\n':
        break;
      default:
        Serial.printf("Perintah tidak dikenal: %c\n", dataDiterima);
        break;
    }
  }
}

void maju() {
  digitalWrite(motorKiriMaju, HIGH);
  digitalWrite(motorKiriMundur, LOW);
  digitalWrite(motorKananMaju, HIGH);
  digitalWrite(motorKananMundur, LOW);
}

void mundur() {
  digitalWrite(motorKiriMaju, LOW);
  digitalWrite(motorKiriMundur, HIGH);
  digitalWrite(motorKananMaju, LOW);
  digitalWrite(motorKananMundur, HIGH);
}

void belokKiri() {
  digitalWrite(motorKiriMaju, LOW);
  digitalWrite(motorKiriMundur, HIGH);
  digitalWrite(motorKananMaju, HIGH);
  digitalWrite(motorKananMundur, LOW);
}

void belokKanan() {
  digitalWrite(motorKiriMaju, HIGH);
  digitalWrite(motorKiriMundur, LOW);
  digitalWrite(motorKananMaju, LOW);
  digitalWrite(motorKananMundur, HIGH);
}

void matikanMotor() {
  digitalWrite(motorKiriMaju, LOW);
  digitalWrite(motorKiriMundur, LOW);
  digitalWrite(motorKananMaju, LOW);
  digitalWrite(motorKananMundur, LOW);
}
