#include "cleaning_command.h"

#include <string.h>

bool createCleaningCommand(
    uint8_t fan1,
    uint8_t fan2,
    uint8_t fan3,
    uint8_t fan4,
    const char *direction,
    const char *state,
    CleaningCommand *command,
    uint16_t messageId)
{
  if (direction == nullptr || state == nullptr || command == nullptr)
  {
    return false;
  }

  const bool isCounterClockwise = strcmp(direction, "ccw") == 0;
  const bool isClockwise = strcmp(direction, "cw") == 0;
  if (!isCounterClockwise && !isClockwise)
  {
    return false;
  }

  const size_t stateLength = strlen(state);
  if (stateLength == 0 || stateLength >= sizeof(command->state))
  {
    return false;
  }

  command->fan1 = fan1;
  command->fan2 = fan2;
  command->fan3 = fan3;
  command->fan4 = fan4;
  memcpy(command->direction, direction, strlen(direction) + 1);
  memcpy(command->state, state, stateLength + 1);
  command->messageId = messageId;
  return true;
}

void createCleaningAcknowledgement(
    uint16_t messageId,
    CleaningAcknowledgement *acknowledgement)
{
  if (acknowledgement == nullptr)
  {
    return;
  }

  acknowledgement->signature = 0xCA1E;
  acknowledgement->messageId = messageId;
}
