# Using AD5754 in teensy audio library

* Dual AD5754 DAC output module for for teensy audio library
* optimised for two AD5754 d/a converters with a total of 8 output channels. 
* Theoretically, will also work for a single AD5754, not tested
* This module relies on another output module to call UpdateAll(), therefor I use the TDM output module to set-up all the timing registers...
