#include <Arduino.h>
#include <Wire1.h>

#include "pinout.h"
#include "segments.h"

// ---------- BACKGROUND DISPLAY MULTIPLEXING ---------- //

// buffers for display, raw and text
static char display[CATHODES];
static char textbuf[32];

// timer compare interrupt does multiplexing in the background
//#define USE_TIMER_1
#ifdef USE_TIMER_1
ISR(TIMER1_COMPA_vect) {
#else
ISR(TIMER2_COMPA_vect) {
#endif
  volatile static unsigned digit = 0;
  segments(0);                     // all segments off
  digitn(digit);             // turn on next digit
  segments(display[digit]);  // light up correct segments
  digit = (digit + 1) % CATHODES; // increment with overflow
}

void setup_display() {
  // setup cathodes and anodes as outputs
  for (uint8_t i = 0; i < CATHODES; i++) {
    pinMode(cathode[i], OUTPUT);
    digitalWrite(cathode[i], CATHODE_OFF);
    display[i] = 0x00;
  }
  for (uint8_t i = 0; i < ANODES; i++) {
    pinMode(anode[i], OUTPUT);
    digitalWrite(anode[i], ANODE_OFF);
  }

  // initialize timer and enable compare match interrupts for refresh
  noInterrupts();
#ifdef USE_TIMER_1
  TCCR1A = 0;
  TCCR1B = (1 << CS12) | (0 << CS11) | (1 << CS10) | (1 << WGM12); // run with /1024 prescaling, CTC mode
  TCNT1  = 0;
  OCR1A  = 8; // 8 --> ca. 1 kHz, faster becomes darker, slower begins to flicker
  TIMSK1 = (1 << OCIE1A); // activate comapre match A interrupt
#else
  TCCR2A = (1 << WGM21); // clear timer on compare match (CTC)
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); // run with /1024 prescaling
  TCNT2  = 0;
  OCR2A  = 8; // 8 --> ca. 1 kHz, faster becomes darker, slower begins to flicker
  TIMSK2 = (1 << OCIE2A); // activate comapre match A interrupt
#endif
  interrupts();

}

// ---------- SERIAL DATA INPUT ---------- //

// just print serial input as text line without animation
// newline clears everything and sets cursor to first digit
// needs to be called periodically in loop()
void serialLine() {
  static unsigned serialpos = 0;
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 0x0a) {
      memcpy(textbuf, "     \0", 6);
      serialpos = 0;
    } else {
      if (serialpos < 5) {
        textbuf[serialpos] = c;
        serialpos += 1;
      } else {
        for (unsigned i = 0; i < serialpos-1; i++) {
          textbuf[i] = textbuf[i+1];
        }
        textbuf[serialpos-1] = c;
      }
    }
  }
}

// ---------- DISPLAY FUNCTIONS ---------- //

// fill the segments with random noise
void noise() {
  for (unsigned i = 0; i < CATHODES; i++) {
    display[i] = random(255) & 0xfe; // mask the decimal dot
  }
}

// display a string on the display
void text(volatile const char *str) {
  for (unsigned i = 0; i < CATHODES && str[i] != 0; i++) {
    display[i] = lookup(str[i]);
  }
}


// ---------- MAIN ---------- //

// slider switch for mode selection inputs
#define SWITCH_I2C  20
#define SWITCH_SPI  21

#define I2C_ADDRESS 0x32
#define UART_SPEED  57600

enum PROTOCOL { PROTOCOL_I2C, PROTOCOL_SPI, PROTOCOL_UART };
PROTOCOL proto;

void setup() {

  // initialize pseudorandom number for noise()
  // THIS IS NOT CRYPTOGRAPHICALLY SECURE
  randomSeed(analogRead(0));
  
  // begin background multiplexing
  setup_display();

  // setup input pins for mode selection
  pinMode(SWITCH_I2C, INPUT_PULLUP);
  pinMode(SWITCH_SPI, INPUT_PULLUP);

  // decide which mode to use on powerup
  if (digitalRead(SWITCH_I2C) == LOW) {

    proto = PROTOCOL_I2C;
    text(" I2C ");
    Wire1.begin(I2C_ADDRESS);
    Wire1.onReceive(i2c_receiver);


  } else if (digitalRead(SWITCH_SPI) == LOW) {

    proto = PROTOCOL_SPI;
    text(" SPI ");
    // use software spi
    pinMode(13, INPUT_PULLUP);
    pinMode(11, INPUT_PULLUP);
    // enable pin change interrupt on PB5
    PCICR  |= 0b00000001;
    PCMSK0 |= 0b00100000;

  } else {

    proto = PROTOCOL_UART;
    text(" UART");
    Serial.begin(UART_SPEED);
    Serial.println("bubbledisp ready");

  }

}


// this software SPI has sync problems!
// maybe some other protocol than simple raw segments is needed
// or a sync frame like in dotstars is necessary?
volatile bool spiflag = false;
volatile char spibuf = 0x00;
volatile unsigned bit = 0;
volatile char buf = 0;
ISR(PCINT0_vect) {
  // on rising edges
  if (digitalRead(13) == HIGH) {
    // shift buffer left
    buf = buf << 1;
    buf |= (digitalRead(11) == HIGH);
    // sprintf(textbuf, "  %02d", bit);
    // text(textbuf);
    bit++;
    if (bit == 8) {
      spibuf = buf;
      spiflag = true;
      bit = 0;
    }
  }

}

void spi_receiver(char c) {
  static unsigned position = 0;
  static unsigned nullcounter = 0;
  display[position] = c;
  position++;
  if (c == 0x00) {
    if (nullcounter < 5)
      nullcounter++;
    else
      // always keep position at 0 if more than five nulls received
      position = 0;
  } else {
    nullcounter = 0;
  }
}

void i2c_receiver(int howmany) {

  // wat?
  if (howmany == 0) {
    return;
  }

  // only one byte is usually a read address
  static unsigned readaddr = 0;
  if (howmany == 1) {
    readaddr = Wire1.read();
    return;
  }

  // otherwise decide by port what to do
  char port = Wire1.read();
  unsigned position = 0;
  switch (port) {

    // write text to display
    default:
    case 0x00:
    case 0x01:
      while (Wire1.available()) {
        char c = Wire1.read();
        if (position < CATHODES) {
          if (port == 0x00) {
            // send HELLO:
            // i2ctransfer -y <dev> w6@0x32 0x00 $(python -c 'for c in "HELLO": print(f"0x{ord(c):02x}")')
            display[position] = lookup(c);
          } else {
            // send 'EƎ||EƎ':
            // i2ctransfer -y <dev> w6@0x32 0x01 0x9e 0xf2 0x6c 0x9e 0xf2
            display[position] = c;
          }
        }
        position += 1;
      }
      break;

    // configure multiplexing rate
    case 0x12:
      uint8_t val = Wire1.read();
      if (val == 0) { val = 1; }
#ifdef USE_TIMER_1
      OCR1A = val;
#else
      OCR2A = val;
#endif
      break;

  }

}




void loop() {

  switch (proto) {

    case PROTOCOL_UART:
      serialLine();
      text(textbuf);
      break;

    case PROTOCOL_I2C:
      // handled in i2c_receiver
      break;

    default:
      if (spiflag) {
        spi_receiver(spibuf);
        spiflag = false;
      }
      break;

  }

}
