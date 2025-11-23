# amprnet-radio.se

**** NOTE THAT YOU HAVE TO HAVE A VALID HAM RADIO LICENSE TO USE THIS RADIO ****

This is an attempt to create a 23 cm packet radio platform using a Texas Instrument CC1312 or CC1314R10 radio processor. According to specifications this chip can
operate in the 1076 - 1315 Mhz band, however there is very little support from TI for this frequency range. 

After successfully programming the radio to this band some preliminary code has been written that implements a serial to RF bridge, where two radios can be set 
up to work as a virtual serial wire, and ethernet to ethernet bridging using a w5500 ethernet module.

The current implementation brings out two serial channels where one is intended for "data" transport and one is intended for a "command terminal" where
the radio configuration can be changed. There is also a SPI channel to connect an SPI to Ethernet module. The radio is powered by 3.3 V, or 5 V via 
a regulator.

There is also a I2C port where a 2x16 or 4x20 character LCD can be connected, useful when doing mobile survey.

A "breakout" PCB has been made, that brings out these interfaces to wire wrap pins similar to arduinos and the like. There is a 3.3V regulator on board.

Power output is +12 dBm, and the radio speed is currently 1 Mbps.

Up until now, focus has been on creating code that operate the radio and the peripherals in a correct fashion, with the hope this could lead to a 
NPR-23 radio similar to the NPR-70 radio by Guillaume / F4HDK or other new usage of the 23 cm HAM radio band.

Version 0.93b is now current at 2025/04/15 and allows:

1  Serial to serial over a 23 cm channel

2  Ethernet to ethernet bridging

# Why build an amprnet radio?

Ham radio operators own most of the 44-net, i.e. all IPV4 addresses that begin with 44...... There are networks built within the AmprNet community in several countries across the world.
Most of these links are created with 2.4 Ghz Wifi radio, that incidently map onto he 13 cm HAM band. However, this equipment is not suited for amateurs to tamper with, in some countries
this is illegal.

However, HAM's have a nice frequency band on 1240 - 1300 Mhz, that promises slightly better propagation and that is very sparsly used.

Lower band are more or less "channelised" due to the proliferation of surplus land mobile equipment, thus preventing the creation of larger bandwidth channels. With some new
integrated circuits coming up, it is now possible to populate this band at low cost.

So in summary; Use the 23 cm band or loose it to some other service.
	Use the IPV4 adresses that we have.
	Do something technical and move the ham community forward a bit.
	Have fun and target a younger more Internet focused part of the population to use HAM radio.

# TO COMPILE  YOU NEED THIS

CCSTUDIO V 12.8.1		(I could not get 20.X to work properly)

SIMPLELINK-LOWPOWER-F2-SDK 8.30.01.01

If you want to be able to program/debug you also need a USB-Jtag device, I have used the LP-XDS110ET from TI, it is supported by CCS and is low cost.

Easiest way to start is to install the CCS, the processor SDK, and create an example by importing the GCC + NORTOS rfUARTbridge example from trhe SDK.

Compile and test that you get a valid image.

Replace all files from this git onto that directory.

Rebuild, and you should have the V 0.93b firmware. 

NOTE!! this code is based on one "big loop", and we really need a RTOS, since that prevents blocking I/O to lock the CPU up. The reason for doing this first version in
this ugly way, is simply "KISS", "Keep it simple stupid". This causes a *real* problem with Ethernet performance, so next version will be RTOS based. The main loop executes
in 17 uS when nothing happens, so we get decent latency, but it is unknown what percentage we "waste" in various blocking I/O's. Most input is based on callbacks.


Gullik / SM4FBD
