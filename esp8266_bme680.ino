#include <EEPROM.h>
#include "bsec.h"
#include "ESP8266WiFi.h"
#include "TickTwo.h" // sstaub

/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
    4d/28d = Operating age of the sensor in days
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

#define STATE_SAVE_PERIOD  UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day
 
// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);
void loadState(void);
void updateState(void);



 
// Create an object of the class Bsec
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
 
String output = {};


//
void takeMeasurement(void);
TickTwo timerMeasurement(takeMeasurement, 3000, 0, MILLIS); 

 
void setup(void)
{
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1); // 1st address for the length
  Serial.begin(115200);
  Wire.begin(0, 2);
 
  iaqSensor.begin(BME680_I2C_ADDR_PRIMARY, Wire);
  checkIaqSensorStatus();

  iaqSensor.setConfig(bsec_config_iaq);
  checkIaqSensorStatus();

  loadState();

  bsec_virtual_sensor_t sensorList1[5] = {
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
  };

  iaqSensor.updateSubscription(sensorList1, 5, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();
 
  bsec_virtual_sensor_t sensorList2[5] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };
 
  iaqSensor.updateSubscription(sensorList2, 5, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();
  
  timerMeasurement.start();
}
 
// Function that is looped forever
void loop(void)
{ 
  timerMeasurement.update();
}
 
// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK)
  {
    if (iaqSensor.status < BSEC_OK)
    {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
      ESP.restart();
      /* Restart in case of failure */
    }
    else
    {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }
 
  if (iaqSensor.bme680Status != BME680_OK)
  {
    if (iaqSensor.bme680Status < BME680_OK)
    {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
      ESP.restart();
      /* Restart in case of failure */
    }
    else
    {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      //Serial.println(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  /* Set a trigger to save the state. Here, the state is saved every STATE_SAVE_PERIOD with the first state being saved once the algorithm achieves full calibration, i.e. iaqAccuracy = 3 */
  if (stateUpdateCounter == 0) {
    if (iaqSensor.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD milliseconds */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    //Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);
      //Serial.println(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}



double dewPointFast(double celsius, double humidity)
{
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidity * 0.01);
  double Td = (b * temp) / (a - temp);
  return Td;
}

void takeMeasurement(void)
{
   unsigned long time_trigger = millis();
  if (iaqSensor.run()) // If new data is available
  { 
    double dewPoint = (dewPointFast(iaqSensor.temperature, iaqSensor.humidity));
    output = "{";
    output += String("'time_trigger': ") + String(time_trigger);
    output += ", " + String("'rawTemperature': ") + String(iaqSensor.rawTemperature);
    output += ", " + String("'pressure': ") + String(iaqSensor.pressure);
    output += ", " + String("'rawHumidity': ") + String(iaqSensor.rawHumidity);
    output += ", " + String("'gasResistance': ") + String(iaqSensor.gasResistance);
    output += ", " + String("'iaq': ") + String(iaqSensor.iaq);
    output += ", " + String("'iaqAccuracy': ") + String(iaqSensor.iaqAccuracy);
    output += ", " + String("'temperature': ") + String(iaqSensor.temperature);
    output += ", " + String("'humidity': ") + String(iaqSensor.humidity);
    output += ", " + String("'dewPoint': ") + String(dewPoint);
    output += ", " + String("'staticIaq': ") + String(iaqSensor.staticIaq);
    output += ", " + String("'co2Equivalent': ") + String(iaqSensor.co2Equivalent);
    output += ", " + String("'breathVocEquivalent': ") + String(iaqSensor.breathVocEquivalent);
    output += "}";
    
    Serial.println(output);
    
    updateState();        
  } 
  else
  {
    checkIaqSensorStatus();
  }
}
 
