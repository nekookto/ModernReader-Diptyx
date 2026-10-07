# Modern Reader

Modern Reader is a fork of the firmware of the [Diptyx E-reader](https://github.com/MartijndenHoed/Diptyx) by Martijn den Hoed and the Diptyx team,
based on Diptyx firmware 1.0.2. See `CHANGELOG.md` for what changed, `NOTICE.md` for credits and `LICENSE` for the MIT license.
It is not affiliated with or endorsed by Diptyx.

Build: `pio run` (PlatformIO, ESP-IDF). Flash `.pio/build/esp32-s3-devkitm-1/firmware.bin` at address `0x10000` to keep your settings.
The manual and firmware info screens are the EPUB files in `data/` (they live in the assets partition). To update them run `pio run -t buildfs` and flash `.pio/build/esp32-s3-devkitm-1/littlefs.bin` at address `0x910000` (books and settings are not touched).

---

## Firmware source
Here, the firmware source of the Diptyx E-reader can be found. This firmware was developed in VSCODE, with PlatformIO and ESP-IDF.

## License

All original source code in this repository are released under the MIT License.

Copyright © 2026 Diptyx

This means you are free to use, copy, modify, merge, publish, distribute, sublicense, and sell copies of the original work, subject to the conditions of the MIT License.

## Third-party software

This firmware makes use of third-party libraries, frameworks, and other software components. These components are not necessarily covered by the MIT License above. They remain subject to their respective licenses and copyright notices.

Where applicable, the relevant third-party licenses and notices are included in the repository or can be found in the corresponding dependency's source repository.

When redistributing this firmware, please make sure to comply with the licenses of all included third-party components in addition to the MIT License covering the original Diptyx work.