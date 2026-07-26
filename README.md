# OnePage Reader — Firmware

> **Based on [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)** (MIT © Dave Allie).
> This is an **independent port** of CrossPoint to the self-designed **OnePage** e-reader (**ESP32-C61**).
> Upstream targets the ESP32-C3 Xteink X4/X3; this repo adapts the firmware to the OnePage C61 hardware.
> Not affiliated with the CrossPoint project or Xteink.

CrossPoint is open-source e-reader firmware — community-built, fully hackable, free forever. This port keeps that engine and adapts it to the OnePage device.

**Runs on:** the self-designed **OnePage** reader (ESP32-C61). Part of the OnePage open-hardware project — firmware · board (PCB) · 3D case · web. See the [OnePage main repo](https://github.com/MoveCall/onepage-reader) *(link TBD)* for hardware and 3D files.

## What this port adds / changes (vs upstream CrossPoint)

- **ESP32-C61 support** — new `onepage` PlatformIO env + board def, board-support SDK (`open-onepage-sdk`), `#ifdef ONEPAGE_C61` guards (~19 core files).
- **Simplified & Traditional Chinese UI** — added i18n translations + CJK SD-card font fallback.
- **Bluetooth page-turner** — BLE HID host that learns any remote's buttons.
- **Power optimization** — BLE-active idle current 42 → 23 mA (controller modem-sleep + 32.768 kHz sleep clock + CPU DFS). See [docs/bluetooth-power-optimization.md](./docs/bluetooth-power-optimization.md).
- **80 MHz flash clock**, OnePage boot logo, in-reader ruled-lines option.

The reader-engine features listed below are inherited from CrossPoint.


## What can CrossPoint do?

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, chapter navigation, footnotes, bookmarks, go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync and more. 

- **Various formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.

- **Screenshots.**

- **Custom fonts**: install your favorite fonts on the SD card.

- **Tilt page turn (X3 only)**.

- **Library workflow**: folder browser, hidden-file toggle, long-press delete, recent books, SD-cache management.

- **Wireless workflows**:
  
  - File transfer web UI
  - EPUB Optimizer
  - Web settings UI/API (edit many device settings from browser)
  - WebSocket fast uploads
  - WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - OTA update checks and installs from GitHub releases

- **Customization**: multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep screen modes, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: 24 UI languages and counting. RTL support.

### Coming soon:

- Dictionary lookup — inline word lookup without leaving the reader.

- More themes.

- Much more! stay tuned.

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool available at
https://crosspointreader.com/#unlock-tool before you can flash CrossPoint.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
> 
> **The only officially supported firmwares in the unlock tool are CrossPoint and CrossInk.**
> 
> Flashing any other firmware on a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path**. Once USB flashing is re-locked, your only way back is via OTA, and if
> the firmware you flashed doesn't support OTA, **there is no way out**.
> 
> **The Papyrix fork has removed OTA update support from its code.** If you flash Papyrix onto a
> USB-locked unit, you will have **zero update or recovery path** and will be stuck on it forever. **Do not flash
> Papyrix (or any other unsupported firmware) on a locked device.**

## Install firmware

> **OnePage (C61) note:** the hosted web installer at crosspointreader.com only serves the upstream **X3/X4 (ESP32-C3)** builds — it does **not** flash OnePage. Build a `firmware.bin` yourself (see [Development quick start](#development-quick-start)) and flash it with the command line below. The web-installer sections that follow are kept for reference to the upstream X-series flow.

### Command line (OnePage / ESP32-C61)

1. Install [`esptool`](https://github.com/espressif/esptool) (`pip install esptool`).
2. Build `firmware.bin` (`~/.platformio/penv/bin/pio run -e onepage`) or take one from CI.
3. Connect via USB-C; find the port (`ls /dev/cu.usbmodem*` on macOS).
4. **Flash depends on the board's SPI-flash vendor** — detect first: `esptool --chip esp32c61 -p <port> flash-id` and read the Manufacturer byte:
   - **Winbond** (`ef`): stub + compression OK, fast (~95 s):
     ```bash
     esptool --chip esp32c61 -p <port> -b 921600 write-flash \
       --flash-mode dio --flash-freq 80m --flash-size 16MB 0x10000 firmware.bin
     ```
   - **Puya** PY25Q128HA (`85`): stub compression crashes — **must** use `--no-stub` (~380 s):
     ```bash
     esptool --chip esp32c61 --no-stub -p <port> -b 115200 write-flash \
       --flash-mode dio --flash-freq 80m --flash-size 16MB 0x10000 firmware.bin
     ```
   Verify success by the "Hash of data verified" line, not esptool's exit code.

<details>
<summary>Upstream X3/X4 (ESP32-C3) install flow — reference</summary>

### Web installer (recommended)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Go to https://crosspointreader.com/#flash-tools, select device (X3 or X4), and choose an official CrossPoint release.

### Web installer (specific version)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download a `firmware.bin` from [Releases](https://github.com/crosspoint-reader/crosspoint-reader/releases), local build, or continuous integration artifact.
3. Go to https://crosspointreader.com/#flash-tools, select device (X3 or X4), click "Custom .bin" and upload a `firmware.bin`.

### Revert to Official Firmware

To revert to the official firmware, you can also flash the latest official firmware using https://crosspointreader.com/#flash-tools.

### Command line

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download `firmware.bin` from the [releases page](https://github.com/crosspoint-reader/crosspoint-reader/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Adjust `/dev/ttyACM0` to match your system.

### Manual

See [Development quick start](#development-quick-start) below.

</details>

---

## Custom SD-card fonts

Convert your own TTF/OTF files into `.cpfont` files that load from the SD card. No firmware reflash is needed.

1. Go to https://crosspointreader.com/fonts and open the "SD-card font builder" form.
2. Upload up to four styles (regular, bold, italic, bold-italic), set the family name, point sizes, and Unicode range.
3. Download the generated `.cpfont` files.
4. Copy them to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
5. Select the font on the device from the font settings.

Conversion runs the firmware repo's `lib/EpdFont/scripts/fontconvert_sdcard.py` script unmodified, so output matches a local host build.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)

---

## Development quick start

### Prerequisites

- [pioarduino](https://github.com/pioarduino/pioarduino) or VS Code + pioarduino plugin
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive git@github.com:MoveCall/crosspoint-onepage.git
cd crosspoint-onepage

# if cloned without --recursive (pulls the onepage-reader-sdk-arduino submodule):
git submodule update --init --recursive
```

### Build / flash / monitor

```bash
# pio must run on Python 3.10+; if your `pio` is on an older Python use the penv one:
~/.platformio/penv/bin/pio run -e onepage            # build (C61)
~/.platformio/penv/bin/pio run -e onepage -t upload  # build + flash
```

To pull selective improvements from upstream CrossPoint (this repo is an independent port, no shared git history — do not `git merge` upstream):

```bash
git remote add upstream https://github.com/crosspoint-reader/crosspoint-reader.git
git fetch upstream
git cherry-pick <commit>   # pick specific board-agnostic reader-engine fixes
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

CrossPoint Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the
cache. This cache directory exists at `.crosspoint` on the SD card. The structure is as follows:

```text
.crosspoint/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   ├── css_rules.cache  # parsed CSS rule cache
│   ├── img_*            # rendered image cache files
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── settings.json        # device settings
├── state.json           # resume/runtime state
└── recent.json          # recent books list
```

Removing `/.crosspoint` clears all cached metadata and forces a full regeneration on next open. Book deletes, overwrites, and moves done through the firmware or web UI clear or re-key matching caches; manual SD-card edits may leave stale cache directories behind.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md). For things to work on, check the [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas) — leave a comment before starting so we don't duplicate effort.

Everyone here is a volunteer, so please be respectful and patient. For governance and community expectations, see [GOVERNANCE.md](./GOVERNANCE.md).

---

## Community forks

One of the best things about open source is that anyone can take the code in a different direction. If you need something outside CrossPoint's [scope](./SCOPE.md), check out the community forks:

- [CrossInk](https://github.com/uxjulia/CrossInk) — Typography and reading tracking: Bionic Reading (bolds word stems to create fixation points), guide dots between words, improved paragraph indents, and replaces the default fonts with ChareInk/Lexend/Bitter.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes via SD card.

- [crosspet](https://github.com/trilwu/crosspet) — A Vietnamese fork that adds a Tamagotchi-style virtual chicken that grows based on your reading milestones (pages read, streaks, care). Also: Flashcards, Weather, Pomodoro timer, and mini-games.

- [crosspoint-reader-cjk](https://github.com/aBER0724/crosspoint-reader-cjk) — Purpose-built for Chinese, Japanese, and Korean reading.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- ~~[PlusPoint](https://github.com/ngxson/pluspoint-reader) — custom JS apps support.~~ (Unmaintained)

- [crosspoint-reader-papers3](https://github.com/juicecultus/crosspoint-reader-papers3) — Crosspoint port for M5Stack Paper S3. 

- [t5s3-reader](https://github.com/ShallowGreen123/t5s3-reader) — Crosspoint port for LilyGo T5 ePaper S3 / T5S3 4.7-inch e-paper device.

**Note:** Many of these features will make their way into CrossPoint over time. We maintain a slower pace to ensure rock-solid stability and squash bugs before they reach your device.

Want to build your own device? Be sure to check out the [de-link](https://github.com/iandchasse/de-link) project.

---

CrossPoint Reader is **not affiliated with Xteink or any device manufacturer**.

Huge shoutout to [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader), which inspired this project.
