# Use MIDI library to parse events from MIDI file into 
# C header file which can be used in microcontroller
# projects.
#
# Author: x29a
# License: see LICENSE.md
#
from midi import ParseMidi
from lib.MidiInFile import MidiInFile

from time import sleep
import glob
import os
import ast
from time import time
from datetime import datetime
import timeit
import collections

# first midi tone (e.g. C1)
first_midi_tone = 36

# name of mapping file to change binding of notes and channels
mapping_file = 'mapping'
input_file = 'input.midi'
# generate into player folder
output_file = '../player/mididata.h'

# functions
def generateData(input, output):
  parser = ParseMidi()
  midi_in = MidiInFile(parser, input)
  midi_in.read()

  allEvents = parser.getEvents()

  last = parser.getLast()

  # each note is a different output/channel for the uC
  channels = parser.getNotes()
  print("channels: "+str(channels))

  # try to read the mapping file
  mapping = {}
  try:
    if os.path.isfile(mapping_file):
      with open(mapping_file, 'r') as f:
        s = f.read()
        mapping = ast.literal_eval(s)
    else:
        print("using default mapping")
  except:
    pass

  # provide default mapping
  if not mapping:
    for tone in xrange(first_midi_tone, first_midi_tone + channels):
      mapping[tone] = (tone - first_midi_tone)


  # state of all channels - init with off (0)
  # value gets carried through all timestamps, so it must actively change
  # channel state is a bitmask for all channels whether they are on (1) or off (0)
  channel_state = [0] * channels

  # result containers
  channel_state_list = []
  value_list = []
  note_list = []
  timing_list = []

  # iterate through all milliseconds
  for current_ms in sorted(allEvents):
      events = allEvents[current_ms]
      # iterate over all events for this millisecond timestamp
      for event in events:
        # check if event contains note information
        if 'note' in event:
          # check if mapping to pin exists - unknown notes are ignored
          if event['note'] in mapping:
            # pin is the channel for which this event occurs - dont confuse with midi channel
            pin = mapping[event['note']]
            # check if event contains on/off information
            if 'mode' in event:
              if event['mode'] == 0:
                channel_state[pin] = 0
              elif event['mode'] == 1:
                channel_state[pin] = 1
              # mode is ON and there is a velocity set
              if event['mode'] == 1 and 'velocity' in event:
                channel_value = event['velocity']
              else:
                # no velocity found or mode is OFF so we turn off instead
                channel_value = 0
              # append to all result containers
              value_list.append(channel_value)
              note_list.append(pin)
              timing_list.append(current_ms)
          else:
            print "no mapping for note: "+event['note']
      # convert the channel state to hex as good compromise between readability and textsize
      hexValue = hex(int(''.join(map(str, reversed(channel_state))), 2))
      channel_state_list.append(hexValue)

  # verify data container lengths
  if len(value_list) != len(note_list) or len(note_list) != len(timing_list):
    print "data containers are not equally filled"
    exit(1)

  # generate header
  with open(output, 'w') as f:
    f.write('// generated header - changes will be overwritten!\n')
    f.write('// generation time: ' + datetime.now().strftime("%Y%m%d%H%M%S") + '\n\n')
    f.write('// header for flash memory access\n')
    f.write('#include <avr/pgmspace.h>\n\n')
    f.write('// total number of different channels/notes used\n')
    f.write('const uint8_t channel_cnt = ' + str(channels) + ';\n')
    f.write('// total number of events\n')
    f.write('unsigned long int event_cnt = ' + str(len(value_list)) + ';\n')
    f.write('// timestamps of events\n')
    f.write('const PROGMEM unsigned long int event_timestamps[] = {' + ','.join(map(str,timing_list)) + '};\n')
    f.write('// event value is velocity (0-127) of note, used for speed control (via PWM)\n')
    f.write('const PROGMEM uint8_t event_values[] = {' + ','.join(map(str,value_list)) + '};\n')
    f.write('// pin/note/channel for event\n')
    f.write('const PROGMEM uint8_t event_notes[] = {' + ','.join(map(str,note_list)) + '};\n')
    f.write('// state (on/off) of all channels for event, used for switching\n')
    # adjust width correctly to save some space
    datatype = 'uint32_t'
    if(channels <= 8):
      datatype = 'uint8_t'
    elif(channels <= 16):
      datatype = 'uint16_t'
    f.write('const PROGMEM ' + datatype + ' event_states[] = {' + ','.join(map(str,channel_state_list)) + '};\n')
    f.write('// index for current event (set to beginning)\n')
    f.write('unsigned long int current_event_index = 0;\n')

  print('events: ' + str(len(value_list)))

# main program
try:
  # TODO use command line parameters
  generateData(input_file, output_file)

finally:
  print('bye')
