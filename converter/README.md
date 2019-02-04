# About
This converter will turn the events from a MIDI file into a header which can be used in microcontroller projects.

The generated header uses the PROGMEM mechanism to store its values into EPROM instead of sparse SRAM.

The converter tries to detect and minimize the required storage by adjusting the variable type.

Tested with Python 2.7.12.

# Usage
* copy `input.midi` to folder (and possibly replace old versions)
* call `python generate.py`
* the output `mididata.h` is copied to the player folder and the project `#include`s it

# Mapping
The `mapping` file can be used to assign MIDI tones to different output channels. The default is to start at MIDI tone 36 (C1) with the first output channel and increment the channel for each tone. See `mapping.example` for the syntax.