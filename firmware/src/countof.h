#include <stddef.h>

// helper to get the length of a const array at compile time
// https://stackoverflow.com/a/13375970
template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];
#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))