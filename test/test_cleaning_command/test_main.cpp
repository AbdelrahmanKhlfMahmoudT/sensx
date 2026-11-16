#include "cleaning_command.h"
#include "cleaning_phase.h"

#include <stdio.h>
#include <string.h>

namespace
{
int failures = 0;
int testsRun = 0;

void expect(bool condition, const char *description)
{
  ++testsRun;
  if (!condition)
  {
    fprintf(stderr, "FAIL: %s\n", description);
    ++failures;
  }
}
}

int main()
{
  CleaningCommand command = {};
  expect(createCleaningCommand(
             1, 0, 2, 0, "ccw", "cleaning", &command),
         "builds a cleaning command");
  expect(command.fan1 == 1 && command.fan2 == 0 &&
             command.fan3 == 2 && command.fan4 == 0,
         "copies all fan values");
  expect(command.messageId == 0,
         "uses default message id when none is provided");
  expect(strcmp(command.direction, "ccw") == 0,
         "copies counterclockwise direction");
  expect(strcmp(command.state, "cleaning") == 0,
         "copies state");

  expect(createCleaningCommand(
             0, 1, 0, 2, "cw", "cleaning", &command, 42) &&
             strcmp(command.direction, "cw") == 0,
         "accepts clockwise direction");
  expect(command.messageId == 42,
         "copies message id into command");
  expect(!createCleaningCommand(
             1, 0, 2, 0, "clockwise", "cleaning", &command),
         "rejects unsupported direction");
  expect(!createCleaningCommand(
             1, 0, 2, 0, nullptr, "cleaning", &command),
         "rejects null direction");
  expect(!createCleaningCommand(
             1, 0, 2, 0, "ccw", nullptr, &command),
         "rejects null state");
  expect(!createCleaningCommand(
             1, 0, 2, 0, "ccw", "cleaning", nullptr),
         "rejects null output pointer");
  expect(!createCleaningCommand(
             1, 0, 2, 0, "ccw", "", &command),
         "rejects empty state");
  expect(!createCleaningCommand(
             1, 0, 2, 0, "ccw", "1234567890123456", &command),
         "rejects state that does not fit payload");
  expect(createCleaningCommand(
             1, 0, 2, 0, "ccw", "123456789012345", &command) &&
             strcmp(command.state, "123456789012345") == 0,
         "accepts maximum state length");

  CleaningAcknowledgement acknowledgement = {};
  createCleaningAcknowledgement(42, &acknowledgement);
  expect(acknowledgement.signature == 0xCA1E &&
             acknowledgement.messageId == 42,
         "creates matching acknowledgement");

  expect(isValidCleaningPhase(0) && isValidCleaningPhase(3),
         "accepts valid phase values");
  expect(!isValidCleaningPhase(4) && !isValidCleaningPhase(255),
         "rejects invalid stored phase values");
  expect(cleaningPhaseAfterCommand(1, true) == 2,
         "advances phase after acknowledged command");
  expect(cleaningPhaseAfterCommand(1, false) == 1,
         "does not advance phase without acknowledgement");
  expect(cleaningPhaseAfterCommand(0, true) == 0,
         "does not change unrelated phase after acknowledgement");

  if (failures != 0)
  {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }

  printf("All %d cleaning command and phase tests passed.\n", testsRun);
  return 0;
}
