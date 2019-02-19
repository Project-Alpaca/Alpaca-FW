# FT-Controller-FW

Teensyduino-based firmware for Project Diva Future Tone controller project based on Teensy 3.x/LC

## Features

- 16 key shift register-based general purpose input (partially remappable, WIP)
- Shift register-based 8 channel programmable lamp driver/power switch (expandable)
- Has a built-in UI for controller settings, no need for a PC (for all runtime settings)
- 1kHz internal scanning rate, timer driven, stable 250Hz (1kHz when compiled with turbo mode enabled) report output
- True slider (although it uses resistive sensing and feels different than official)
  - Capacitive sensing ETA unknown
- **Immune** to system updates depending on your settings of USB descriptor thanks to the usage of traditional passthrough technique (instead of relying on stolen keys like B***k)
- Does not use `delay()` unless absolutely necessary or does not matter
- Hackable in every sense unlike B***k boards
- Built with love :P

## Build

1. Execute patches/auto.sh under patches.
```shell
cd patches && bash ./auto.sh
```

2. Copy `patches/patched/framework-arduinoteensy-ds4` to `package` directory under your PlatformIO settings directory and `patches/patched/teensy-ds4` to `platforms` directory.
```shell
cd patched
cp -r framework-arduinoteensy-ds4 /path/to/your/pfio/config/dir/packages
cp -r teensy-ds4 /path/to/your/pfio/config/dir/platforms
```

3. Run `pio run` at the project root directory.
```shell
cd ../../
pio run
```

4. Now you can use `pio run -t upload` to upload the built firmware to your teensy.

## Usage

TODO

(There are some hints on the wiki and in the code, feel free to check them out)
