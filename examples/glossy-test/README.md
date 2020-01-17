# Glossy Test - A simple application the evaluate Glossy

Glossy Test allows the evaluation of the Glossy implementation for the DW1000
built for the Contiki OS.


## `glossy_test` process

The program accounts for two types of nodes:

* the flood initiator
* receivers

Once the process starts, it checks whether the device is the
initiator.

* If it is not, it waits some time Tr and then it
  goes to the `glossy_thread`.

* If it is, the device waits a time Ti, Ti > Tr, to be
  sure receivers are booted, after which it continues to
  the `glossy_thread`.

Within `glossy_thread` a receiver keeps scanning the channel until 
receiving a packet from the initiator, after which it can synchronise 
to it and schedule synchronised listening phases.

An initiator instead, issues a Glossy flood to flood a packet
identified by a sequence number. When the flood ends, the node
waits until the next period before transmitting a new packet.


## Configuration and compilation

Currently, the implementation of Glossy for DW1000 only supports the
EVB1000 platform. We are working to enable it on the DWM1001 as well.

It is possible to tune both Glossy and the application.
The application parameters can be configured directly
in the `glossy_test.c` file and comprise:

* the *Period*, the time between two consecutive Glossy floods
  issued by the initiator,
* the *Slot* duration, the time after which a Glossy flood is **forced**
  to stop in case it hasn't already,
* the *Guard* time, the time used as offset for receivers to start
  listening before the expected flood start time, once the receiver
  is synchronised with the initiator,

Configurations for the Glossy protocol itself can be set
in the `project-conf.h`, a list with the possible values can
be found in the `project-conf-template.h`.

Parameters include:

* *N_TX* the maximum number of transmissions, within a Glossy flood
  after which the communication is considered complete and the Glossy 
  round ends.


* the *Glossy Version* used, in which:

    1. `Glossy Standard` denotes the original version of the protocol,
       in which the communication pattern for a receiver within a flood
       is such that a transmission follows each and every reception;

    2. `Glossy TX only` denotes the version in which a receiver, after
       the first reception, performs only transmissions.

* the *Slot estimation approach* to use, which can be:

    1. `Dynamic`, in which the slot is computed as the sum of the
        times between the reception and transmission or
        transmission and reception of two packets with consecutive
        relay counters and the total number of such pairs encountered.

    2. `Static`, in which the slot is estimated based on the length
        of the frame received.

* whether to use the dw1000 *Smart Tx Power* feature (default to No).

* the payload length to be used within the application (default to 115).


In case no settings are defined related to Glossy in the `project-conf.h`
file, the protocol will use the *TX only Version* with *Static Slot Estimation* as default values.

### NOTE

The project comes with no `project-conf.h` file. This is
to allow the simgen.py script generate it. This also means that radio
configurations stated in `radio-conf.h` **ARE NOT TAKEN
INTO ACCOUNT** by default, since this header is supposed to be included 
in the `project-conf.h` file.

If the user desires to define
a specific project-conf to be used, they can
rename the `project-conf-template.h` file to `project-conf.h`,
define Glossy parameters to their liking and compile the
project issuing the make command.

Within the `radio-conf.h` file, communication parameters
for the DW1000 radio chip are defined.
As mentioned previously, if no project-conf is employed, the
radio configuration is set to some default values, so it will
work anyhow.


### Compilation and running on the UniTn testbed

To compile the appication is required to issue the `make`
command.

Within the project folder a `glossy_test.json` file is present.
By default a subset of testbed nodes is considered and paths
to dependency files (`python_script` file and application binary)
are **relative paths**.

## `simgen.py` - Performing planned tests

To make (hopefully) easier the process of planning tests on
the UniTn testbed, the `simgen.py` program present
in the `test_tools` folder should be used.

Check out its README to know more on its usage.
