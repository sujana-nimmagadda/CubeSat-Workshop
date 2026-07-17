#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <LiquidCrystal_I2C.h>

#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "CUBES";

LiquidCrystal_I2C lcd(0x27, 20, 4);

struct Telemetry {
  float temperature;
  float humidity;
  float pressure_hpa;
  float altitude_m;
  int   azimuth_deg;
};

// Signal strength tracking
unsigned long lastPacketTime = 0;
int signalStrength = 0; // 0–5

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(50);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Ground Station");

  if (!radio.begin()) {
    Serial.println("nRF24 init FAILED");
    while (1) delay(500);
  }

  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(80);
  radio.setRetries(5, 15);
  radio.setCRCLength(RF24_CRC_16);
  radio.enableDynamicPayloads();

  radio.openReadingPipe(0, address);
  radio.startListening();

  Serial.println("RX ready");
}

void printToLCD(const Telemetry &t) {
  lcd.clear();

  // Line 1
  lcd.setCursor(0,0);
  lcd.print("GNU Ground Station");

  // Line 2 → Temp + Humidity
  lcd.setCursor(0,1);
  lcd.print("T:");
  if (!isnan(t.temperature)) lcd.print(t.temperature,1);
  else lcd.print("N/A");
  lcd.print((char)223); lcd.print("C ");

  lcd.print("RH:");
  if (!isnan(t.humidity)) lcd.print(t.humidity,0);
  else lcd.print("N/A");
  lcd.print("%");

  // Line 3 → Pressure + Altitude
  lcd.setCursor(0,2);
  lcd.print("P:");
  if (!isnan(t.pressure_hpa)) lcd.print(t.pressure_hpa,0);
  else lcd.print("N/A");
  lcd.print("hPa ");

  lcd.print("A:");
  if (!isnan(t.altitude_m)) lcd.print(t.altitude_m,2);
  else lcd.print("N/A");
  lcd.print("m");

  // Line 4 → Azimuth + Direction + Signal
  lcd.setCursor(0,3);
  lcd.print("Az:");
  if (t.azimuth_deg >= 0) lcd.print(t.azimuth_deg);
  else lcd.print("N/A");

  lcd.print(" ");

  // Direction
  int az = t.azimuth_deg;
  if (az < 0) lcd.print("N/A");
  else if ((az >= 337) || (az < 23)) lcd.print("N");
  else if (az < 68) lcd.print("NE");
  else if (az < 113) lcd.print("E");
  else if (az < 158) lcd.print("SE");
  else if (az < 203) lcd.print("S");
  else if (az < 248) lcd.print("SW");
  else if (az < 293) lcd.print("W");
  else lcd.print("NW");

  // Signal strength bars
  lcd.print(" S:");
  for (int i = 0; i < signalStrength; i++) {
    lcd.print("|");
  }
}

void loop() {
  if (radio.available()) {
    Telemetry t;
    radio.read(&t, sizeof(Telemetry));

    // ---- Signal strength estimation ----
    unsigned long now = millis();
    unsigned long gap = now - lastPacketTime;
    lastPacketTime = now;

    if (gap < 1200) signalStrength = 5;
    else if (gap < 1500) signalStrength = 4;
    else if (gap < 2000) signalStrength = 3;
    else if (gap < 3000) signalStrength = 2;
    else signalStrength = 1;

    // ---- Serial output ----
    Serial.print("T: "); 
    if (!isnan(t.temperature)) Serial.print(t.temperature,1); else Serial.print("N/A");
    Serial.print("C\tRH: ");
    if (!isnan(t.humidity)) Serial.print(t.humidity,1); else Serial.print("N/A");
    Serial.print("%\tP: ");
    if (!isnan(t.pressure_hpa)) Serial.print(t.pressure_hpa,2); else Serial.print("N/A");
    Serial.print("hPa\tAlt: ");
    if (!isnan(t.altitude_m)) Serial.print(t.altitude_m,2); else Serial.print("N/A");
    Serial.print(" m\tAz: ");
    if (t.azimuth_deg >= 0) Serial.print(t.azimuth_deg); else Serial.print("N/A");
    Serial.println();

    // ---- LCD update ----
    printToLCD(t);
  }

  delay(50);
}
