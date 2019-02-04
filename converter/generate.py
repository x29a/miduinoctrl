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

  # result container
  result = collections.OrderedDict()
  # state of all channels - init with off (0)
  channel_state = [0] * channels
  # iterate through all milliseconds
  for current_ms in sorted(allEvents):
      events = allEvents[current_ms]
      # iterate over all events for this millisecond
      for event in events:
        # check if event contains note information
        if 'note' in event:
          # check if mapping for pin exists
          if event['note'] in mapping:
            pin = mapping[event['note']]
            # check if event contains on/off information
            if 'mode' in event:
              if event['mode'] == 0:
                channel_state[pin] = 0
              elif event['mode'] == 1:
                channel_state[pin] = 1
              else:
                print("unknown mode in event:" + event)
          else:
            print("no mapping for note:" + str(event['note']))
      # convert the channel state to hex for the C header
      # TODO add channel_state and current_ms to header
      hexValue = hex(int(''.join(map(str, reversed(channel_state))), 2))
      result[current_ms] = hexValue
  # generate header
  with open(output, 'w') as f:
    f.write('// generated header - changes will be overwritten!\n')
    f.write('// generation time:' + datetime.now().strftime("%Y%m%d%H%M%S") + '\n')
    f.write('// header for flash memory access\n')
    f.write('#include <avr/pgmspace.h>\n\n')
    f.write('// number of events\n')
    f.write('unsigned long int event_cnt = ' + str(len(result)) + ';\n')
    f.write('// event timestamps in milliseconds\n')
    f.write('const PROGMEM unsigned long int event_timestamps[] = {' + ','.join(map(str,result.keys())) + '};\n')
    f.write('// events, index like timestamps, value is binary encoded state for channels\n')
    # adjust width correctly to save some space
    datatype = 'uint32_t'
    if(channels <= 8):
        datatype = 'uint8_t'
    elif(channels <= 16):
        datatype = 'uint16_t'

    f.write('const PROGMEM ' + datatype + ' events[] = {' + ','.join(result.values()) + '};\n')
    f.write('// index for current event\n')
    f.write('unsigned long int current_event_index = 0;\n')    
  print('events: ' + str(len(result)))

# main program
try:
  # TODO use command line parameters
  generateData(input_file, output_file)

finally:
  print('bye')
