#include <SPI.h>
#include <Ethernet.h>
#include <ModbusIP.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

/* =================================================================================
 * KONFIGURACJA SIECI I MODBUS
 * ================================================================================= */

ModbusIP mb;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 4);

// Rejestry
const int MB_REG_R0      = 0;
const int MB_REG_R1      = 1;
const int MB_REG_R2      = 2;
const int MB_REG_R3      = 3;
const int MB_REG_TEMP_HI = 4;
const int MB_REG_TEMP_LO = 5;

// Zmienne na dane
int16_t val_R0 = 0;
int16_t val_R1 = 0;
int16_t val_R2 = 0;
int16_t val_R3 = 0;
int32_t val_Temp32 = 0;

/* =================================================================================
 * KONFIGURACJA ADS1220 (SPI)
 * ================================================================================= */

SPIClass SPI_2(PB15, PB14, PB13);
#define ADS1220_CS_PIN    PB12
#define ADS1220_DRDY_PIN  PB10

#define CMD_RESET 6
#define CMD_START 8
#define CMD_WREG  64
#define CMD_RDATA 16

const float R_REF = 5100.0;       
const float R_0 = 1000.0;         
const float TEMP_OFFSET = 0.0;    

volatile bool drdyIntrFlag = false;

// Przerwanie - zgłasza gotowość danych co 50ms (dla 20 SPS)
void drdyIntr() {
  drdyIntrFlag = true;
}

void ads1220_writeReg(uint8_t reg, uint8_t value) {
  digitalWrite(ADS1220_CS_PIN, LOW);
  SPI_2.transfer(CMD_WREG | (reg << 2));
  SPI_2.transfer(value);
  digitalWrite(ADS1220_CS_PIN, HIGH);
}

void ads1220_cmd(uint8_t cmd) {
  digitalWrite(ADS1220_CS_PIN, LOW);
  SPI_2.transfer(cmd);
  digitalWrite(ADS1220_CS_PIN, HIGH);
}

int32_t ads1220_readData() {
  digitalWrite(ADS1220_CS_PIN, LOW);
  uint8_t msb = SPI_2.transfer(0);
  uint8_t mid = SPI_2.transfer(0);
  uint8_t lsb = SPI_2.transfer(0);
  digitalWrite(ADS1220_CS_PIN, HIGH);

  int32_t value = ((int32_t)msb << 16) | ((int32_t)mid << 8) | lsb;
  if (value & 0x800000) value |= 0xFF000000;
  return value;
}

/* =================================================================================
 * KONFIGURACJA ADS1115 (I2C)
 * ================================================================================= */

Adafruit_ADS1115 ads1; 
Adafruit_ADS1115 ads2; 

unsigned long last_ads1115_read = 0;
const int ADS1115_INTERVAL = 100;

/* =================================================================================
 * SETUP
 * ================================================================================= */
void setup() {

  Serial.begin(115200);
  delay(4000);

  // 1. ETHERNET & MODBUS
  Ethernet.init(PA4);
  Ethernet.begin(mac, ip);
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());

  mb.addHreg(MB_REG_R0);
  mb.addHreg(MB_REG_R1);
  mb.addHreg(MB_REG_R2);
  mb.addHreg(MB_REG_R3);
  mb.addHreg(MB_REG_TEMP_HI);
  mb.addHreg(MB_REG_TEMP_LO);

  // 2. ADS1220 (SPI)
  pinMode(ADS1220_CS_PIN, OUTPUT);
  digitalWrite(ADS1220_CS_PIN, HIGH);
  pinMode(ADS1220_DRDY_PIN, INPUT);

  SPI_2.begin();
  SPI_2.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));

  ads1220_cmd(CMD_RESET);
  delay(10);

  // --- KONFIGURACJA PRECYZYJNA (20 SPS) ---
  
  // REG 0: 52 (Gain=4)
  ads1220_writeReg(0, 52); 

  // REG 1: 4 (20 SPS, Normal Mode, Continuous)
  // Wartość 4 włącza tryb 20Hz, który ma najlepszy filtr 50Hz/60Hz
  ads1220_writeReg(1, 4);  

  // REG 2: 85 (Ext Ref, filtr 50/60Hz włączony)
  ads1220_writeReg(2, 85); 

  // REG 3: 32 (IDAC1 włączony)
  ads1220_writeReg(3, 32); 

  ads1220_cmd(CMD_START);
  delay(10);
  
  attachInterrupt(digitalPinToInterrupt(ADS1220_DRDY_PIN), drdyIntr, FALLING);

  // 3. ADS1115 (I2C)
  Wire.begin(PB6, PB7);
  if (!ads1.begin(72)) { while(1); }
  if (!ads2.begin(73)) { while(1); }

  // Ustawiamy maksymalną prędkość (860 SPS), żeby nie blokować pętli
  // Chociaż ADS1220 działa wolno i dokładnie, ADS1115 musi działać szybko, 
  // żeby nie "zamrażać" procesora.
  ads1.setDataRate(RATE_ADS1115_860SPS);
  ads2.setDataRate(RATE_ADS1115_860SPS);

  Serial.println("System READY");
}

/* =================================================================================
 * LOOP
 * ================================================================================= */
void loop() {
  mb.task();

  // Aktualizacja rejestrów Modbus
  mb.Hreg(MB_REG_R0, val_R0);
  mb.Hreg(MB_REG_R1, val_R1);
  mb.Hreg(MB_REG_R2, val_R2);
  mb.Hreg(MB_REG_R3, val_R3);
  mb.Hreg(MB_REG_TEMP_HI, (uint16_t)(val_Temp32 >> 16));
  mb.Hreg(MB_REG_TEMP_LO, (uint16_t)(val_Temp32 & 0xFFFF));

  // --- ODCZYT TEMPERATURY (RAW - BEZ UŚREDNIANIA) ---
  if (drdyIntrFlag) {
    drdyIntrFlag = false;
    
    int32_t raw_adc = ads1220_readData();
    
    if (raw_adc > 0 && raw_adc < 8388592) {
       // Przeliczenie natychmiastowe
       float r_now = ((float)raw_adc * R_REF) / (8388607.0 * 4.0);
       float t_now = (r_now - R_0) / 3.85;
       t_now += TEMP_OFFSET;
       
       // Zapis wyniku bezpośrednio do rejestru (x1000)
       val_Temp32 = (int32_t)(t_now * 1000.0);
    }
  }

  // --- ODCZYT NAPIĘĆ ---
  if (millis() - last_ads1115_read >= ADS1115_INTERVAL) {
    last_ads1115_read = millis();

    float v;
    v = ads1.readADC_Differential_0_1() * 6.144 / 32767.0;
    val_R0 = (int16_t)(v * 1000);
    v = ads1.readADC_Differential_2_3() * 6.144 / 32767.0;
    val_R1 = (int16_t)(v * 1000);
    v = ads2.readADC_Differential_0_1() * 6.144 / 32767.0;
    val_R2 = (int16_t)(v * 1000);
    v = ads2.readADC_Differential_2_3() * 6.144 / 32767.0;
    val_R3 = (int16_t)(v * 1000);
  }
}