// Copyright (c) 2020 Anton Semjonov
// Licensed under the MIT License

#pragma once
#include <Arduino.h>
#include "pinout.h"

//
// ---------- SEGMENTS ----------
//
// each digit needs exactly one byte where each bit
// is mapped to a particular segment anode:
//                             a
//      Ad╶┐┌╴Ae             ────
//     Ac╶┐││┌╴Af         f │    │
//    Ab╶┐││││┌╴Ag          │  g │ b
//   Aa╶┐││││││┌╴Adp  ❯❯     ────
//    0b00000000          e │    │
//                          │    │ c
//                           ────
//                             d   ■ dp
void segments(const uint8_t c) {
  digitalWrite(Aa,  (1 << 7) & c);
  digitalWrite(Ab,  (1 << 6) & c);
  digitalWrite(Ac,  (1 << 5) & c);
  digitalWrite(Ad,  (1 << 4) & c);
  digitalWrite(Ae,  (1 << 3) & c);
  digitalWrite(Af,  (1 << 2) & c);
  digitalWrite(Ag,  (1 << 1) & c);
  digitalWrite(Adp, (1 << 0) & c);
}

// character lookup for segment mapping
unsigned lookup(char ch) {
  switch (ch) {
    // numbers
    case '0': return 0b11111100;
    case '1': return 0b01100000;
    case '2': return 0b11011010;
    case '3': return 0b11110010;
    case '4': return 0b01100110;
    case '5': return 0b10110110;
    case '6': return 0b10111110;
    case '7': return 0b11100000;
    case '8': return 0b11111110;
    case '9': return 0b11110110;
    // alphabet
    case 'A': return 0b11101110;
    case 'a': return lookup('A');
    case 'B': return lookup('b');
    case 'b': return 0b00111110;
    case 'C': return 0b10011100;
    case 'c': return 0b00011010;
    case 'D': return lookup('d');
    case 'd': return 0b01111010;
    case 'E': return 0b10011110;
    case 'e': return lookup('E');
    case 'F': return 0b10001110;
    case 'f': return lookup('F');
    case 'G': return lookup('6');
    case 'g': return lookup('9');
    case 'H': return 0b01101110;
    case 'h': return 0b00101110;
    case 'I': return 0b00001100;
    case 'i': return 0b00001000;
    case 'J': return 0b01110000;
    case 'j': return lookup('J');
    case 'K': return lookup('k');
    case 'k': return 0b10101110;
    case 'L': return 0b00011100;
    case 'l': return lookup('I'); //           _  _
    case 'M': return 0b11001100;  // M + m =  | || |
    case 'm': return 0b11100100;  //          |    |
    case 'N': return lookup('n');
    case 'n': return 0b00101010;
    case 'O': return lookup('0');
    case 'o': return 0b00111010;
    case 'P': return 0b11001110;
    case 'p': return lookup('P');
    case 'Q': return lookup('q');
    case 'q': return 0b11100110;
    case 'R': return lookup('r');
    case 'r': return 0b00001010;
    case 'S': return 0b10110110;
    case 's': return lookup('S');
    case 'T': return lookup('t');
    case 't': return 0b00011110;
    case 'U': return 0b01111100;
    case 'u': return 0b00111000;
    case 'V': return lookup('U');
    case 'v': return lookup('u');
    case 'W': return 0b00111100; // like M + m
    case 'w': return 0b01111000; //
    case 'X': return lookup('H');
    case 'x': return lookup('H');
    case 'Y': return lookup('y');
    case 'y': return 0b01110110;
    case 'Z': return lookup('2');
    case 'z': return lookup('2');
    // symbols
    case '.': return 0b00000001;
   case '\'': return 0b00000100;
    case '-': return 0b00000010;
    case '_': return 0b00010000;
    case '>': return 0b00110000;
    case '<': return 0b00011000;
    case '=': return 0b00010010;
    case '~': return 0b10010010;
    // empty be default
    default: return 0;
  }
}

//
// ---------- DIGITS ----------
//
// simple digit activator with a bitmask, left is LSB
//
//    ┌╴fifth╶─────╴▯▯▯▯▮
//    │┌╴fourth╶───╴▯▯▯▮▯
//    ││┌╴third╶───╴▯▯▮▯▯
//    │││┌╴second╶─╴▯▮▯▯▯
//    ││││┌╴first╶─╴▮▯▯▯▯
//  0b00000
//
void digits(const uint8_t d) {
  digitalWrite(C1, !((1 << 0) & d));
  digitalWrite(C2, !((1 << 1) & d));
  digitalWrite(C3, !((1 << 2) & d));
  digitalWrite(C4, !((1 << 3) & d));
  digitalWrite(C5, !((1 << 4) & d));
}

// wrapper for the above with zero-indexed integers
inline void digitn(const uint8_t d) {
  digits(1 << d);
}

