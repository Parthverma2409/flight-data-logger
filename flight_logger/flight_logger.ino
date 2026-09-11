/*
 * FLIGHT DATA LOGGER - ESP32
 * Hardware: ESP32-WROOM-32 + MPU6050 + BMP280 + NRF24L01+ PA+LNA
 * Output:   USB Serial CSV (live dashboard) + NRF24 telemetry
 *
 * Wiring (see README.md):
 *   MPU6050 / BMP280 : SDA=GPIO21, SCL=GPIO22, VCC=3V3
 *   NRF24L01+        : MOSI=23, MISO=19, SCK=18, CSN=5, CE=4, VCC=3V3
 *                      10-100uF cap across NRF24 VCC/GND is mandatory.
 */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include <RF24.h>

// ============ CONFIGURATION ============
#define SAMPLE_RATE_MS   50       // 20 Hz sampling
#define SERIAL_BAUD      115200
#define RADIO_ENABLED    1        // set to 0 if you have no second NRF24 yet
#define BMP_ADDRESS      0x76     // try 0x77 if the BMP280 is not found

// NRF24 pins (ESP32 default VSPI + custom CE/CSN)
#define RF_CE_PIN        4
#define RF_CSN_PIN       5
const uint64_t PIPE_ADDR = 0xF0F0F0F0E1LL;   // must match ground station

#define G_TO_MS2         9.80665f
#define RAD_TO_DPS       57.2957795f

// ============ GLOBALS ============
Adafruit_MPU6050 mpu;
Adafruit_BMP280  bmp;
RF24 radio(RF_CE_PIN, RF_CSN_PIN);

float basePressure = 0;
unsigned long lastSample = 0;
uint32_t packetCount = 0;
bool radioOk = false;

/*
 * Compact telemetry packet.
 * NRF24L01+ hard limit is 32 bytes per payload, so floats are scaled to
 * integers. This struct is 24 bytes. Keep it identical on the ground station.
 */
#pragma pack(push, 1)
struct TelemetryPacket {
  uint32_t timestamp;   // ms since boot
  int16_t  ax, ay, az;  // g    x 1000
  int16_t  gx, gy, gz;  // deg/s x 1  (clamped +/- 32767)
  float    altitude;    // m
  int16_t  temperature; // degC x 100
  uint16_t packetId;    // wraps at 65535
};
#pragma pack(pop)

static int16_t clamp16(float v) {
  if (v >  32767.0f) return  32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println(F("=== FLIGHT DATA LOGGER ==="));

  // --- I2C sensors ---
  Wire.begin(21, 22);            // SDA=21, SCL=22
  Wire.setClock(400000);

  if (!mpu.begin()) {
    Serial.println(F("ERROR: MPU6050 not found. Check wiring."));
    while (1) delay(100);
  }
  Serial.println(F("MPU6050 OK"));
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  if (!bmp.begin(BMP_ADDRESS)) {
    Serial.println(F("ERROR: BMP280 not found. Try changing BMP_ADDRESS to 0x77."));
    while (1) delay(100);
  }
  Serial.println(F("BMP280 OK"));
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_1);

  // --- Baseline altitude ---
  Serial.print(F("Calibrating baseline pressure... "));
  float sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += bmp.readPressure();
    delay(20);
  }
  basePressure = sum / 50.0f;
  Serial.print(basePressure);
  Serial.println(F(" Pa"));

  // --- NRF24 radio ---
#if RADIO_ENABLED
  if (!radio.begin()) {
    Serial.println(F("WARNING: NRF24 not responding. Continuing without radio."));
  } else {
    radio.setPALevel(RF24_PA_LOW);          // raise to RF24_PA_HIGH once the cap is in
    radio.setDataRate(RF24_250KBPS);        // max range
    radio.setPayloadSize(sizeof(TelemetryPacket));
    radio.setRetries(1, 3);                 // do not stall the 20 Hz loop
    radio.openWritingPipe(PIPE_ADDR);
    radio.stopListening();                  // TX mode
    radioOk = true;
    Serial.print(F("NRF24L01 TX ready, payload "));
    Serial.print((int)sizeof(TelemetryPacket));
    Serial.println(F(" bytes"));
  }
#endif

  // --- CSV header for the dashboard ---
  Serial.println(F("timestamp_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,pressure_pa,temp_c,altitude_m,packet_id"));
  Serial.println(F("=== LOGGING STARTED ==="));
}

void loop() {
  unsigned long now = millis();
  if (now - lastSample < SAMPLE_RATE_MS) return;
  lastSample = now;

  // --- Read sensors ---
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Adafruit returns m/s^2 and rad/s. Convert to g and deg/s so the CSV
  // matches its own header and the dashboard g-force readout is correct.
  float ax = accel.acceleration.x / G_TO_MS2;
  float ay = accel.acceleration.y / G_TO_MS2;
  float az = accel.acceleration.z / G_TO_MS2;
  float gx = gyro.gyro.x * RAD_TO_DPS;
  float gy = gyro.gyro.y * RAD_TO_DPS;
  float gz = gyro.gyro.z * RAD_TO_DPS;

  float pressure    = bmp.readPressure();
  float temperature = bmp.readTemperature();
  float altitude    = 44330.0f * (1.0f - pow(pressure / basePressure, 1.0f / 5.255f));

  // --- CSV to serial (live dashboard) ---
  Serial.printf("%lu,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.0f,%.2f,%.2f,%lu\n",
                now, ax, ay, az, gx, gy, gz,
                pressure, temperature, altitude,
                (unsigned long)packetCount);

  // --- Radio ---
#if RADIO_ENABLED
  if (radioOk) {
    TelemetryPacket pkt;
    pkt.timestamp   = now;
    pkt.ax          = clamp16(ax * 1000.0f);
    pkt.ay          = clamp16(ay * 1000.0f);
    pkt.az          = clamp16(az * 1000.0f);
    pkt.gx          = clamp16(gx);
    pkt.gy          = clamp16(gy);
    pkt.gz          = clamp16(gz);
    pkt.altitude    = altitude;
    pkt.temperature = clamp16(temperature * 100.0f);
    pkt.packetId    = (uint16_t)packetCount;

    bool ok = radio.write(&pkt, sizeof(pkt));
    if (!ok && packetCount % 100 == 0) {
      Serial.println(F("[RADIO] TX failed - check ground station / capacitor"));
    }
  }
#endif

  packetCount++;
}
