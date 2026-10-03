// dart_ascii_art.h

#ifndef DART_ASCII_ART_H
#define DART_ASCII_ART_H

// #include <avr/pgmspace.h> // REMOVE THIS LINE for ESP32

// Define the lines of your simplified dart target ASCII art as constant strings stored in Flash
// The 'const' keyword alone is often enough for ESP32 to place this data in Flash
const char DART_ASCII_ART_LINE_0[] = ".----."; // 8 chars wide
const char DART_ASCII_ART_LINE_1[] = "|(+)|"; // 8 chars wide
const char DART_ASCII_ART_LINE_2[] = "| @  |"; // 8 chars wide
const char DART_ASCII_ART_LINE_3[] = "`----'"; // 8 chars wide

// Create an array of pointers to these constant strings
// Again, 'const' helps ensure this is in Flash
const char* const DART_ASCII_ART[] = {
  DART_ASCII_ART_LINE_0,
  DART_ASCII_ART_LINE_1,
  DART_ASCII_ART_LINE_2,
  DART_ASCII_ART_LINE_3
};

// Define the number of lines
#define DART_ASCII_ART_NUM_LINES (sizeof(DART_ASCII_ART) / sizeof(DART_ASCII_ART[0]))

#endif // DART_ASCII