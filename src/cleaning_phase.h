#ifndef CLEANING_PHASE_H
#define CLEANING_PHASE_H

#include <stdint.h>

bool isValidCleaningPhase(uint8_t phase);
uint8_t cleaningPhaseAfterCommand(uint8_t currentPhase, bool acknowledged);

#endif
