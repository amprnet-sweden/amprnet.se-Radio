# amprnet-radio.se

**** NOTE THAT YOU HAVE TO HAVE A VALID HAM RADIO LICENSE TO USE THIS RADIO ****

This is an attempt to create a 23 cm packet radio platform using a Texas Instrument CC1312 radio processor. According to specifications this chip can operate
in the 1076 - 1315 Mhz band, however there is very little support from TI for this frequency range. 

After successfully programming the radio to this band some preliminary code has been written that implements a serial to RF bridge, where two radios can be set 
up to work as a virtual serial wire, and ethernet to ethernet bridging using a w5500 ethernet module.

The current implementation brings out two serial channels where one is intended for "data" transport and one is intended for a "command terminal" where
the radio configuration can be changed. There is also a SPI channel to connect an SPI to Ethernet module. The radio is powered by 3.3 V, or 5 V via 
a regulator.

There is also a I2C port where a 2x16 or 4x20 character LCD can be connected, useful when doing mobile survey.

Power output is +12 dBm, and the radio speed is currently 1 Mbps.

Up til now, focus has been on creating code that operate the radio and the peripherals in a correct fashion, with the hope this could lead to a 
NPR-23 radio similar to the NPR-70 radio by Guillaume / F4HDK and other new usage of the 23 cm HAM radio band.

Version 0.93b is now current at 2025/04/15 and allows:

1  Serial to serial over a 23 cm channel

2  Ethernet to ethernet bridging

