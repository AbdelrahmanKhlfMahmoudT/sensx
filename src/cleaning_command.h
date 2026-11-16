#ifndef CLEANING_COMMAND_H
#define CLEANING_COMMAND_H

#include <stdint.h>

struct CleaningCommand
{
  uint8_t fan1;
  uint8_t fan2;
  uint8_t fan3;
  uint8_t fan4;
  char direction[4];
  char state[16];
  uint16_t messageId;
};

struct CleaningAcknowledgement
{
  uint16_t signature;
  uint16_t messageId;
};

bool createCleaningCommand(
    uint8_t fan1,
    uint8_t fan2,
    uint8_t fan3,
    uint8_t fan4,
    const char *direction,
    const char *state,
    CleaningCommand *command,
    uint16_t messageId = 0);

void createCleaningAcknowledgement(
    uint16_t messageId,
    CleaningAcknowledgement *acknowledgement);

#endif
