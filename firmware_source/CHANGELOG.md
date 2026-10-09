# Modern Reader change log

Modern Reader is based on Diptyx firmware 1.0.2. This log lists the changes in each release, newest first. The same history is available on the reader under Settings -> Firmware info.

## 1.0.2

### Library
- Added **Find books** to search titles, authors and series, with filters for all books, favorites or unread books.
- Opening a search result keeps the book in its usual library group.

### Reading and setup
- Chapter-list hints now name the physical controls, including double-tap scrolling.
- A short controls guide appears on first start and can be opened again from Settings.

### USB file transfer
- The transfer screen shows when the reader is waiting for a cable, connected, or ready to unplug.
- The reader recognizes the computer's safe-eject command and warns if the cable is removed first.
- The same transfer steps are used from the menu and at startup.

### Firmware info
- Updated the built-in firmware info book with these release notes.

## 1.0.1

### Reading interface
- Updated the full-screen chapter list to match the Modern Reader menu style: consistent two-level headers on both pages, a selected-position counter, clearer row separators, a rounded selected row, and a visible continuation header on the right page.

### Firmware info
- Rebuilt the on-device firmware-info EPUB from this complete running change log. The version history now flows as one continuous EPUB section instead of several short sections that often left one screen page blank. All earlier release entries remain included.

### Build
- Added the ESP-IDF component lock file so the TinyUSB component resolves to the same version in future builds.

## 1.0.0

### Library
- Group the library by **Title/series** (new default), **Folder** or **Author** (Settings -> Device settings -> Library grouping).
- Series name and volume are read from the EPUB (Calibre series, EPUB 3 collections) and cached with the book data.
- Books are found in the SD card root and in a `Books` folder, including sub-folders. In Folder mode every folder is a group, books outside folders are "Unsorted".
- Series are sorted by volume number: file name first (`03 - Title`, `Vol 3`, `Book 3`, `#3`, `v03`), then the book's own series index, then the title. Books without a number come last. Natural number order (2 before 10). Leading articles (The, A, An) are ignored for the A-Z groups.
- Favorites are always listed first.
- "Now reading" card and a recent books list (newest first).

### Interface
- New card-based menu design: icon tiles, scrollbar, dithered header, footer with button hints (Back, and Fav on books only).
- New "modern reader" wordmark on the main menu (`include/brandLogo.h`, generated from Inter Display).
- Info panel on the right page: library overview, group contents, book details (cover, author, series, progress, favorite), setting descriptions.
- Right button toggles favorite on a book.
- Manual and Firmware info moved into Settings. Firmware info is now a real page with fork notice, credits and this change log.
- Double tap Up/Down to jump a page in long lists.

### Reading
- Quick menu: full-screen chapter list, toggles (dark mode, sunlight mode, pages/percentage), Library entry.
- Turning back from the first page, or forward past the last page, returns to the library.
- A held Center button no longer opens and immediately closes the quick menu.

### Pictures
- **Progressive JPEG support** (`src/progressiveJpeg.cpp`): most commercial EPUBs use progressive JPEGs, which the old decoder (TJpgDec, baseline only) could not read. The new decoder reads only the brightness channel (the screen is black and white) and rebuilds the picture at a reduced size, so a 1404 x 2000 picture needs about 1 MB of RAM. Tested against real book illustrations: mean difference to a full decode is about 2-3 grey levels at reduced size, exact at full size.
- Pictures without an explicit size that are larger than the page are now shrunk proportionally (they used to be squeezed to 480 x 648).
- Picture file extensions are matched case-insensitively (.JPG, .PNG), and PNG covers are accepted.

### File transfer and stability
- Fixed a crash when entering USB transfer mode: `tinyusb_console_deinit()` tried to restore a UART console that does not exist and left stdout/stderr NULL.
- 64 GB (SDXC) cards: card initialisation is retried at lower bus speeds.
- The USB driver no longer auto-mounts the SD card inside its own task; the TinyUSB task has a larger stack.
- A failed transfer shows the failing stage and error code on screen.
- After a brownout, crash or watchdog reset the device shows it at start-up, with the last transfer step it reached.

### Other
- Renamed to Modern Reader (also the USB product/manufacturer string).
- Defaults changed: library grouping is Title/series.
- Build pinned to PlatformIO `espressif32@6.13.0` and `esp_littlefs` v1.20.4 for reproducible builds.
- Manual rewritten for the new features and branded as Modern Reader, with credits to the original authors.
