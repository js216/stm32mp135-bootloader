# USB MSC Bootloader for STM32MP135

This is a tiny, single-stage USB Mass Storage bootloader for the STM32MP135.
When the board powers on, it can enumerate as a flash drive on your computer, so
you can write SD-card images directly with simple tools like dd—no TF-A, U-Boot,
or complex setup required. Programs can then be loaded and executed immediately,
making it easy to experiment, modify, or add new functionality without
navigating a multi-stage boot chain. It works on the [eval
board](https://www.st.com/en/evaluation-tools/stm32mp135f-dk.html) as well as my
simpler [custom board](https://github.com/js216/stm32mp135_test_board).

Features include:

- Write to SD card from USB via Mass Storage Class, like a flash drive
  *(optional)*
- Load program from SD card to DDR and execute it *(optional)*
- Command line with tab completion and "up arrow" history *(optional)*
- Super simple, hackable code: <2k lines of code on top of the [ST HAL
  drivers](https://wiki.st.com/stm32mpu/wiki/STM32CubeMP13_Package_-_Getting_started)

### Hardware setup

1. Insert the SD card into the slot.

2. Connect `CN12` (`PWR_IN`), the USB-C port to the right of the screen, to a
   powered USB hub. (Consult this [photo](https://embd.cc/images/board.jpg) for
   connector labels.)

3. Connect `CN10`, the Micro USB left of the screen, to a desktop computer,
   which will enumerate as a serial port (e.g. `/dev/ttyACM0` on Linux, or
   `COM20` on Windows).

4. Connect `CN7`, the USB-C port on the lower left corner of the screen, to a
   host computer. This port will be used to transfer the data via Mass Storage.

### Getting Started

1. To compile the program, just run Make:

       cd msc_boot
       make CFLAGS_EXTRA=-DUSE_STPMIC1x=1

   If everything goes well, it should print which binaries were included in the
   generated SD card image:

       File                      LBA      Size       Blocks
       -------------------------------------------------------
       main.stm32                128      100352     197
       blink.bin                 324      16896      34

   Make note of the `LBA` address of the `blink.bin` program (324), and the
   number of blocks (34). You'll use these with the `load_sd` command in the
   next section.

2. The initial boot can be done USB-C with DIP switch set to `BOOT = 000` (see
   below for initial boot alternatives):

       STM32_Programmer_CLI.exe -c port=usb1 -w scripts/flash.tsv

3. In the serial console (115200 baud, no parity), the prompt (`> `) should be
   displayed. Write `help` and press Enter to get a list of available commands.

### Using the Bootloader

The bootloader enumerates as a USB flash drive. Use `dd` (on Windows, get
it from [here](http://www.chrysocome.net/dd)) to write the SD card image:

    dd if=build/sdcard.img of=/dev/sdc # on Linux
    dd if=build/sdcard.img of=\\.\E:   # on Windows

⚠ WARNING: This will erase all data on the target device, so double-check that it
is the newly-enumerated SD card and contains no important files.

After writing the SD card, open the serial console (115200 baud) and load a
program into DDR using the `load_sd` command, then execute it with `jump`:

    > load_sd 34 324
    > jump

This runs the blink program: it prints diagnostics and keeps blinking the red
led. Success!

To run other programs, generate an SD card image containing the bootloader and
the program. For example, the blink SD image was created with:

    python3 scripts/sdimage.py build/sdcard.img build/main.stm32 build/blink.bin

The script prints a table of where each program is installed on the SD card. Use
the shown block number and size with `load_sd` and `jump` to load and run any
program.

### Configuration Flags

Several flags control the build process, depending on what features are desired
in the final executable. Pass them to make via the `CFLAGS_EXTRA=-D<flag>`
mechanism as shown above. The `<flag>` can be one of:

- `USE_STPMIC1x`: Enable this (set to `1`) if your board includes the STPMIC1
  PMIC (true for the official eval board). Disable it (set to `0`) for simple
  custom boards that power the STM32MP135 directly.

Other features can be disabled just by removing them from the `main()` function:

- Remove command line support by removing `cmd_init()` and `cmd_poll()`.
- Remove the blinking by removing `blink()`.
- Remove USB support by removing `usb_init()`.
- Remove SD card support by removing `sd_init()`.

### Initial Boot Alternatives

The initial download as explained above requires the
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html),
which implements the custom DFU (Device Firmware Upgrade) protocol used by ST.
There are other ways to load this program onto the STM32MP135 that do not
require the STM32CubeProgrammer:

- Use a separate computer to copy the image (`build/sdcard.img`) directly to the
  beginning of the SD card, re-insert the card into the board, set `BOOT = 101`
  (SD card boot), and press the reset button.

- Alternatively: Use the UART bootloader (`BOOT = 000`, "serial boot") with the
  provided Python script to write the bootloader directly into SYSRAM:

      python3 scripts/uart_boot.py -c COM20 -f build/main.stm32

- Or even: Use JTAG (`BOOT = 100`, "engineering boot"):

      JLinkGDBServer.exe -device STM32MP135F -if swd -port 2330
      arm-none-eabi-gdb.exe -q -x scripts/load.gdb

### Contributing

Questions, bug reports, and bring-up notes are welcome—feel free to open a
GitHub issue if something isn't clear or if you run into hardware issues on your
own STM32MP135 board. Even simple questions help improve the documentation.

Small, focused pull requests are also appreciated. Please follow the general
style of the existing code and keep features optional so the bootloader stays
small and easy to understand. Run static code analysis via `make check` before
committing changes.

### Author

Jakob Kastelic (Stanford Research Systems)
