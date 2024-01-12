<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Merge changes from [`golioth/reference-design-template@template_v1.2.0`](https://github.com/golioth/reference-design-template/tree/template_v1.2.0).
- Add a `CHANGELOG.md` to track changes moving forward.

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
