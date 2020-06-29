// Copyright (c) 2020 Anton Semjonov
// Licensed under the MIT License

#pragma once
#include <stddef.h>

// helper to get the length of a const array at compile time
// https://stackoverflow.com/a/13375970
template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];
#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))

// cathode pins
#define C1   6
#define C2   5
#define C3  19
#define C4  16
#define C5  26
const uint8_t cathode[] = { C1, C2, C3, C4, C5 };
#define CATHODES countof(cathode)
#define CATHODE_ON  LOW
#define CATHODE_OFF !CATHODE_ON

// anode pins
#define Aa   7
#define Ab   3
#define Ac   2
#define Ad  15
#define Ae   4
#define Af  18
#define Ag  14
#define Adp 17
const uint8_t anode[] = { Aa, Ab, Ac, Ad, Ae, Af, Ag, Adp };
#define ANODE_ON  HIGH
#define ANODE_OFF !ANODE_ON
#define ANODES countof(anode)
