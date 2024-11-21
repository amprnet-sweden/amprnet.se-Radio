# amprnet-radio.se

This is an attempt to create a 23 cm packet radio platform using a Texas Instrument CC1312 radio processor. According to specifications this chip can operate
in the 1076 - 1315 Mhz band, however there is very little support from TI for this frequency range. 

After successfully programming the radio to this band some preliminary code has been written that implements a serial to RF bridge, where two radios can be set 
up to work as a virtual serial wire, and ethernet to ethernet bridging using a w5500 ethernet module.

The current implementation brings out two serial channels where one is intended for "data" transport and one is intended for a "command terminal" where
the radio configuration can be changed. There is also a SPI channel to connect an SPI to Ethernet module. The radio is powered by 3.3 V, or 5 V via 
a regulator.

Power output is +12 dBm, and the radio speed is currently 1 Mbps.
