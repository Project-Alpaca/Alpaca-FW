# FT-Controller-FW

Teensyduino-based firmware for Project Diva FT Controller project

## Build

1. Execute patches/auto.sh under patches.
```sh
cd patches && bash ./auto.sh
```

2. Copy `patches/patched/framework-arduinoteensy-ds4` to `package` directory under your PlatformIO settings directory and `patches/patched/teensy-ds4` to `platforms` directory.
```sh
cd patched
cp framework-arduinoteensy-ds4 /path/to/your/pfio/config/dir/packages
cp teensy-ds4 /path/to/your/pfio/config/dir/platforms
```

3. Run `pio run` at the project root directory.
```sh
cd ../../
pio run
```

4. Now you can use `pio run -t upload` to upload the built firmware to your teensy.
