// temperature, humidity, pressure sensor (BME280) related code
// BME680 with BSEC/IAQ removed for simplicity; iaq is always 0.

#include <Arduino.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include "log.h"
#include "thp_sensor.h"

static int type_thp = 0;
static Adafruit_BME280 bme280;

bool setup_thp_sensor(void) {
  if (bme280.begin(0x76) || bme280.begin(0x77))
    type_thp = 280;

  switch (type_thp) {
    case 280: log(INFO, "BME_Status: ok, ID: BME280"); break;
    default:  log(INFO, "BME_Status: not found"); break;
  }
  return (type_thp > 0);
}

const char *get_thp_name(void) {
  return (type_thp == 280) ? "BME280" : "none";
}

bool read_thp_sensor(float *temperature, float *humidity, float *pressure, int *iaq) {
  *iaq = 0;
  if (type_thp != 280) {
    *temperature = 0.0;
    *humidity    = 0.0;
    *pressure    = 0.0;
    return false;
  }
  *temperature = bme280.readTemperature();
  *humidity    = bme280.readHumidity();
  *pressure    = bme280.readPressure();
  return true;
}
