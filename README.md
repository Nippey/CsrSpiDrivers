# CsrSpiDrivers #
## Goals ##
The goals of this project are to write a set of replacement SPI drivers for BlueLab4.1 for those without functional LPT ports and without the budget for a Csr approved USB-SPI adapter.

## Projects overview ##
### spilpt ###
This was the first step towards writing a new SPI driver: decompilation of the current LPT driver, as to be a starting point for anyone wanting to write his or her own driver.
The goal is to be as close to the original as possible, aiming for identical assembly code.
Most of the functions are already identical, although in some cases this is impossible because of either a different compiler being used, or it being more difficult than I can manage.

Mind you that the original spilpt.dll contains a few bugs (namely not properly closing all ports), and should not be used as a starting point for your own driver. See spilpt.fixed for that instead.

### spilpt.fixed ###
This is the second step, bug-fixing the spilpt.dll driver.
Basically this contains any bugs and optimizations (speed-wise or code-wise to increase readability) that do not impact the general functionality.
The main difference between spilpt and spilpt.fixed is that the former is meant to be code-identical and the latter is meant to be functionally identical.

### spilpt.arduino.bitbang ###
This is the first attempt at getting the thing actually working without an LPT port. This is a minimal change of spilpt.fixed to communicate with an arduino instead of an LPT port.

### spitlpt.arduino.offload ###
This should be the final product, a dll that offloads most of the work to the arduino to speed things up.


# Added/Modified by Nippey #
## Goals ##
Trying to add support for FTDI's FT232R USB-2-UART converter. This chip features a BitBang-mode which enables reading and changing eight of it's pins individually.

### spilpt.forwarder.fixed ###
The original forwarder-DLL loads additional DLLs during the execution of the DllMain-Function. This is not permitted following microsofts guidelines and may end in an error-message! This modification loads the forwarded DLL on the first call of another function than DllMain.
Another feature added is an INI-file to configure the behaviour of the forwarder.

### spilpt.ftdi ###
This is the first try to make the FT232R work as I wish. It is already posssible to extract some information from the bluetooth module, but it takes a long time! Reading/Writing the firmware is not possible.
At the moment I can't make any progress, as I have no working LPT-port which could give me reference signals.