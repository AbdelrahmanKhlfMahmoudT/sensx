#include "rtc_storage.h"

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

namespace
{
constexpr uint8_t EEPROM_ADDRESS = 0x57;
constexpr uint16_t DIRTY_READINGS_START = 0;
constexpr uint16_t DIRTY_TIMESTAMPS_START = 120;
constexpr uint16_t STATUS_FLAG_ADDRESS = 140;
constexpr uint8_t READING_SIZE_BYTES = 2;
constexpr uint8_t TIMESTAMP_SIZE_BYTES = 3;

RTC_DS3231 rtc;
bool rtcReady = false;

bool writeEeprom(uint16_t address, const uint8_t *data, uint8_t length)
{
  Wire.beginTransmission(EEPROM_ADDRESS);
  if (Wire.write(static_cast<uint8_t>(address >> 8)) != 1 ||
      Wire.write(static_cast<uint8_t>(address & 0xFF)) != 1)
  {
    Wire.endTransmission();
    return false;
  }

  for (uint8_t i = 0; i < length; ++i)
  {
    if (Wire.write(data[i]) != 1)
    {
      Wire.endTransmission();
      return false;
    }
  }

  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  delay(5);
  return true;
}

bool readEeprom(uint16_t address, uint8_t *data, uint8_t length)
{
  Wire.beginTransmission(EEPROM_ADDRESS);
  if (Wire.write(static_cast<uint8_t>(address >> 8)) != 1 ||
      Wire.write(static_cast<uint8_t>(address & 0xFF)) != 1)
  {
    Wire.endTransmission();
    return false;
  }
  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  const uint8_t received =
      Wire.requestFrom(EEPROM_ADDRESS, static_cast<uint8_t>(length));
  if (received != length)
  {
    while (Wire.available())
    {
      Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < length; ++i)
  {
    if (!Wire.available())
    {
      return false;
    }
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}
}

bool beginRtcStorage()
{
  Wire.begin();
  rtcReady = rtc.begin();
  return rtcReady;
}

bool getRtcTimestamp(uint32_t *timestamp)
{
  if (!rtcReady || timestamp == nullptr)
  {
    return false;
  }

  const DateTime now = rtc.now();
  *timestamp = static_cast<uint32_t>(now.day()) * 10000UL +
      static_cast<uint32_t>(now.hour()) * 100UL +
      static_cast<uint32_t>(now.minute());
  return true;
}

bool adjustRtcToBuildTime()
{
  if (!rtcReady)
  {
    return false;
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  return true;
}

bool saveDirtyMeasurements(
    const int readings[CLEANING_READING_COUNT],
    const uint32_t timestamps[CLEANING_TIMESTAMP_COUNT])
{
  if (readings == nullptr || timestamps == nullptr)
  {
    return false;
  }

  for (uint8_t i = 0; i < CLEANING_READING_COUNT; ++i)
  {
    const uint16_t value = static_cast<uint16_t>(readings[i]);
    const uint8_t bytes[] = {
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF)};
    if (!writeEeprom(
            DIRTY_READINGS_START + i * READING_SIZE_BYTES,
            bytes,
            sizeof(bytes)))
    {
      return false;
    }
  }

  for (uint8_t i = 0; i < CLEANING_TIMESTAMP_COUNT; ++i)
  {
    const uint32_t value = timestamps[i];
    const uint8_t bytes[] = {
        static_cast<uint8_t>(value >> 16),
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF)};
    if (!writeEeprom(
            DIRTY_TIMESTAMPS_START + i * TIMESTAMP_SIZE_BYTES,
            bytes,
            sizeof(bytes)))
    {
      return false;
    }
  }

  return true;
}

bool loadDirtyMeasurements(
    int readings[CLEANING_READING_COUNT],
    uint32_t timestamps[CLEANING_TIMESTAMP_COUNT])
{
  if (readings == nullptr || timestamps == nullptr)
  {
    return false;
  }

  for (uint8_t i = 0; i < CLEANING_READING_COUNT; ++i)
  {
    uint8_t bytes[READING_SIZE_BYTES] = {};
    if (!readEeprom(
            DIRTY_READINGS_START + i * READING_SIZE_BYTES,
            bytes,
            sizeof(bytes)))
    {
      return false;
    }
    readings[i] = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  }

  for (uint8_t i = 0; i < CLEANING_TIMESTAMP_COUNT; ++i)
  {
    uint8_t bytes[TIMESTAMP_SIZE_BYTES] = {};
    if (!readEeprom(
            DIRTY_TIMESTAMPS_START + i * TIMESTAMP_SIZE_BYTES,
            bytes,
            sizeof(bytes)))
    {
      return false;
    }
    timestamps[i] =
        (static_cast<uint32_t>(bytes[0]) << 16) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        bytes[2];
  }

  return true;
}

bool saveCleaningPhase(uint8_t phase)
{
  return writeEeprom(STATUS_FLAG_ADDRESS, &phase, sizeof(phase));
}

bool loadCleaningPhase(uint8_t *phase)
{
  if (phase == nullptr)
  {
    return false;
  }
  return readEeprom(STATUS_FLAG_ADDRESS, phase, sizeof(*phase));
}
