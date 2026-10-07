# Firmware files

## Modern Reader 1.0.0 (full clean install)

Download [`ModernReader_v1.0.0_merged.bin`](ModernReader_v1.0.0_merged.bin) and flash it at **`0x0000`** using an ESP32-S3-compatible flasher. The merged image contains the bootloader, partition table, Modern Reader application, and built-in assets. Its SHA-256 checksum is in [`ModernReader_v1.0.0_merged.bin.sha256`](ModernReader_v1.0.0_merged.bin.sha256).

**This is a clean/full flash. It overwrites the device flash and erases existing settings, book data, and metadata. Back up anything important first.** It is intended only for the Diptyx E-reader hardware.

To enter USB flash mode, turn the device fully off for about 20 seconds, then hold the center joystick while connecting USB-C. Use an ESP32-S3-compatible flasher; browser-based flashers require WebSerial support.

For a source-built application update that preserves existing data, see the [build and flashing instructions](../README.md#build-from-source) in the project README. Flash the app-only `firmware.bin` at `0x10000`; do not use the full merged image when you need to preserve books or settings.

## Inherited upstream files

The `diptyx_firmware_1.0.1*` and `diptyx_firmware_1.0.2*` files in this folder are inherited from the original Diptyx project. They are unchanged upstream binaries and **do not include Modern Reader**. Do not flash them if you are expecting the fork’s features.

The supplied `modern-reader-assets-littlefs.bin` is a 1 MB **assets partition only** (built-in manual and firmware-info EPUBs), not a complete firmware image. Flash it at `0x910000` only when you specifically need to update those assets.

To build Modern Reader, follow the instructions in the [project README](../README.md#build-from-source). The application image is written at `0x10000`; the full merged image is written at `0x0000`. Read the README’s flashing cautions and back up important data before flashing.
