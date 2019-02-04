# About
Arduino based system that is controlled by MIDI sequences.

# Converter
The [converter](converter/README.md) is used to turn a MIDI file into a C Header for microcontrollers.

# Player
The [player](player/README.md) utilizes the generated MIDI header to control various outputs (MCP23017, relay boards, MOSFETs or GPIO).

# PCB
There are multiple [PCB boards](pcb/README.md) which act as adapters (for relay and RF boards) and controllers (MCP23017 and MOSFETs).