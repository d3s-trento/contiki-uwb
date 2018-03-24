sensniff Contiki Project
========================
This example can be used to create an IEEE 802.15.4 wireless sniffer firmware,
meant to be used in parallel with
[sensniff](https://github.com/g-oikonomou/sensniff).

Running
=======
* Build this example and program your device
* Connect your device to a host
* Run sensniff on your host
* Fire up wireshark and enjoy.

You can run sensniff manually, or you can simply run `make sniff` from within
this directory. If you choose the latter option, you may have to specify the
port where you device is connected by using the PORT variable. For example, if
your device is connected to `/dev/ttyUSB1` then you should run
`make PORT=/dev/ttyUSB1 sniff`.

Make sure your device's UART baud rate matches the `-b` argument passed to
sensniff. I strongly recommend using at least 460800. This comment does not
apply if your device is using native USB.

Subsequently, make absolutely certain that your device is tuned to the same RF
channel as the network you are trying to sniff. You can change sniffing channel
through sensniff's text interface.

More details in sensniff's README.

## NOTE
Currently, not all the features of sensniff are supported for the DecaWave EVB1000.
For instance, it is not possible to change the RF channel at runtime using the 
sensniff Python application.
