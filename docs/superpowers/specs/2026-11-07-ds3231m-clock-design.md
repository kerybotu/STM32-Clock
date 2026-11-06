# DS3231M RTC Integration Design

## Intent

Replace the STM32L010F4 internal RTC/LSE time source with an external DS3231M so timekeeping is maintained by the module's compensated MEMS time base and VBAT backup. Document complete wiring for the selected DS3231M 8-SO package.

## Goals

- DS3231M is the sole clock/calendar source for display, PA4 editing, and PA1 one-second toggling.
- Preserve OLED on PA9/PA10 and use a separate software I2C bus on PA5=SCL and PA6=SDA for DS3231M.
- Use 3.3 V pullups on the DS bus, a local VCC decoupling capacitor, and a backup battery on VBAT.
- Retain existing first-start preset 2026-10-18 16:20:00 when DS3231M OSF reports time invalid.
- Keep PA4 editing cached and commit to DS3231M only when leaving the seconds setting.
- Document the 8-SO pinout, optional outputs, supply, pullups, and wiring to STM32 in README.

## Non-Goals

- Do not calibrate the DS3231M with its aging offset register.
- Do not use INT/SQW or 32KHZ as required signals; leave them available but unconnected by default.
- Do not change the OLED bus, button wiring, display layout, or firmware flashing state.
- Do not claim physical timing validation without a connected target and instrument measurement.

## Approaches

- **Dedicated software I2C bus (selected):** OLED remains on PA9/PA10; DS3231M uses PA5/PA6. This matches the user's requested isolation and avoids sharing bus timing and transactions.
- Shared PA9/PA10 bus: fewer wires and distinct addresses, but rejected because the user selected an independent DS3231M bus.

## Implementation

- Add DS3231M byte read/write helpers using 7-bit address 0x68, sequential BCD calendar access, and ACK/NACK handling.
- Read time/date as one seven-byte burst and status separately during initialization.
- Weekday numbering is Monday=1 through Sunday=7; if OSF is set, initialize 2026-10-18 16:20:00 with Sunday=7, then clear OSF and disable unused 32.768 kHz output.
- Route `RTC_ReadSnapshot` and calendar writes through DS3231M. Remove internal RTC/LSE initialization from the active startup path; leave PC14/PC15 unused by firmware.
- Keep SysTick key scanning and the queued OLED display path unchanged except for using DS3231M snapshot fields.
- Add host-side tests for BCD/calendar encoding and register snapshot decoding.

## Hardware Calls

- DS bus pins: PA5 (TSSOP20 pin 11) SCL and PA6 (pin 12) SDA.
- DS3231M 8-SO pinout: 1=32KHZ, 2=VCC, 3=INT/SQW, 4=RST, 5=GND, 6=VBAT, 7=SDA, 8=SCL.
- DS3231M package selection: 8-SO.
- DS3231M 8-SO pinout: 1=32KHZ, 2=VCC, 3=INT/SQW, 4=RST, 5=GND, 6=VBAT, 7=SDA, 8=SCL.
- Pull up DS SCL and SDA separately with 4.7 kOhm resistors to 3.3 V. Do not use 5 V pullups.
- Connect a low-leakage 3 V coin cell positive to VBAT and negative to common GND; connect 100 nF from VCC to GND. If VBAT backup is intentionally unused, connect VBAT to GND.
- Leave 32KHZ and INT/SQW open; do not add pullups unless those outputs are later used.

## Verification

- Host tests cover BCD encoding/decoding, 24-hour fields, weekday/date fields, and invalid BCD rejection.
- Run `make clean all`, `git diff --check`, and inspect vector/build size.
- Attempt target flash only if CMSIS-DAP is detected; report hardware timing as unverified until PA1 is measured.
