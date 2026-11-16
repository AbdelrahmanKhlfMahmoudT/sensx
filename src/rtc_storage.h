#ifndef RTC_STORAGE_H
#define RTC_STORAGE_H

#include <stdint.h>

constexpr uint8_t CLEANING_READING_COUNT = 60;
constexpr uint8_t CLEANING_TIMESTAMP_COUNT = 6;

bool beginRtcStorage();
bool getRtcTimestamp(uint32_t *timestamp);
bool adjustRtcToBuildTime();

bool saveDirtyMeasurements(
    const int readings[CLEANING_READING_COUNT],
    const uint32_t timestamps[CLEANING_TIMESTAMP_COUNT]);
bool loadDirtyMeasurements(
    int readings[CLEANING_READING_COUNT],
    uint32_t timestamps[CLEANING_TIMESTAMP_COUNT]);

bool saveCleaningPhase(uint8_t phase);
bool loadCleaningPhase(uint8_t *phase);

#endif
