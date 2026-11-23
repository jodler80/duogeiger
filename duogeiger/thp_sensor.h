// temperature, humidity, pressure sensor (usually a BME280) related code

#ifndef _THP_SENSOR_H_
#define _THP_SENSOR_H_

// Abfrageintervall des Umweltsensors [ms] (Name stammt noch aus der BME680/BSEC-Zeit).
#define BME680_BSEC_LP_READOUT_INTERVAL 3000  // ms

// Erkennt und initialisiert den Umweltsensor.
bool setup_thp_sensor(void);
// Gibt Sensorbezeichnung für Logs/Web zurück.
const char *get_thp_name(void);
// Liest °C, % rel. Feuchte und Druck in Pa (nicht hPa); IAQ ist immer 0.
bool read_thp_sensor(float *temperature, float *humidity, float *pressure, int *iaq);

#endif // _THP_SENSOR_H_
