# Changelog

## v0.3.0

- add emulation mode for use with Siglent VNAs
- update to Pico SDK 2.1.1 (improves USB stability)
- fix intermittent deadlock during USB communication
- fix timing of CS pin to flash chip which can lead to failed writes
- accept \\r\\n and \\n as line endings for SCPI commands 

## v0.2.3

- minor error recovery improvements for USB connection problems
- add option to write factory coefficients (either from file or from server)
- format partition when deleting factory coefficients

## v0.2.2

- Replace forward slahes (`/`) in the serial number with `_` to fix factory calibration problems

## v0.2.1

First public release
