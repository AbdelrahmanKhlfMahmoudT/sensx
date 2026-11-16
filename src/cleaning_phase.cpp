#include "cleaning_phase.h"

bool isValidCleaningPhase(uint8_t phase)
{
  return phase <= 3;
}

uint8_t cleaningPhaseAfterCommand(uint8_t currentPhase, bool acknowledged)
{
  if (currentPhase == 1 && acknowledged)
  {
    return 2;
  }
  return currentPhase;
}
