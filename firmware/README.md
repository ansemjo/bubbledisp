# Programming

I developed this circuit board with a pogo-pin adapter in mind to program the
ATmega328PB. The pads are on the bottom side and you need something like the
adapter sold by [PNOXI on tindie.com][pnoxi].

Furthermore everything is done through `pio`, the commandline tool for [PlatformIO][pio].

[pnoxi]: https://www.tindie.com/products/pnoxi/avr-isp-pogo-pin-adapter-2x3-idc2x3-pogo-254mm/
[pio]: https://platformio.org/install/cli

## Fuses

The fuse values are configured in `platformio.ini`. Run the `fuses` target to
program them to the correct values, which are mostly the defaults except for
disabling the `CLOCKDIV` to reach 8 MHz with the internal RC.

    pio run -t fuses

## Firmware

To compile and flash the firmware simply use the `upload` target. I'm using an
Adafruit FT323H to program my chips. If you're using something different, you'll
need to change the `upload_protocol = ...` in `platformio.ini` accordingly.

    pio run -t program
