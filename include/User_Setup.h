#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED

#include "Constants.h"

// Driver for 1.28in round 240x240 panel
#define GC9A01_DRIVER

// SPI pin mapping (NodeMCU-32S)
#define TFT_SCLK PIN_TFT_SCLK
#define TFT_MOSI PIN_TFT_MOSI
#define TFT_CS PIN_TFT_CS
#define TFT_DC PIN_TFT_DC
#define TFT_RST PIN_TFT_RST

#define SPI_FREQUENCY 40000000

// Built-in bitmap fonts used by drawString/drawCentreString font IDs
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define SMOOTH_FONT

#endif