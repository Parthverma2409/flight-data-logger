/*
 * GROUND STATION - ESP32 (or Arduino Nano/Uno with a 3.3V-safe NRF24 adapter)
 * Receives TelemetryPacket over NRF24L01+ and prints the same CSV format the
 * flight logger prints over USB, so flightdeck.html works against either side.
 *
 * Wiring is identical to the logger: MOSI=23, MISO=19, SCK=18, CSN=5, CE=4.
 * The TelemetryPacket struct MUST stay byte-identical to flight_logger.ino.
 */

#include <SPI.h>
#include <RF24.h>

#define RF_CE_PIN    4
#define RF_CSN_PIN   5
#define SERIAL_BAUD  115200
const uint64_t PIPE_ADDR = 0xF0F0F0F0E1LL;

RF24 radio(RF_CE_PIN, RF_CSN_PIN);

#pragma pack(push, 1)
struct TelemetryPacket {
  uint32_t timestamp;
  int16_t  ax, ay, az;   // g x 1000
  int16_t  gx, gy, gz;   // deg/s
  float    altitude;     // m
  int16_t  temperature;  // degC x 100
  uint16_t packetId;
};
#pragma pack(pop)

unsigned long lastRx = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println(F("=== GROUND STATION ==="));

  if (!radio.begin()) {
    Serial.println(F("ERROR: NRF24 not responding. Check the capacitor and CE/CSN pins."));
    while (1) delay(100);
  }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(sizeof(TelemetryPacket));
  radio.openReadingPipe(1, PIPE_ADDR);
  radio.startListening();

  Serial.println(F("timestamp_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,pressure_pa,temp_c,altitude_m,packet_id"));
}

void loop() {
  if (radio.available()) {
    TelemetryPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    lastRx = millis();

    // pressure is not transmitted (payload budget); back it out of altitude
    // so the CSV column stays populated for the dashboard tile.
    float pressure = 101325.0f * pow(1.0f - pkt.altitude / 44330.0f, 5.255f);

    Serial.printf("%lu,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.0f,%.2f,%.2f,%u\n",
                  (unsigned long)pkt.timestamp,
                  pkt.ax / 1000.0f, pkt.ay / 1000.0f, pkt.az / 1000.0f,
                  (float)pkt.gx, (float)pkt.gy, (float)pkt.gz,
                  pressure,
                  pkt.temperature / 100.0f,
                  pkt.altitude,
                  pkt.packetId);
  }

  if (millis() - lastRx > 3000) {
    lastRx = millis();
    Serial.println(F("[GS] no packets for 3s"));
  }
}
