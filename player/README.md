# About
Sketch which utilizes the generated MIDI header to controll outputs of an arduino. 

The code is highly optimized for Arduino/Genuino Nano, but should work (with small modifications) on other platforms as well.

# Settings
In order to change the behavior of the player for the correct setup, change the defines at the top (either comment them or set value to 0 to disable).

## TESTMODE
In TESTMODE the controller flashes all outputs in a repeated way. This is useful for debugging electrical wiring issues.

## DEBUG_SERIAL
When enabling DEBUG_SERIAL the controller opens a serial connection at 9600 baud and prints debug information to it. It can be read with e.g. the Arduino IDE Serial Monitor.

## USE_MCP
When enabling USE_MCP the controller assumes a connected MCP23017 GPIO extender (16 outputs). This setting overrides USE_RELAY.

When using USE_MCP, this [MCP23017 library](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library) is required.

## USE_RELAY
When specifying USE_RELAY the controller assumes a connected relay board with inverted logical levels (HIGH means off, LOW means on).

## Neither USE_MCP or USE_RELAY
If neither is specified, the controller uses the GPIO pins (from FIRST_GPIO to LAST_GPIO) and drives them accordingly. This can be used to operate e.g. the MOSFET based powerswitching board.

## USE_RF24
When enabling USE_RF24 the controller tries to synchronize with other boards via RF circuits supported by the RF24 library. This feature is still highly experimental.

When using USE_MCP, this [RF24 library](https://tmrh20.github.io/RF24) is required.

# Compilation
In order to compile (and upload) the player, some external libraries are required, depending on the settings (see above). Install them according to the [guide](https://www.arduino.cc/en/guide/libraries).
