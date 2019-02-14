#include <Wire.h>
#include <util/atomic.h>

// CONFIGURATION
// test mode, toggle all IO in delayTime ms intervals
#define TESTMODE 1
const unsigned long delayTime = 500;

// print debug messages via serial (this overrides the STATUS_LED because they both use the same LED)
#define DEBUG_SERIAL 0

// use MCP23017 or direct GPIO
// if using MCP it is assumed to be be connected to an MCP at address 0x20.
// This results in a maximum of 16 outputs.
// if not using MCP it is assumed to be driving some switching technology via GPIO directly
// This results in a maximum of 10 outputs (FIRST_GPIO to LAST_GPIO)
// USE_MCP overrides USE_RELAY
#define USE_MCP 0

// if using RELAY board, the logical levels are inverted (HIGH is off and LOW is on)
// if not using RELAY, it is assumed to be driving a e.g. MOSFET based board
#define USE_RELAY 0

// use RF24 radio to sync multiple nodes - experimental
#define USE_RF24 0

/* /!\ INTERNAL VARIABLES BELOW - DONT CHANGE /!\ */
#if !defined(TESTMODE) || !TESTMODE
// generated header with MIDI information - see converter
#include "mididata.h"
#endif

// timestamp of next action, only read in setup() and when index changes
unsigned long int nextTimestamp = 0;

// GPIOs used when not using an MCP
// start at GPIO 2 because 0 and 1 could be used for trx
#define FIRST_GPIO 2
// stop at 12 because 13 is internal LED
#define LAST_GPIO 12

#if defined(USE_MCP) && USE_MCP
// https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
#include "Adafruit_MCP23017.h"
// global object to interface MCP23017
Adafruit_MCP23017 mcp;
// use all 16 outputs
#define FIRST_PIN 0
#define LAST_PIN 16
#else
// use the range of available GPIO pins
#define FIRST_PIN FIRST_GPIO
// +1 because the condition will be "<" and not "<="
#define LAST_PIN LAST_GPIO+1
#endif

#if defined(USE_RF24) && USE_RF24
// https://tmrh20.github.io/RF24
#include "RF24.h"
// CE is connected to 18 and CS(N)/SS to 19
RF24 radio(18, 19);
const uint8_t pipe[] = { 0xCE, 0xCE, 0xCE, 0xCE, 0xCE };
const uint64_t msg_obey = 0xDEADBEEFLL;
const uint64_t msg_start = 0xDEADC0DELL;
bool iAmMaster = 0;
unsigned long int nextObeySend = 0;
// interval at which the obey messages are sent
#define OBEY_INTERVAL 10000
#endif

// is the detected size of the events array 32bit or less (different read functions required)
unsigned int eventSizeIs32bit = 0;

// is this a new run of the program
bool isNewRun = true;

// dont use internal LED (LED_BUILTIN) but instead RX led for status
#define STATUS_LED 0

void setup()
{
  // set status LED to output
  pinMode(STATUS_LED, OUTPUT);

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
  // init serial connection
  Serial.begin(9600);
  // wait for connection to be ready
  while (!Serial);
#endif

#if defined(USE_MCP) && USE_MCP
  // 0 equals hw address 0x20 equals A0, A1, A2 low
  mcp.begin(0);
#endif

#if defined(USE_RF24) && USE_RF24
  // seed random generator
  randomSeed(analogRead(0));
  radio.begin();
  // disable auto-acks
  radio.setAutoAck(false);
  // maximum power
  // TODO: change to RF24_PA_MAX with cap and power
  radio.setPALevel(RF24_PA_MAX);
  // use a different than default channel to minimize crossover
  radio.setChannel(109);
  //pinMode(18, OUTPUT);
  //pinMode(19, OUTPUT);
#endif

  for (int pin = FIRST_PIN; pin < LAST_PIN; ++pin)
  {
    setDigitalPin(pin, LOW);
#if defined(USE_MCP) && USE_MCP
    mcp.pinMode(pin, OUTPUT);
#else
    pinMode(pin, OUTPUT);
#endif
  }

#if !defined(TESTMODE) || !TESTMODE
  // check data size of events to decide on the read method
  if ((sizeof(event_states) / event_cnt) > 2)
  {
    eventSizeIs32bit = 1;
  }
#endif

#if defined(USE_RF24) && USE_RF24
  // check if there is a master broadcasting already
  radio.openReadingPipe(1, pipe);
  radio.startListening();

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
  Serial.println("looking for master");
#endif

  // random backoff to cope with multiple devices powered on at same time
  delay(random(1000));

  // check for 60s and if nothing arrives, become master
  nextTimestamp = millis() + 60UL * 1000;
  while (1)
  {
    if (radio.available())
    {
#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
      Serial.println("recv:");
#endif
      // received something, lets check if its from another node
      uint64_t recv_data;
      radio.read(&recv_data, sizeof(recv_data));
#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
      Serial.println((uint32_t)recv_data);
#endif
      if (recv_data == msg_obey || recv_data == msg_start)
      {
        // this is a valid master commanding us, obey!
        iAmMaster = 0;
        break;
      }
    }
    else if (millis() >= nextTimestamp)
    {
      // timeout, let me master this!
      iAmMaster = 1;
      radio.stopListening();
      radio.closeReadingPipe(1);
      radio.openWritingPipe(pipe);

      // tell the others who is boss
      radio.write(&msg_obey, sizeof(msg_obey));
      break;
    }
  }

  // show via TX led if master (on) or not (off)
  pinMode(1, OUTPUT);
  if (iAmMaster)
  {
    digitalWrite(1, LOW);
  }
  else
  {
    digitalWrite(1, HIGH);
  }

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
  Serial.print("iAmMaster:");
  Serial.println(iAmMaster);
#endif
#endif

#if !defined(TESTMODE) || !TESTMODE
  // read the first timestamp for action
  nextTimestamp = pgm_read_dword_near(event_timestamps + current_event_index);
#endif
}

// main loop
#if defined(TESTMODE) && TESTMODE
// test loop
void loop()
{
#if defined(USE_RF24) && USE_RF24
  // this is a linear loop() so tell everybody at the beginning that we are starting now
  if (iAmMaster)
  {
    radio.write(&msg_start, sizeof(msg_start));
  }
  else
  {
    // wait for start message to arrive
    while (1)
    {
      if (radio.available())
      {
        // received something, lets check if its from uceremony and a start command
        uint64_t recv_data;
        radio.read(&recv_data, sizeof(recv_data));
        if (recv_data == msg_start)
        {
          // ok, lets go
          break;
        }
      }
    }
  }
#endif

  for (int pin = FIRST_PIN; pin < LAST_PIN; ++pin)
  {
    setDigitalPin(pin, HIGH);
  }
  digitalWrite(STATUS_LED, HIGH);
  delay(delayTime);

  for (int pin = FIRST_PIN; pin < LAST_PIN; ++pin)
  {
    setDigitalPin(pin, LOW);
  }
  digitalWrite(STATUS_LED, LOW);
  delay(delayTime);

  for (int pin = FIRST_PIN; pin < LAST_PIN; ++pin)
  {
    setDigitalPin(pin, HIGH);
    digitalWrite(STATUS_LED, HIGH);
    delay(delayTime);
    setDigitalPin(pin, LOW);
    digitalWrite(STATUS_LED, LOW);
    delay(delayTime);
  }
#if defined(USE_MCP) && USE_MCP
  for (int pin = 0; pin < 16; ++pin)
  {
    mcp.digitalWrite(pin, HIGH);
    digitalWrite(STATUS_LED, HIGH);
    delay(delayTime);
    mcp.digitalWrite(pin, LOW);
    digitalWrite(STATUS_LED, LOW);
    delay(delayTime);
  }
#else
  for (int pin = FIRST_GPIO; pin <= LAST_GPIO; ++pin)
  {
#if defined(USE_RELAY) && USE_RELAY
    digitalWrite(pin, LOW);
    digitalWrite(STATUS_LED, HIGH);
    delay(delayTime);
    digitalWrite(pin, HIGH);
    digitalWrite(STATUS_LED, LOW);
    delay(delayTime);
#else
    digitalWrite(pin, HIGH);
    digitalWrite(STATUS_LED, HIGH);
    delay(delayTime);
    digitalWrite(pin, LOW);
    digitalWrite(STATUS_LED, LOW);
    delay(delayTime);
#endif
  }
#endif
}
#else
void loop()
{
#if defined(USE_RF24) && USE_RF24
  // if the sequence is about to start we need to devide roles
  if (isNewRun && current_event_index == 0)
  {
    if (iAmMaster)
    {
      radio.write(&msg_start, sizeof(msg_start));
    }
    else
    {
      // wait for start message to arrive
      while (1)
      {
        if (radio.available())
        {
          // received something, lets check if its from uceremony and a start command
          uint64_t recv_data;
          radio.read(&recv_data, sizeof(recv_data));
          if (recv_data == msg_start)
          {
            // ok, lets go
            break;
          }
        }
      }
    }

    // not a new run anymore
    isNewRun = false;

    // reset the timer so that there is no offset for the first event
    setMillis(0);
  }
#endif

  const unsigned long int now = millis();

#if defined(USE_RF24) && USE_RF24
  // lets remind everybody again that there is a master
  if (now >= nextObeySend)
  {
    if (iAmMaster)
    {
      // use timeout to not divert too much
      radio.writeBlocking(&msg_obey, sizeof(msg_obey), 50);
      const unsigned long int timePassed = millis() - now;
      delay(50 - timePassed);
      //Serial.println("reminding");
    }
    else
    {
      // TODO add delay in slave so it keeps up with the sending?
      delay(50);
    }
    nextObeySend = millis() + OBEY_INTERVAL;
  }
#endif

  // current timestamp is GE to next event timestamp - act!
  if (now >= nextTimestamp)
  {
    unsigned long int nextConfig = 0;
    if (eventSizeIs32bit)
    {
      nextConfig = pgm_read_dword_near(event_states + current_event_index);
    }
    else
    {
      nextConfig = pgm_read_word_near(event_states + current_event_index);
    }

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
    Serial.println("######");
    Serial.print("now: ");
    Serial.println(now);
    Serial.print("ts: ");
    Serial.println(nextTimestamp);
    Serial.print("curidx: ");
    Serial.println(current_event_index);
    Serial.print("value: ");
    Serial.println(nextConfig);
#endif

    // send actions to outputs
    setValues(nextConfig);

    // iterate to next event timepoint
    current_event_index++;

    // end of sequence reached - start from beginning
    if (current_event_index >= event_cnt)
    {
      // introduce internal timer and reset it instead
      current_event_index = 0;

      // tell everybody this is a new run
      isNewRun = true;

#if defined(USE_RF24) && USE_RF24
      nextObeySend = 0;
#endif

      // turn all channels off
      resetOutputs();

      // jump to beginning of program, its a bit hacky but works
      //asm volatile ("  jmp 0");
      // actually just set timer to beginning
      setMillis(0);
    }

    // read timestamp of next action
    nextTimestamp = pgm_read_dword_near(event_timestamps + current_event_index);
  }
}

// set config values to either MCP or GPIO depending on config
void setValues(unsigned long int values)
{
  // for debug set the internal LED to the LSB (channel 1)
  digitalWrite(STATUS_LED, ((values >> 3) & 0x01) ? HIGH : LOW);

#if defined(USE_MCP) && USE_MCP
  // set all 16 channels at once - use binary mask to limit to 2bytes because one MCP cant handle more
  mcp.writeGPIOAB(values & 0xFF);
#else

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
  Serial.print("bin: ");
#endif

  // GPIO is limiting factor but start with bitposition at 0 (most right bit)
  for (int pin = FIRST_GPIO, bitposition = 0; pin <= LAST_GPIO; pin++, bitposition++)
  {
#if defined(USE_RELAY) && USE_RELAY
    // invert because in direct GPIO mode the relay board is off when HIGH
    const bool pinState = !((values >> bitposition) & 0x01);
#else
    const bool pinState = ((values >> bitposition) & 0x01);
#endif

#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
    Serial.print(pinState);
#endif

    digitalWrite(pin, pinState);
  }
#if defined(DEBUG_SERIAL) && DEBUG_SERIAL
  Serial.println();
#endif
#endif
}

// set all outputs to HIGH/off (e.g. during reset)
void resetOutputs()
{
#if defined(USE_MCP) && USE_MCP
  mcp.writeGPIOAB(0);
#else
  for (int pin = FIRST_GPIO; pin <= LAST_GPIO; ++pin)
  {
    setDigitalPin(pin, LOW);
  }
#endif
}
#endif

void setDigitalPin(int pin, bool value)
{
  if (pin >= FIRST_PIN && pin < LAST_PIN)
  {
#if defined(USE_MCP) && USE_MCP
    mcp.digitalWrite(pin, value);
#else
    // if using a relay board, the GPIO logic values are inverted
    // HIGH is off and LOW is on
#if defined(USE_RELAY) && USE_RELAY
    // invert logic
    digitalWrite(pin, !value);
#else
    digitalWrite(pin, value);
#endif
#endif
  }
}

void setMillis(unsigned long ms)
{
  extern unsigned long timer0_millis;
  ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
  {
    timer0_millis = ms;
  }
}
