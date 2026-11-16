# SENX ESP32-C3 Cleaning Monitor

This project reads an MQ135 air-quality sensor, records readings and timestamps
using an RTC and I2C EEPROM, and sends a fan-control command to another ESP32
over ESP-NOW.

## Project layout

| File | Purpose |
|---|---|
| `platformio.ini` | PlatformIO board, Arduino framework, serial speeds, USB CDC options, and RTClib dependency. |
| `src/main.cpp` | Arduino entry point and application flow: collects sensor readings, manages cleaning phases, and coordinates the RTC storage and ESP-NOW modules. |
| `src/rtc_storage.h` | Public API and array sizes for RTC and EEPROM operations. |
| `src/rtc_storage.cpp` | DS3231 initialization and timestamps, plus I2C EEPROM read/write for dirty readings, timestamps, and phase. |
| `src/cleaning.h` | Public ESP-NOW setup, send, receive-handler, and acknowledgement APIs. |
| `src/cleaning.cpp` | ESP-NOW peer setup, command sending and retry/acknowledgement logic, and receiver callback. |
| `src/cleaning_command.h` | Shared command and acknowledgement packet layouts. Both ESP32s must use matching definitions. |
| `src/cleaning_command.cpp` | Hardware-independent command validation and acknowledgement construction. |
| `src/cleaning_phase.h` | Public phase validation and command transition helpers. |
| `src/cleaning_phase.cpp` | Phase validation and the rule for advancing after an acknowledged cleaning command. |
| `test/test_cleaning_command/test_main.cpp` | Host-side unit tests for command creation, acknowledgements, and phase helpers. |
| `test/README` | Test instructions and receiver-side ESP-NOW setup notes. |

`sensor_storage.cpp` and `sensor_storage.h`, if present in the workspace, are
currently empty and are not used. Sensor reading persistence is implemented in
`rtc_storage.cpp`.

## Application flow

`setup()` starts Serial and initializes the RTC/I2C module with
`beginRtcStorage()`. If initialization fails, the main loop stops rather than
trying to use an unavailable RTC.

The current phase is stored in the external I2C EEPROM and read on each loop:

| Phase | Current behavior |
|---|---|
| `0` | Collect 60 pre-cleaning MQ135 readings, sampling about once per second. Take one timestamp after each group of 10 readings, save all readings and timestamps, then save phase `1`. |
| `1` | Initialize ESP-NOW and send the cleaning command. The sender retries up to three times and waits up to 750 ms per attempt for the matching receiver acknowledgement. Save phase `2` only after acknowledgement; otherwise remain in phase `1` and retry on a later loop. |
| `2` | Collect 60 post-cleaning readings and six timestamps, then load the pre-cleaning readings and timestamps from EEPROM and save phase `3`. |
| `3` | Reset the phase to `0`. |

Invalid phase values are logged and reset to phase `0`.

## Sensor calculation

`readSensorData()` reads `MQ135_PIN`, converts the ADC reading to voltage,
applies the voltage divider ratio, estimates sensor resistance, and calculates
an approximate ppm value using the constants `RL`, `R0`, `A`, and `B` in
`main.cpp`. These constants are calibration-specific and may need adjustment
for the actual sensor and circuit.

## RTC and EEPROM API

Include `rtc_storage.h` to use these functions:

- `beginRtcStorage()` starts I2C and initializes the DS3231.
- `getRtcTimestamp(&value)` returns the current date/time in the compact
  `DDHHMM` format (day, hour, minute). It does not include month or year.
- `adjustRtcToBuildTime()` sets the DS3231 to the firmware build date/time.
  This is not called automatically; use it only when intentionally setting
  the RTC, then remove or disable that call so the clock is not reset on every
  reboot.
- `saveDirtyMeasurements(readings, timestamps)` stores 60 readings and six
  timestamps.
- `loadDirtyMeasurements(readings, timestamps)` retrieves those values.
- `saveCleaningPhase(phase)` and `loadCleaningPhase(&phase)` persist the
  application phase.

The current EEPROM layout is:

| Address range | Contents |
|---|---|
| `0-119` | 60 pre-cleaning readings, 2 bytes each, big-endian. |
| `120-137` | Six timestamps, 3 bytes each, big-endian. |
| `140` | One-byte phase value. |

The DS3231 I2C address is normally `0x68`; the EEPROM address used by the
code is `0x57`. They share the I2C bus. Confirm these addresses and the
EEPROM's capacity/addressing behavior against the actual modules.

## ESP-NOW configuration

In `src/main.cpp`, replace the all-zero `CLEANING_PEER_MAC` with the receiver's
station MAC address. The receiver must initialize ESP-NOW with the sender's
station MAC address instead.

The sender's current command is:

```text
fan1=1, fan2=0, fan3=2, fan4=0, direction="ccw", state="cleaning"
```

The fan values are sent as bytes; the firmware does not currently enforce
application-specific ranges for them. The direction must be `"ccw"` or `"cw"`.
The receiver should include the same `cleaning_command.h` packet definition.

On the receiver, call `beginCleaningEspNow(senderMac)` and register an
application handler with `setCleaningCommandHandler(handler)`. The handler
should apply the fan settings and return `true` only when they were applied;
the module then acknowledges that command. Keep the handler quick because it
runs from the ESP-NOW receive callback. The sender waits for an acknowledgement
with the matching message ID before advancing its phase.

ESP-NOW send and acknowledgement behavior must be tested with two physical
ESP32 boards; host-side unit tests do not test radio communication.

## Build, test, and upload

Run these commands from the project directory in a PlatformIO terminal:

```powershell
pio run
pio run -t upload
pio device monitor
```

The serial monitor speed is `115200`. If upload fails to find the board, set
the appropriate COM port in PlatformIO or use the BOOT button procedure
required by the board.

The command and phase unit tests use a host C++ compiler, so no ESP32 needs to
be connected. From the project directory in PowerShell with `g++` on `PATH`:

```powershell
g++ -std=c++11 -Wall -Wextra -Werror -I src src/cleaning_command.cpp src/cleaning_phase.cpp test/test_cleaning_command/test_main.cpp -o cleaning_command_tests.exe
.\cleaning_command_tests.exe
```

These tests cover command packet creation and validation, acknowledgement
packet creation, phase validation, and the phase-advance rule. They do not
exercise the I2C hardware, sensor, or ESP-NOW radio.

## Hardware checks before running

- **MQ135 input:** `main.cpp` currently sets `MQ135_PIN` to GPIO34. GPIO34 is
  not available on the ESP32-C3. Select an ADC-capable GPIO supported by the
  exact C3 Super Mini board and connect the sensor output to that pin.
- **Voltage levels:** ensure the MQ135 circuit output and voltage divider do
  not exceed the ESP32-C3 ADC input voltage limits.
- **I2C wiring:** connect the RTC and EEPROM to the board's I2C SDA/SCL pins
  and common ground. Verify module addresses and voltage compatibility.
- **Receiver MAC:** replace the sender's all-zero peer MAC before attempting
  ESP-NOW transmission.
- **EEPROM initialization:** an uninitialized phase byte may read as `255`;
  the application treats invalid values as phase `0`. Ensure address `140`
  does not overlap another stored value.
- **Post-cleaning data:** post-cleaning readings and timestamps are currently
  kept only in RAM. The code does not yet save them to EEPROM or compare them
  with the pre-cleaning readings; phase `3` currently just restarts the
  workflow.
