#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <QMC5883LCompass.h>

#define LED_PIN 7
#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "CUBES";

Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;
QMC5883LCompass compass;

bool bmp_ok = false;
bool aht_ok = false;

const float SEA_LEVEL_HPA = 1013.25;

struct Telemetry {
  float temperature;
  float humidity;
  float pressure_hpa;
  float altitude_m;
  int   azimuth_deg;
};

unsigned long lastBMPReset = 0;
int bmpFailCount = 0;

// 🔥 I2C BUS RESET FUNCTION (KEY FIX)
void resetI2C() {
  Wire.end();
  delay(50);
  Wire.begin();
  Wire.setClock(100000);
  Serial.println("🔄 I2C BUS RESET");
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  Wire.setClock(100000);

  delay(100);

  Serial.println("TX: sensors + nRF24 starting...");

  // BMP init
  if (bmp.begin()) {
    bmp_ok = true;

    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500
    );

    Serial.println("BMP280 OK");
  } else {
    Serial.println("BMP280 NOT FOUND");
  }

  // AHT init
  if (aht.begin()) {
    aht_ok = true;
    Serial.println("AHT20 OK");
  } else {
    Serial.println("AHT NOT FOUND");
  }

  compass.init();
  Serial.println("Compass init called");

  // RF init
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

  radio.openWritingPipe(address);
  radio.stopListening();

  Serial.println("TX ready");
}

void loop() {
  Telemetry t;

  t.temperature = NAN;
  t.humidity = NAN;
  t.pressure_hpa = NAN;
  t.altitude_m = NAN;
  t.azimuth_deg = -1;

  // ---- AHT20 ----
  if (aht_ok) {
    sensors_event_t humE, tempE;
    if (aht.getEvent(&humE, &tempE)) {
      t.temperature = tempE.temperature;
      t.humidity = humE.relative_humidity;
    }
  }

  // ---- BMP280 (STRONG RECOVERY VERSION) ----
  if (bmp_ok) {
    delay(10);

    float p_pa = bmp.readPressure();

    if (p_pa > 30000 && p_pa < 110000) {
      t.pressure_hpa = p_pa / 100.0f;
      t.altitude_m = bmp.readAltitude(SEA_LEVEL_HPA);
      bmpFailCount = 0; // reset fail counter
    } else {
      Serial.println("⚠ BMP FAIL");

      bmpFailCount++;

      // 🔥 FULL RECOVERY
      resetI2C();
      bmp_ok = bmp.begin();

      if (bmp_ok) {
        Serial.println("✅ BMP RECOVERED");
      } else {
        Serial.println("❌ BMP STILL FAILING");
      }
    }
  }

  // ---- WATCHDOG RESET ----
  if (bmpFailCount >= 5) {
    Serial.println("🚨 FORCE SENSOR RESET");

    resetI2C();

    bmp.begin();
    aht.begin();

    bmpFailCount = 0;
  }

  // ---- PERIODIC SAFE RESET ----
  if (millis() - lastBMPReset > 15000) {
    Serial.println("🔄 PERIODIC SENSOR REFRESH");

    resetI2C();
    bmp.begin();

    lastBMPReset = millis();
  }

  // ---- Compass ----
  compass.read();
  byte az = compass.getAzimuth();
  t.azimuth_deg = map((int)az, 0, 255, 0, 359);

  // ---- Send ----
  digitalWrite(LED_PIN, HIGH);

  bool ok = radio.write(&t, sizeof(Telemetry));

  delay(50);
  digitalWrite(LED_PIN, LOW);

  // ---- Serial ----
  Serial.print("TX -> ");
  Serial.print("T: "); Serial.print(isnan(t.temperature) ? NAN : t.temperature, 1);
  Serial.print("C ");
  Serial.print("RH: "); Serial.print(isnan(t.humidity) ? NAN : t.humidity, 1);
  Serial.print("% ");
  Serial.print("P: "); Serial.print(isnan(t.pressure_hpa) ? NAN : t.pressure_hpa, 2);
  Serial.print("hPa ");
  Serial.print("Alt: "); Serial.print(isnan(t.altitude_m) ? NAN : t.altitude_m, 2);
  Serial.print("m ");
  Serial.print("Az: "); Serial.print(t.azimuth_deg);
  Serial.print(" => send: ");
  Serial.println(ok ? "OK" : "FAIL");

  delay(900);
}
