# relay_board
The 8-channel relay board was designed originally to support field re-calibration of Wayne Halo units. That project is not currently being pursued further but the relay board could be useful in other production or test applications.

The board hardware is controlled by an ESP32-S3 DevKit module running firmware providing a communication API for controlling GPIO and Wi-Fi operations.

This repo provides libraries supporting control of the board relays, connecting to Wi-Fi access points, and communicating a test API running on target hardware.

## board firmware
This firmware is built for the ESP32-S3 DevKitC-N16R8 module which is currently used in the relay board fixture and will eventually be part of the GRID45 Gang Programmer fixture.

The firmware provides a serial communication interface consisting of a commamd/response sequence. The messaging is framed using ASCII control characters. The test_comm Python library implements the host-side of this protocol.

Functions provided by the firmware command interface include:
- set baud rate
- reboot firmware
- echo test
- get firmware version
- set/get board configuration
- scan for BLE SSIDs
- scan for Wi-Fi access points
- connect to Wi-Fi access point
- perform HTTP POST and GET operations with a remote target
- configure GPIO pins
- read GPIO inputs
- write GPIO outputs

## relay_lib
A Python package of libraries for the relay board

### app_utils.py
General application support functions
- list_comm_devices : Return a list of names of USB comm ports matching vendor id and product id criteria
- get_usb_sn : Return list of serial numbers of USB comm ports matching vendor and product id criteria
- compare_versions : Compare two version strings and return an indicator if one if lower, higher, or equal to the other

### board_control.py
class boardControl<br/>
Communicates with the GPIO command handler in the board firmware to configure, read, and write GPIO pins. A mapping mechanism is provided so GPIOs can be assigned descriptive names and referenced so.

When the class is instantiated, it must be passed the instance of the testerApi (see test_comm.py) and the GPIO map. The map is a list of dictionaries, one dictionary for each GPIO pin. The dictionary form is:

```{"name": NAME, "gpio_num": IO, "dir": DIR, "active_hi": <True|False>}```
- NAME is a descriptive name to assign to the GPIO
- IO is the GPIO number
- DIR is "in" or "out" per the desired IO direction
- active_hi is True if the input or output is considered active when the IO is set to 1, False otherwise

The board firmware stores some parameters in its non-volatile storage
- unit_sn : serial number string for the board. Used to identify boards if a script is written to control mulitple boards.
- tty_sn : serial number of FTDI serial board (if any). Not used for the relay board, but will be used in the GRID45 gang programmer to match the board with its associated serial ports.

class methods
- initialize : configure the IO pin directions and set outputs to inactive state. Call this before using any other method
- gpio_set : set the state of the named output pin
- gpio_get : return True if the input is active
- config_set : Set board configuration
- config_get : Read board configuration

### http_api.py
class httpAPI<br/>
This provides low-level command/response exchange via HTTP with a target device running a test API on its soft-AP access point. See the wifiComm class in wifi_comm.py for the HTTP POST support.

class methods:
- command : Use this for commands that return data
- command_no_resp : Use this for commands that return only success/fail indication without data
- fail_reason : Returns a string describing the reason for the most recent error

### relay_control.py
class relayControl<br/>
This is a child class to gpioControl, applying a layer of abstraction that maps each relay numbers 1..8 to its associated GPIO pins.

class methods:
- set_relay : Set the selected relay (1..8) on or off

### test_comm.py
This provides two classes: testerComm provides low-level serial message exchange with the board CPU while testerAPI is a child class that builds on this, providing core-level functions in the board CPU.

class testerComm<br/>
Low-level message exchange with host PC.

class methods:
- open : Open the serial connection
- close : Close the serial connection
- command : Send a command, receive the response, and return response data
- command_no_resp : Send a command and return True on success, False on failure. Use for - commands that do not return data
- spy : Receive and print serial from the board CPU. Used to capture out-of-band transmissions for debugging purposes.
- version : Return the version of the class library
- fail_reason : Return the reason for the most recent failure
- reset : Perform a hard reset of the board CPU by toggling the RTS line
- set_local_baud : Change the baud rate of the local end of the serial connection

class testerAPI<br/>
This provides an API to basic functions provided by the firmware on the board CPU. Board-specific functions will be provided by other libraries such as gpioControl and wifiComm.

class methods
- fw_version : Return the version of the firmware running on the board CPU
- uptime : Return the number of seconds the board firmware has been running
- reboot : Signal the board firmware to reboot
- chip_info : Return a dictionary of information about the board CPU
- echo : Send a string to the board CPU and expect it to be echoed back
- baud_set : Signal the board to change its baud rate. On success, change the local baud rate to match

### wifi_comm.py
class wifiComm<br/>
This provides low-level Wi-Fi support for connecting to a target device through the board CPU. The tester board acts as the Wi-Fi agent, connecting to the target and perform HTTP exchanges with it.

class methods
- ble_scan : Return a list of visible BLE SSIDs
- ble_scan_for : Return a list of BLE SSIDs beginning with the specified string e.g. find BLE SSIDs starting with "WW-HALO-"
- wifi_scan : Return a list of visible Wi-Fi access point SSIDs
- wifi_status : Return status of connection to a Wi-Fi access point
- wifi_connect : Connect to the specified SSID
- wifi_disconnect : Close existing connection
- http_post : Perform HTTP POST of a text payload to the given URL
- http_post_bin : Perform HTTP POST of a binary payload to the given URL
- http_get : Perform HTTP GET to the specified URL

The stream functions are provided for transferring large amounts of data through the relay board to a remote target - larger than can be passed over on POST operation. The sequence of use would be: open, one or more writes, finish, and close.
- http_stream_open : Open a stream with the specified URL
- http_stream_close : Close an open stream
- http_stream_write_bin : write binary data to the stream
- http_stream_finish : signal the end of transfer prior to closing the stream
