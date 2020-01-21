# DecaWave EVB1000/DWM1001 Contiki Port with integrated Glossy and Crystal

## Overview 
[Contiki](https://github.com/contiki-os/contiki) is an open source operating system that runs on constrained embedded systems and provides standardized low-power wireless communication.

This repository contains a Contiki OS port for the DecaWave EVB1000 and DWM1001 platforms and
includes our implementations of Glossy and Crystal communication protocols and UWB ranging primitives.


## Port Features
This port includes support for:
* Contiki system clock and rtimers
* Watchdog timer
* Serial-over-USB logging using printf
* LCD display
* LEDs
* Rime stack over UWB
* IPv6 stack over UWB [to be tested]
* Single-sided Two-way Ranging (SS-TWR) with frequency offset compensation
* Double-sided Two-way Ranging (DS-TWR)
* [Glossy](https://ieeexplore.ieee.org/document/5779066), a fast flooding and synchronisation primitive (only EVB1000)
* [Crystal](https://dl.acm.org/doi/10.1145/2994551.2994558), a fast and reliable data collection protocol based on Glossy (only EVB1000)


Note that the ranging mechanisms are currently implemented using short IEEE 802.15.4 addresses.

## Code Structure
```
├── cpu
│   └── stm32f105
├── dev
│   └── dw1000
│       ├── crystal
│       └── glossy
├── examples
│   ├── crystal-test
│   ├── deployment
│   ├── glossy-test
│   ├── ranging
│   ├── range-collect
│   └── sensniff
└── platform
    ├── evb1000
    └── dwm1001

```

## Requirements
* For both platforms
  * [GNU Arm Embedded Toolchain](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm)
* For EVB1000
  * [ST-Link V2 Tools](https://github.com/texane/stlink)
* For DWM1001
  * [Nordic nRF5 IoT SDK v0.9.x](https://developer.nordicsemi.com/nRF5_IoT_SDK/nRF5_IoT_SDK_v0.9.x)
  * [SEGGER J-Link](https://www.segger.com/downloads/jlink/)

To use this port, clone the Contiki-UWB GitHub repository.
```
$ git clone https://github.com/d3s-trento/contiki-uwb.git
```

Inside the `contiki-uwb` directory, initialise the Contiki submodule:
```
$ git submodule update --init contiki
```

Then, either set `UWB_CONTIKI` environment variable pointing at the
`contiki-uwb` directory or define it in your application Makefile (you can use
`examples/ranging/Makefile` as a template).

For DWM1001, set also the following environment variables:
```
export NRF52_SDK_ROOT=/path/to/nrf_sdk
export NRF52_JLINK_PATH=/path/to/jlink/bin
```


## Examples
We include three main examples showing:
- how to perform ranging: **examples/ranging**
- how to use the Rime stack for data collection together with ranging: **examples/range-collect**
- and a sniffer port for **sensniff** that can be used to debug your applications.

Note that other general Contiki examples directly available in the original
Contiki OS GitHub repository, e.g, the Rime stack examples, can also be used on
the DecaWave EVB1000 platform. To use them, start by copying them to `contiki-uwb/examples`
and adjust the Makefile appropriately.

### Build and program your first example
#### EVB1000
Go to the **examples/ranging** folder and compile your example as:

```
$ cd examples/ranging/
$ make TARGET=evb1000
```

To flash your device with the compiled application, first connect the ST-LINK V2 programmer to your 
DecaWave EVB1000 and power up both the ST-LINK programmer and the EVB1000 through USB. Then, simply run:

```
$ make TARGET=evb1000 rng.upload
```

If everything works out fine, you should get a positive message like: 
> Flash written and verified! jolly good!

#### DWM1001
Go to the **examples/ranging** folder and compile your example as:

```
$ cd examples/ranging/
$ make TARGET=dwm1001
```

To flash your device with the compiled application, connect the device to the computer through USB.
Then, simply run:

```
$ make TARGET=dwm1001 rng.upload
```

If everything works out fine, you should get a positive message like: 
> Verifying flash   [100%] Done.

#### Configuring the ranging application
Note that the ranging application requires to set the IEEE 802.15.4 short address of the responder (i.e., the node 
to range with in the **rng.c** file). After flashing a device with any application from this code, the device should
show its short IEEE address in the LCD display. Set this address appropriately in the line:
```
linkaddr_t dst = {{0xdd, 0x37}};
```
This particular address should be displayed in the LCD (EVB1000 only) as `dd37`, it is also printed to the standard
output when the node boots.

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

If you want to use Glossy and/or Crystal, define the following in your application Makefile:
```
UWB_WITH_GLOSSY = 1
```
This will include Glossy and Crystal into the compilation process and exclude all other Contiki stacks.


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

* **[Poster: Enabling Contiki on Ultra-Wideband Radios](http://pablocorbalan.com/files/posters/contikiuwb-ewsn18.pdf)**.
Pablo Corbalán, Timofei Istomin, and Gian Pietro Picco. In Proceedings of the 15th International Conference on Embedded Wireless Systems and Networks (EWSN), Madrid (Spain), February 2018.

Please, consider citing this poster when using this port in your work.
```
@inproceedings{contiki-uwb,
 title = {{Poster: Enabling Contiki on Ultra-wideband Radios}},
 author = {Corbal\'{a}n, Pablo and Istomin, Timofei and Picco, Gian Pietro},
 booktitle = {Proceedings of the International Conference on Embedded Wireless Systems and Networks},
 series = {EWSN'18},
 year = {2018},
}
```

A research paper about our Glossy and Crystal implementations for DW1000 has been accepted for publishing at EWSN'2020.

* **Concurrent Transmissions for Multi-hop Communication on Ultra-wideband Radios**.
Diego Lobba, Matteo Trobinger, Davide Vecchia, Timofei Istomin, Gian Pietro Picco (University of Trento).

## License
This port makes use of low-level drivers provided by DecaWave and STMicroelectronics. These drivers are licensed on a separate terms.
The files developed by our research group for this port are under BSD license.

## Disclaimer
Although we tested the code extensively, this port is a research prototype that likely contains bugs. We take no responsibility for and give no warranties in respect of using this code.
