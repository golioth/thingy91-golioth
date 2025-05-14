<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.0] - 2025-06-03

### Changed

- Merge changes from
  [`golioth/reference-design-template@template_v2.7.2`](https://github.com/golioth/reference-design-template/tree/template_v2.7.2).
- Removed `CONFIG_GOLIOTH_SAMPLE_SETTINGS` from prj.conf. With Golioth
  Firmware SDK v0.18.1 this symbol is now automatically selected.

### Fixed

- CHANGELOG: use correct date for release v1.5.0

## [1.5.0] - 2025-02-11

### Added

- Unique package names for each board. Use `thingy91` or `thingy91x` when creating OTA packages.

### Changed

- Merge changes from
  [`golioth/reference-design-template@template_v2.6.0`](https://github.com/golioth/reference-design-template/tree/template_v2.6.0).

## [1.4.0] - 2024-12-20

### Added

- Support for Thingy91x

### Changed

- Merge changes from
  [`golioth/reference-design-template@template_v2.5.0`](https://github.com/golioth/reference-design-template/tree/template_v2.5.0).
- Sensor readings moved to their own functions. This facilitates reading different sensors on the
  Thingy91 and Thingy91x

### Known issues

- Thingy91x: The BME68x environment sensor sometimes shows repeated, unchanging values for
  IAQ/VOC/CO2 readings. The same behavior is present in the factory sample from Nordic and believed
  to be an issue with the upstream driver.

## [1.3.0] - 2024-10-03

### Added

- Pipeline example

### Changed

- Merge changes from
  [`golioth/reference-design-template@template_v2.4.1`](https://github.com/golioth/reference-design-template/tree/template_v2.4.1).
- Update board name for Zephyr hardware model v2
- Use `VERSION` file instead of `prj.conf` to set firmware version

## [1.2.0] - 2024-05-03

### Changed

- Merge changes from
  [`golioth/reference-design-template@template_v2.2.1`](https://github.com/golioth/reference-design-template/releases/tag/template_v2.2.1).
  This targets Golioth Firmware SDK v0.13.1.
- The count of the sensor reads (both up and down) which are recorded using LightDB State will now
  wrap around when reaching max/min value.
- LightDB State observations are received as JSON replies from the server instead of CBOR.
- Turn off unused shells (ADXL372, FLASH, SENSOR) to reduce binary size.

## [1.1.0] - 2024-01-12

### Added

- Merge changes from [`golioth/reference-design-template@template_v1.2.0`](https://github.com/golioth/reference-design-template/tree/template_v1.2.0).
- Add a `CHANGELOG.md` to track changes moving forward.
- Add debug logging for counters.

### Fixed

- Fix some checkpatch and formatting issues.

### Changed

- Minor general app cleanup.

### Removed

- Remove unsupported board overlays.
- Remove Aludel Mini-specific battery monitor code.
- Remove references to Ostentus.

## [1.0.0] - 2023-05-02

This is the first release of the `thingy91-golioth` repo.

Binaries include:

- A PDF guide to programming your Thingy91 using nRF Connect for Desktop tools
- An `Thingy91_Golioth_Example_vX.X.X.hex` initial binary image that you will program over USB
- An `app_update_thingy91_vX.X.X.bin` that you should upload to the cloud as your "rollback" image
- An `app_update_thingy91_vX.X.Y.bin` (where Y = X + 1)

To execute an Over-the-Air update, follow the directions on the Golioth docs: [Docs link](https://docs.golioth.io/firmware/zephyr-device-sdk/firmware-upgrade/build-sample-application#4-upload-new-firmware-to-the-golioth-console)
