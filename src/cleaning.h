#ifndef CLEANING_H
#define CLEANING_H

#include <esp_err.h>
#include <stdint.h>
#include "cleaning_command.h"

using CleaningCommandHandler = bool (*)(const CleaningCommand &command);

esp_err_t beginCleaningEspNow(const uint8_t peerMac[6]);
void setCleaningCommandHandler(CleaningCommandHandler handler);
esp_err_t sendCleaningCommand(
    uint8_t fan1,
    uint8_t fan2,
    uint8_t fan3,
    uint8_t fan4,
    const char *direction,
    const char *state);

esp_err_t sendCleaningAcknowledgement(
    const uint8_t receiverMac[6],
    uint16_t messageId);

#endif
