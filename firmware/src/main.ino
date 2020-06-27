#include <Arduino.h>

#include "countof.h"
#include "pinout.h"
#include "segments.h"

// ---------- BACKGROUND DISPLAY MULTIPLEXING ---------- //

// counter and buffer for background refresh
volatile static uint8_t __isr_digit = 0;
volatile static uint8_t display[CATHODES];

//#define USE_TIMER_1

// timer compare interrupt does multiplexing in the background
#ifdef USE_TIMER_1
ISR(TIMER1_COMPA_vect) {
#else
ISR(TIMER2_COMPA_vect) {
#endif
  segments(0);                      // disable all segments
  digitn(__isr_digit);                 // turn on next digit
  segments(display[__isr_digit]);  // turn on correct segments
  __isr_digit = (__isr_digit + 1) % CATHODES;    // increment with overflow
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

static char serialbuf[64];
static char textbuf[64];
static unsigned serialpos = 0;

void serialLine() {

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 0x0a) {
      memcpy(textbuf, "_    \0", 6);
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

void serialRunner() {

  if (Serial.available() > 0) {

    char c = Serial.read();
    if (c == 0x0a) {
      serialbuf[serialpos] = 0x00;
      memcpy(textbuf, serialbuf, serialpos + 1);
      // for (unsigned i = 0; i <= serialpos; i++) {
      //   textbuf[i] = serialbuf[i];
      // }
      reset_textrunner();
      serialpos = 0;
    } else {
      serialbuf[serialpos] = c;
      serialpos += 1;
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

// testing refresh rates with buttons
void decreaseRefreshRate() {
  unsigned t = OCR2A / 1.17;
  OCR2A = (t < 1) ? 1 : t;
}
void increaseRefreshRate() {
  unsigned t = OCR2A * 1.17 + 1;
  OCR2A = (t > 255) ? 255 : t;
}

volatile unsigned __bh_debounce = 0;
void buttonHandler() {
  bool l = false; //left();
  bool r = false; //right();

  if (!(l || r)) {

    unsigned now = millis();
    if ((now - __bh_debounce) > 200) {

    switch (todo) {

      case REF_INC: increaseRefreshRate();
        break;

      case REF_DEC: decreaseRefreshRate();
        break;

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

void setup() {

  Serial.begin(57600);
  Serial.println("Hello, World!");

  // initialize pseudorandom number
  // THIS IS NOT CRYPTOGRAPHICALLY SECURE
  randomSeed(analogRead(0));
  
  // setup background multiplexing
  setup_display();

  // configure buttons as inputs
  //pinMode(LEFT,  INPUT_PULLUP);
  //pinMode(RIGHT, INPUT_PULLUP);

  // the button debounce caps take a short moment to charge
  //while (left() || right()) { };

  // attach button handlers
  //attachInterrupt(digitalPinToInterrupt(LEFT),  buttonHandler, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(RIGHT), buttonHandler, CHANGE);

  text("HELLO");

}


void loop() {

  if (mode != prevmode) {
    reset_reveal();
    reset_textrunner();
    prevmode = mode;
  }

  if (config) {
    char str[6];
    snprintf(str, 6, "oc%3d", OCR2A);
    text(str);
    delay(10);
    return;
  }

  switch (mode) {

    default:
    case NOISE:
      noise();
      break;

    case DICE:
      reveal(diceroll, 150);
      break;

    case ANTON:
      reveal("Anton", 250);
      break;

    case TEXT:

      // textrunner("rEd ALErt! ");

      // serialRunner();
      // textrunner(textbuf);

      serialLine();
      text(textbuf);

      break;

  }

}
