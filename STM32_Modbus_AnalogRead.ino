#include <SPI.h>
#include <Ethernet.h>
#include <ModbusIP.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// MOD_BUS
ModbusIP mb;

// ADS1115 - pierwszy moduł
Adafruit_ADS1115 ads1;

// ADS1115 - drugi moduł o adresie 0x49
Adafruit_ADS1115 ads2;

// REJESTRY dla MOD_BUS
int R0;
int R1;
int R2;
int R3;

void setup() {
  Serial.begin(115200);

  // MOD_BUS
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  IPAddress ip(192, 168, 0, 4);

  Ethernet.init(PA4);
  Ethernet.begin(mac, ip);
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  mb.addHreg(0);
  mb.addHreg(1);
  mb.addHreg(2);
  mb.addHreg(3);

  // I2C Init
  Wire.begin(PB6, PB7);

  // Inicjalizacja pierwszego ADS1115 na adresie 0x48
  if (!ads1.begin(0x48)) {
    Serial.println("Nie wykryto pierwszego ADS1115");
    while (1);
  }
  ads1.setDataRate(8);  // 8 SPS

  // Inicjalizacja drugiego ADS1115 na adresie 0x49
  if (!ads2.begin(0x49)) {
    Serial.println("Nie wykryto drugiego ADS1115");
    while (1);
  }
  ads2.setDataRate(8);  // 8 SPS

  Serial.println("ADS1115 gotowe");
}



// ODCZYTY NAPIĘCIA Z MODUŁÓW (NAPIĘCIE RÓŻNICOWE MIĘDZY A0-A1, A2-A3)

void loop() {
  // Odczyt z pierwszego ADS1115 (adres 0x48)
  int16_t diff_A0_A1 = ads1.readADC_Differential_0_1();
  float voltage_A0_A1 = diff_A0_A1 * 6.144 / 32767.0;
  R0 = (int16_t)(voltage_A0_A1 * 1000);

  int16_t diff_A2_A3 = ads1.readADC_Differential_2_3();
  float voltage_A2_A3 = diff_A2_A3 * 6.144 / 32767.0;
  R1 = (int16_t)(voltage_A2_A3 * 1000);

  // Odczyt z drugiego ADS1115 (adres 0x49)
  int16_t diff2_A0_A1 = ads2.readADC_Differential_0_1();
  float voltage2_A0_A1 = diff2_A0_A1 * 6.144 / 32767.0;
  R2 = (int16_t)(voltage2_A0_A1 * 1000);

  int16_t diff2_A2_A3 = ads2.readADC_Differential_2_3();
  float voltage2_A2_A3 = diff2_A2_A3 * 6.144 / 32767.0;
  R3 = (int16_t)(voltage2_A2_A3 * 1000);

  // MOD_BUS task i wysyłanie rejestrów
  mb.task();
  mb.Hreg(0, R0);
  mb.Hreg(1, R1);
  mb.Hreg(2, R2);
  mb.Hreg(3, R3);
}
