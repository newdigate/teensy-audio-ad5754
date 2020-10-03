# Using AD5754 in teensy audio library

* Dual AD5754 DAC output module for for teensy audio library
* optimised for two AD5754 d/a converters with a total of 8 output channels. 
* Theoretically, will also work for a single AD5754, not tested
* This module relies on another output module to call UpdateAll(), therefor I use the TDM output module to set-up all the timing registers...


# TODO: 
* At 14Mhz SPI clk speed, the audio buffers are transmitted slightly faster than the period which the audio buffers are received
  * So the audio is condensed and then paused for a tiny amount of time
  * possibly slow down the transmit rate, by transmitting individual bytes at a regular interval, between data writes, without asserting the cable select, so ignored by AD5754 devices 
