# DecaWave EVB1000 Contiki Port 

## Overview
This repository contains a Contiki port for the DecaWave EVB1000 platform. The DecaWave EVB1000 is a platform developed by DecaWave that includes an STM32F105 microcontroller, a DecaWave DW1000 ultra-wideband transceiver, and an LCD display. Contiki is an open source operating system that runs on constrained embedded systems and provides standardized low-power wireless communication.

## Port Features
This port includes support for:
* Contiki system clock and rtimers
* Watchdog timer
* Serial-over-USB logging using printf
* LCD display
* LEDs
* Rime stack over UWB
* IPv6 stack over UWB [to be tested]
* Single-sided Two-way Ranging (SS-TWR)
* Double-sided Two-way Ranging (DS-TWR)

Note that the ranging mechanisms are currently implemented using short IEEE 802.15.4 addresses.

## Code Structure
```
├── cpu
│   └── stm32f105
├── dev
│   └── dw1000
├── examples
│   ├── range-collect
│   ├── ranging
│   ├── sensniff
└── platform
    └── evb1000
```

## Requirements
* [GNU Arm Embedded Toolchain](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm)
* [ST-Link V2 Tools](https://github.com/texane/stlink)
* [Contiki OS](https://github.com/contiki-os/contiki)

To use this port, clone the Contiki OS GitHub repository.
```
$ git clone https://github.com/contiki-os/contiki
```

Then, you may set `CONTIKI` in your `PATH` as:
```
export CONTIKI=/path/to/contiki
```

## Examples
We include three main examples showing:
- how to perform ranging: **examples/ranging**
- how to use the Rime stack for data collection together with ranging: **examples/range-collect**
- and a sniffer port for **sensniff** that can be used to debug your applications.

Note that other general Contiki examples directly available in the original Contiki OS GitHub repository, e.g,
the Rime stack examples, can also be used on the DecaWave EVB1000 platform.

### Build and program your first example
Go to the **examples/ranging** folder and compile your example as:

```
$ cd examples/ranging/
$ make 
```

To flash your device with the compiled application, first connect the ST-LINK V2 programmer to your 
DecaWave EVB1000 and power up both the ST-LINK programmer and the EVB1000 through USB. Then, simply run:

```
$ make rng.upload
```

If everything works out fine, you should get a positive message like: 
> Flash written and verified! jolly good!

Note that the ranging application requires to set the IEEE 802.15.4 short address of the responder (i.e., the node 
to range with in the **rng.c** file). After flashing a device with any application from this code, the device should
show its short IEEE address in the LCD display. Set this address appropriately in the line:
```
linkaddr_t dst = {{0xdd, 0x37}};
```
This particular address should be displayed in the LCD as `0xdd37`.

### IEEE Addresses
In this port, we generate the IEEE 802.15.4 addresses used for the network stacks and ranging 
using the DW1000 part id and lot id numbers. To see how we generate the addresses look at 
the **set_rf_params()** function in **platform/evb1000/contiki-main.c**.

### Serial Port 
After you flash the node, you should be able to connect to the serial port and see the serial input printed by the device by:
```
$ make PORT=/dev/tty.usbmodem1411 login
```

The `PORT` variable will change depending on your operating system.
If you are using the ranging appication, you should receive some data as follows:
```
R req
R success: 1.064711 bias 1.344711
```

### Compilation Options
If your application does not need to perform ranging, you can disable ranging in your `project-conf.h` file as:
```
#define DW1000_CONF_RANGING_ENABLED 0
```
By default, ranging is enabled.

## Porting to other Platforms / MCUs
This port can be easily adapted for other platforms and MCUs based on the DW1000 transceiver. The radio driver only requires platform-specific implementations for the following functions:
* writetospi(cnt, header, length, buffer)
* readfromspi(cnt, header, length, buffer)
* decamutexon()
* decamutexoff(stat)
* deca_sleep(t)

For an overview of these functions, we refer the reader to the platform-specific section on **dev/dw1000/decadriver/deca_device_api.h** and
the implementation on **platform/evb1000/dev/dw1000-arch.[ch]**. These functions must be implemented in the platform **dw1000-arch** code.

## Publications
This port has been published as a poster at [EWSN'18](https://ewsn2018.networks.imdea.org).

* **[Poster: Enabling Contiki on Ultra-Wideband Radios](http://pablocorbalan.com/docs/posters/ewsn18-contikiuwb.pdf)**.
Pablo Corbalán, Timofei Istomin, and Gian Pietro Picco. In Proceedings of the 15th International Conference on Embedded Wireless Systems and Networks (EWSN), Madrid (Spain), February 2018.

Please, consider citing this poster when using this port in your work.

## License
This port makes use of low-level drivers provided by DecaWave and STMicroelectronics. These drivers are licensed on a separate terms.
The files developed by our research group for this port are under BSD license.

## Disclaimer
Although we tested the code extensively, this port is a research prototype that likely contains bugs. We take no responsibility for and give no warranties in respect of using this code.
