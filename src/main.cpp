#include <Arduino.h>
#include "cleaning.h"
#include "cleaning_phase.h"
#include "rtc_storage.h"

const uint8_t CLEANING_PEER_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const int MQ135_PIN = 34;

uint32_t timeStampsClean[CLEANING_TIMESTAMP_COUNT] = {};
int sensorReadingsClean[CLEANING_READING_COUNT] = {};

uint32_t timeStampsDirty[CLEANING_TIMESTAMP_COUNT] = {};
int sensorReadingsDirty[CLEANING_READING_COUNT] = {};

// Hardware & Calibration Constants
const float RL = 22.0;       
const float R0 = 2.8;              // Clean air resistance (kOhm)

const float VOLTAGE_DIVIDER_RATIO = 1.51515; 

const float A = 102.2; 
const float B = -2.473;


int readSensorData();
bool rtcStorageReady = false;

void setup() 
{
  Serial.begin(115200); 
  rtcStorageReady = beginRtcStorage();
  if (!rtcStorageReady)
  {
    Serial.println("RTC initialization failed.");
  }
  pinMode(MQ135_PIN, INPUT);
}

void loop() 
{
  if (!rtcStorageReady)
  {
    return;
  }

  uint8_t currentPhase = 0;
  if (!loadCleaningPhase(&currentPhase))
  {
    Serial.println("Failed to read cleaning phase from EEPROM.");
    delay(1000);
    return;
  }

  if (!isValidCleaningPhase(currentPhase))
  {
    Serial.printf("Invalid phase value %u; resetting to phase 0.\n", currentPhase);
    if (!saveCleaningPhase(0))
    {
      Serial.println("Failed to reset cleaning phase in EEPROM.");
      delay(1000);
      return;
    }
    currentPhase = 0;
  }

  switch (currentPhase)
  {
      case 0:
        // Before cleaning
        for (int y = 0; y < 60; y += 10)
        {
            for (int x = 0; x < 10; x++) 
            {
              int i = y + x;
              sensorReadingsDirty[i] = (int)(readSensorData());
              unsigned long startMillis = millis();
              while (millis() - startMillis < 1000) 
              {
                yield(); // Keeps ESP32 background tasks and watchdog timer happy
              }
            }
            if (!getRtcTimestamp(&timeStampsDirty[y / 10]))
            {
              Serial.println("Failed to read timestamp from RTC.");
              return;
            }
        } 
        
        if (!saveDirtyMeasurements(sensorReadingsDirty, timeStampsDirty))
        {
          Serial.println("Failed to save pre-cleaning measurements to EEPROM.");
          delay(1000);
          break;
        }
        if (!saveCleaningPhase(1))
        {
          Serial.println("Failed to save cleaning phase to EEPROM.");
          delay(1000);
        }
      break;  
      
      case 1:
        // Start cleaning: fan1=1, fan2=0, fan3=2, fan4=0, counterclockwise.
        {
          const esp_err_t initResult = beginCleaningEspNow(CLEANING_PEER_MAC);
          if (initResult != ESP_OK)
          {
            Serial.printf("ESP-NOW initialization failed: %s\n", esp_err_to_name(initResult));
          }
          else
          {
            const esp_err_t sendResult = sendCleaningCommand(
                1, 0, 2, 0, "ccw", "cleaning");
            const uint8_t nextPhase = cleaningPhaseAfterCommand(
                currentPhase, sendResult == ESP_OK);
            if (nextPhase != currentPhase)
            {
              if (!saveCleaningPhase(nextPhase))
              {
                Serial.println("Failed to save cleaning phase to EEPROM.");
              }
            }
            if (sendResult != ESP_OK)
            {
              Serial.printf("Cleaning command send failed: %s\n", esp_err_to_name(sendResult));
              delay(1000);
            }
          }
        }
      break;
      
      case 2:
        // After cleaning
        for (int y = 0; y < 60; y += 10)
        {
            for (int x = 0; x < 10; x++) 
            {
              int i = y + x;
              sensorReadingsClean[i] = (int)(readSensorData());
              unsigned long startMillis = millis();
              while (millis() - startMillis < 1000) 
              {
                yield(); // Keeps ESP32 background tasks and watchdog timer happy
              }
            }
            if (!getRtcTimestamp(&timeStampsClean[y / 10]))
            {
              Serial.println("Failed to read timestamp from RTC.");
              return;
            }
        }
        
        if (!loadDirtyMeasurements(sensorReadingsDirty, timeStampsDirty))
        {
          Serial.println("Failed to load pre-cleaning measurements from EEPROM.");
          delay(1000);
          break;
        }
        if (!saveCleaningPhase(3))
        {
          Serial.println("Failed to save cleaning phase to EEPROM.");
          delay(1000);
        }
      break;
      case 3:
        

        if (!saveCleaningPhase(0))
        {
          Serial.println("Failed to save cleaning phase to EEPROM.");
          delay(1000);
        }
      break;

  }
}


int readSensorData()
{
  int raw_adc = analogRead(MQ135_PIN);
  
  float V_esp = raw_adc * (3.3 / 4095.0);
  float Vout = V_esp * VOLTAGE_DIVIDER_RATIO;

  if (Vout <= 0.05) Vout = 0.05;
  if (Vout >= 4.95) Vout = 4.95;

  float Rs = RL * ((5.0 - Vout) / Vout);
  float ratio = Rs / R0;
  float ppm = A * pow(ratio, B);
  
  return (int)ppm;
}
