#include <Arduino.h>
#include <Wire1.h>

#include "pinout.h"
#include "segments.h"

// ---------- BACKGROUND DISPLAY MULTIPLEXING ---------- //

// counter and buffer for background refresh
volatile static uint8_t __isr_digit = 0;
volatile static uint8_t display[CATHODES];

// timer compare interrupt does multiplexing in the background
//#define USE_TIMER_1
#ifdef USE_TIMER_1
ISR(TIMER1_COMPA_vect) {
#else
ISR(TIMER2_COMPA_vect) {
#endif
  segments(0);                     // all segments off
  digitn(__isr_digit);             // turn on next digit
  segments(display[__isr_digit]);  // light up correct segments
  __isr_digit = (__isr_digit + 1) % CATHODES; // increment with overflow
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
  OCR1A  = 27; // 8 --> ca. 1 kHz, faster becomes darker, slower begins to flicker
  TIMSK1 = (1 << OCIE1A); // activate comapre match A interrupt
#else
  TCCR2A = (1 << WGM21); // clear timer on compare match (CTC)
  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); // run with /1024 prescaling
  TCNT2  = 0;
  OCR2A  = 27; // 8 --> ca. 1 kHz, faster becomes darker, slower begins to flicker
  TIMSK2 = (1 << OCIE2A); // activate comapre match A interrupt
#endif
  interrupts();

}


// ---------- SERIAL DATA INPUT ---------- //

static char textbuf[64];
static unsigned serialpos = 0;

// just print serial input as text line without animation
// newline clears everything and sets cursor to first digit
void serialLine() {
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
volatile unsigned __noise_delay = 50;
volatile char diceroll[CATHODES+1];
void noise() {
  for (unsigned i = 0; i < CATHODES; i++) {
    display[i] = random(255) & 0xfe; // mask the decimal dot
  }
  delay(__noise_delay);
}

// display a string on the display
void text(volatile const char *str) {
  for (unsigned i = 0; i < CATHODES && str[i] != 0; i++) {
    display[i] = lookup(str[i]);
  }
}

volatile unsigned __tr_offset = 0;
volatile unsigned __tr_delay = 180;
volatile bool __tr_freeze = false;
void reset_textrunner() {
  __tr_offset = 0;
  __tr_freeze = false;
}
void textrunner(const char *str) {
  static char __tr_buf[CATHODES];
  if (__tr_freeze) return;
  for (unsigned i = 0; i < CATHODES; i++) {
    __tr_buf[i] = str[(i + __tr_offset) % strlen(str)];
  }
  text(__tr_buf);
  __tr_offset = (__tr_offset + 1) % strlen(str);
  delay(__tr_delay);
}

volatile bool __rv_done = false;
volatile unsigned __rv_step = 0;
volatile unsigned __rv_digit = 0;
void reset_reveal() {
  __rv_step = 0;
  __rv_digit = 0;
  __rv_done = false;
}
void reveal(volatile const char *str, const unsigned steplen) {
  if (__rv_done) return;
  if (__rv_step >= (steplen / (__noise_delay != 0 ? __noise_delay : 1))) {
    if (__rv_digit == CATHODES) {
      __rv_done = true;
      return;
    } else {
      display[__rv_digit] = lookup(str[__rv_digit]);
      __rv_digit++;
      __rv_step = 0;
    };
  }
  for (unsigned i = __rv_digit; i < CATHODES; i++) {
    display[i] = random(255) & 0xfe;
  }
  __rv_step += 1;
  delay(__noise_delay);
}

// ---------- STATE MACHINES ---------- //



enum modes { NOISE, DICE, ANTON, TEXT, _N_MODES_ };
volatile static unsigned mode = TEXT;
volatile static unsigned prevmode = mode;
volatile static bool config = false;

enum todos { NONE, REF_INC, REF_DEC, MODE, CONFIG, ROLL };
volatile static todos todo = NONE;

volatile unsigned __bh_debounce = 0;
void buttonHandler() {
  bool l = false; //left();
  bool r = false; //right();

  if (!(l || r)) {

    unsigned now = millis();
    if ((now - __bh_debounce) > 200) {

    switch (todo) {

      case CONFIG:
        config = !config;
        reset_reveal();
        reset_textrunner();
        break;

      case ROLL:
        snprintf((char*)diceroll, 6, "%5lu", random(99999));
        reset_reveal();
        mode = DICE;
        break;

      case MODE:
        switch (mode) {
          case DICE:
            mode = NOISE;
            break;
          case NOISE:
            mode = NOISE + 2;
            break;
          default:
            mode = (modes)((mode + 1) % _N_MODES_);
            break;
        };
        break;

      default:
        break;

    }

    __bh_debounce = now;
    }
    todo = NONE;
    return;
  }

  if (l && r) {
    todo = CONFIG;
    return;
  }

  if (!(todo == MODE || todo == CONFIG)) {

    if (config) {
      if (l) todo = REF_DEC;
      if (r) todo = REF_INC;
    } else {
      switch (mode) {

        case NOISE:
          if (l) todo = MODE;
          if (r) todo = ROLL;
          break;

        case DICE:
          if (l) todo = MODE;
          if (r) todo = ROLL;
          break;

        case ANTON:
          if (l) todo = MODE;
          if (r) reset_reveal();
          break;

        case TEXT:
          if (l) todo = MODE;
          if (r) __tr_freeze = !__tr_freeze;

        default:
          break;

      }
    }
  }

}


//
// ---------- MAIN ----------
//

// slider switch for mode selection inputs
#define SWITCH_I2C  20
#define SWITCH_SPI  21

enum PROTOCOL { PROTOCOL_I2C, PROTOCOL_SPI, PROTOCOL_UART };
static PROTOCOL proto;

void setup() {

  // initialize pseudorandom number
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
    text("  I2C");
    delay(1000);
    Wire1.begin(0x32);
    text("Adr32");
    delay(1000);
    memcpy(textbuf, "00000\0", 6);
    Wire1.onReceive(i2c_receiver);

  } else if (digitalRead(SWITCH_SPI) == LOW) {

    proto = PROTOCOL_SPI;
    text("  SPI");

  } else {

    proto = PROTOCOL_UART;
    text(" UART");
    Serial.begin(57600);
    Serial.println("bubbledisp ready");

  }
  delay(1000);


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
  unsigned int pos = 0;
  switch (port) {

    // write text to display
    default:
    case 0x00:
      // send HELLO:  i2ctransfer -y 8 w6@0x32 0x00 $(python -c 'for c in "HELLO": print(f"0x{ord(c):02x}")')
      while (Wire1.available()) {
        textbuf[pos] = Wire1.read();
        pos += 1;
      }
      text(textbuf);
      break;

    // configure multiplexing rate
    case 0x12:
#ifdef USE_TIMER_1
      OCR1A = Wire1.read();
#else
      OCR2A = Wire1.read();
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
      noise();
      break;

  }

}
