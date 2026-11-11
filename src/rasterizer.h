#pragma once

#if defined(_KERNEL_MODE)
#include <ntddk.h>
#else
#include <windows.h>
#endif
#include "driver.h"

// 32-bit ARGB Color Definitions
#define COLOR_BLACK         0xFF000000
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_RED           0xFFFF2222
#define COLOR_DARK_RED      0xFF880000
#define COLOR_GREEN         0xFF22FF22
#define COLOR_DARK_GREEN    0xFF008800
#define COLOR_BLUE          0xFF0055FF
#define COLOR_BSOD_BLUE     0xFF0078D7
#define COLOR_DARK_BLUE     0xFF002244
#define COLOR_NAVY          0xFF001122
#define COLOR_YELLOW        0xFFFFFF00
#define COLOR_GOLD          0xFFFFCC00
#define COLOR_ORANGE        0xFFFF8800
#define COLOR_CYAN          0xFF00FFFF
#define COLOR_DARK_CYAN     0xFF008888
#define COLOR_MAGENTA       0xFFFF00FF
#define COLOR_PURPLE        0xFF880088
#define COLOR_GRAY          0xFF888888
#define COLOR_LIGHT_GRAY    0xFFCCCCCC
#define COLOR_DARK_GRAY     0xFF333333
#define COLOR_TRANSPARENT   0x00000000

// 8x8 Monospace Bitmap Font (ASCII 32 ' ' to 126 '~')
// Each character consists of 8 bytes representing 8 horizontal scanlines (1 = pixel on, 0 = pixel off)
static const UCHAR g_Font8x8[96][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 32 ' '
    { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 }, // 33 '!'
    { 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 34 '"'
    { 0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00 }, // 35 '#'
    { 0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00 }, // 36 '$'
    { 0x00, 0x63, 0x66, 0x0C, 0x18, 0x33, 0x63, 0x00 }, // 37 '%'
    { 0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00 }, // 38 '&'
    { 0x30, 0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00 }, // 39 '\''
    { 0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00 }, // 40 '('
    { 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00 }, // 41 ')'
    { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 }, // 42 '*'
    { 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00 }, // 43 '+'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30 }, // 44 ','
    { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 }, // 45 '-'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 }, // 46 '.'
    { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 }, // 47 '/'
    { 0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00 }, // 48 '0'
    { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00 }, // 49 '1'
    { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00 }, // 50 '2'
    { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00 }, // 51 '3'
    { 0x0C, 0x1C, 0x34, 0x64, 0x7E, 0x04, 0x04, 0x00 }, // 52 '4'
    { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00 }, // 53 '5'
    { 0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00 }, // 54 '6'
    { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00 }, // 55 '7'
    { 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00 }, // 56 '8'
    { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00 }, // 57 '9'
    { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00 }, // 58 ':'
    { 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30 }, // 59 ';'
    { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 }, // 60 '<'
    { 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 }, // 61 '='
    { 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00 }, // 62 '>'
    { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00 }, // 63 '?'
    { 0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00 }, // 64 '@'
    { 0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00 }, // 65 'A'
    { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 }, // 66 'B'
    { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 }, // 67 'C'
    { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 }, // 68 'D'
    { 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00 }, // 69 'E'
    { 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00 }, // 70 'F'
    { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00 }, // 71 'G'
    { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 }, // 72 'H'
    { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // 73 'I'
    { 0x0E, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00 }, // 74 'J'
    { 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00 }, // 75 'K'
    { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 }, // 76 'L'
    { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 }, // 77 'M'
    { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 }, // 78 'N'
    { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // 79 'O'
    { 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00 }, // 80 'P'
    { 0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00 }, // 81 'Q'
    { 0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00 }, // 82 'R'
    { 0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00 }, // 83 'S'
    { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, // 84 'T'
    { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // 85 'U'
    { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, // 86 'V'
    { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 }, // 87 'W'
    { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 }, // 88 'X'
    { 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00 }, // 89 'Y'
    { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00 }, // 90 'Z'
    { 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00 }, // 91 '['
    { 0x40, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00 }, // 92 '\\'
    { 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00 }, // 93 ']'
    { 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 94 '^'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00 }, // 95 '_'
    { 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00 }, // 96 '`'
    { 0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00 }, // 97 'a'
    { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00 }, // 98 'b'
    { 0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00 }, // 99 'c'
    { 0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00 }, // 100 'd'
    { 0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00 }, // 101 'e'
    { 0x0E, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x00 }, // 102 'f'
    { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C }, // 103 'g'
    { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, // 104 'h'
    { 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // 105 'i'
    { 0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C }, // 106 'j'
    { 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00 }, // 107 'k'
    { 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // 108 'l'
    { 0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00 }, // 109 'm'
    { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, // 110 'n'
    { 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // 111 'o'
    { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60 }, // 112 'p'
    { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06 }, // 113 'q'
    { 0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00 }, // 114 'r'
    { 0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00 }, // 115 's'
    { 0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00 }, // 116 't'
    { 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00 }, // 117 'u'
    { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, // 118 'v'
    { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 }, // 119 'w'
    { 0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00 }, // 120 'x'
    { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C }, // 121 'y'
    { 0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00 }, // 122 'z'
    { 0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00 }, // 123 '{'
    { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 }, // 124 '|'
    { 0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00 }, // 125 '}'
    { 0x33, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }  // 126 '~'
};

// Shorthand color macros for sprite pixel art
#define _X_ COLOR_TRANSPARENT
#define _W_ COLOR_WHITE
#define _C_ COLOR_CYAN
#define _B_ COLOR_BLUE
#define _Y_ COLOR_YELLOW
#define _O_ COLOR_ORANGE
#define _R_ COLOR_RED
#define _K_ COLOR_DARK_RED
#define _G_ COLOR_GRAY
#define _D_ COLOR_DARK_GRAY
#define _M_ COLOR_MAGENTA
#define _P_ COLOR_PURPLE

// 16x16 Player Spaceship Sprite (32-bit ARGB)
static const ULONG g_SpritePlayerShip16x16[16 * 16] = {
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_W_,_W_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_C_,_W_,_W_,_C_,_X_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_C_,_C_,_W_,_W_,_C_,_C_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_C_,_C_,_B_,_W_,_W_,_B_,_C_,_C_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_C_,_C_,_B_,_B_,_W_,_W_,_B_,_B_,_C_,_C_,_X_,_X_,_X_,
    _X_,_X_,_C_,_C_,_B_,_B_,_B_,_W_,_W_,_B_,_B_,_B_,_C_,_C_,_X_,_X_,
    _X_,_C_,_C_,_B_,_B_,_B_,_B_,_W_,_W_,_B_,_B_,_B_,_B_,_C_,_C_,_X_,
    _C_,_C_,_B_,_B_,_B_,_B_,_B_,_W_,_W_,_B_,_B_,_B_,_B_,_B_,_C_,_C_,
    _C_,_B_,_B_,_B_,_B_,_B_,_B_,_C_,_C_,_B_,_B_,_B_,_B_,_B_,_B_,_C_,
    _C_,_B_,_B_,_D_,_D_,_D_,_B_,_C_,_C_,_B_,_D_,_D_,_D_,_B_,_B_,_C_,
    _C_,_B_,_D_,_D_,_D_,_D_,_D_,_C_,_C_,_D_,_D_,_D_,_D_,_D_,_B_,_C_,
    _C_,_C_,_D_,_O_,_O_,_D_,_D_,_C_,_C_,_D_,_D_,_O_,_O_,_D_,_C_,_C_,
    _X_,_C_,_D_,_O_,_Y_,_O_,_D_,_X_,_X_,_D_,_O_,_Y_,_O_,_D_,_C_,_X_,
    _X_,_X_,_X_,_Y_,_W_,_Y_,_X_,_X_,_X_,_X_,_Y_,_W_,_Y_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_Y_,_X_,_X_,_X_,_X_,_X_,_X_,_Y_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_
};

// 24x24 Boss Unit "BugCheck Overlord" Sprite (32-bit ARGB)
static const ULONG g_SpriteBoss24x24[24 * 24] = {
    _X_,_X_,_X_,_R_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_R_,_X_,_X_,_X_,
    _X_,_X_,_R_,_R_,_R_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_R_,_R_,_R_,_X_,_X_,
    _X_,_R_,_R_,_K_,_R_,_R_,_X_,_X_,_X_,_R_,_R_,_X_,_X_,_R_,_R_,_X_,_X_,_X_,_R_,_R_,_K_,_R_,_R_,_X_,
    _R_,_R_,_K_,_K_,_K_,_R_,_R_,_X_,_R_,_D_,_D_,_R_,_R_,_D_,_D_,_R_,_X_,_R_,_R_,_K_,_K_,_K_,_R_,_R_,
    _R_,_K_,_K_,_D_,_K_,_K_,_R_,_R_,_D_,_G_,_G_,_D_,_D_,_G_,_G_,_D_,_R_,_R_,_K_,_K_,_D_,_K_,_K_,_R_,
    _X_,_R_,_K_,_D_,_D_,_K_,_R_,_D_,_G_,_W_,_W_,_G_,_G_,_W_,_W_,_G_,_D_,_R_,_K_,_D_,_D_,_K_,_R_,_X_,
    _X_,_X_,_R_,_K_,_D_,_R_,_R_,_D_,_G_,_Y_,_Y_,_G_,_G_,_Y_,_Y_,_G_,_D_,_R_,_R_,_D_,_K_,_R_,_X_,_X_,
    _X_,_X_,_X_,_R_,_R_,_R_,_D_,_D_,_D_,_R_,_R_,_D_,_D_,_R_,_R_,_D_,_D_,_D_,_R_,_R_,_R_,_X_,_X_,_X_,
    _X_,_X_,_R_,_R_,_D_,_D_,_D_,_M_,_M_,_M_,_M_,_M_,_M_,_M_,_M_,_M_,_M_,_D_,_D_,_D_,_R_,_R_,_X_,_X_,
    _X_,_R_,_R_,_D_,_D_,_P_,_M_,_M_,_W_,_W_,_M_,_M_,_M_,_M_,_W_,_W_,_M_,_M_,_P_,_D_,_D_,_R_,_R_,_X_,
    _R_,_R_,_D_,_D_,_P_,_M_,_M_,_W_,_C_,_C_,_W_,_M_,_M_,_W_,_C_,_C_,_W_,_M_,_M_,_P_,_D_,_D_,_R_,_R_,
    _R_,_K_,_D_,_P_,_M_,_M_,_M_,_W_,_C_,_C_,_W_,_M_,_M_,_W_,_C_,_C_,_W_,_M_,_M_,_M_,_P_,_D_,_K_,_R_,
    _R_,_K_,_D_,_P_,_M_,_M_,_M_,_M_,_W_,_W_,_M_,_Y_,_Y_,_M_,_W_,_W_,_M_,_M_,_M_,_M_,_P_,_D_,_K_,_R_,
    _X_,_R_,_D_,_D_,_P_,_M_,_M_,_M_,_M_,_M_,_Y_,_W_,_W_,_Y_,_M_,_M_,_M_,_M_,_M_,_P_,_D_,_D_,_R_,_X_,
    _X_,_X_,_R_,_D_,_D_,_P_,_P_,_M_,_M_,_M_,_Y_,_W_,_W_,_Y_,_M_,_M_,_M_,_P_,_P_,_D_,_D_,_R_,_X_,_X_,
    _X_,_X_,_R_,_R_,_D_,_D_,_D_,_P_,_P_,_M_,_M_,_Y_,_Y_,_M_,_M_,_P_,_P_,_D_,_D_,_D_,_R_,_R_,_X_,_X_,
    _X_,_R_,_R_,_K_,_R_,_D_,_D_,_D_,_D_,_P_,_P_,_P_,_P_,_P_,_P_,_D_,_D_,_D_,_D_,_R_,_K_,_R_,_R_,_X_,
    _R_,_R_,_K_,_K_,_R_,_R_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_R_,_R_,_K_,_K_,_R_,_R_,
    _R_,_K_,_K_,_R_,_X_,_R_,_R_,_D_,_G_,_D_,_G_,_D_,_D_,_G_,_D_,_G_,_D_,_R_,_R_,_X_,_R_,_K_,_K_,_R_,
    _X_,_R_,_R_,_X_,_X_,_X_,_R_,_R_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_D_,_R_,_R_,_X_,_X_,_X_,_R_,_R_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_R_,_R_,_K_,_K_,_R_,_R_,_K_,_K_,_R_,_R_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_R_,_R_,_K_,_R_,_R_,_K_,_R_,_R_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_R_,_O_,_Y_,_Y_,_O_,_R_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,
    _X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_O_,_Y_,_Y_,_O_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_,_X_
};

// 4x8 Player Laser Sprite
static const ULONG g_SpritePlayerLaser4x8[4 * 8] = {
    _C_,_W_,_W_,_C_,
    _C_,_W_,_W_,_C_,
    _C_,_W_,_W_,_C_,
    _B_,_C_,_C_,_B_,
    _B_,_C_,_C_,_B_,
    _B_,_C_,_C_,_B_,
    _X_,_B_,_B_,_X_,
    _X_,_B_,_B_,_X_
};

// 8x8 Boss Plasma Orb Sprite
static const ULONG g_SpriteBossPlasma8x8[8 * 8] = {
    _X_,_X_,_R_,_R_,_R_,_R_,_X_,_X_,
    _X_,_R_,_O_,_Y_,_Y_,_O_,_R_,_X_,
    _R_,_O_,_Y_,_W_,_W_,_Y_,_O_,_R_,
    _R_,_Y_,_W_,_W_,_W_,_W_,_Y_,_R_,
    _R_,_Y_,_W_,_W_,_W_,_W_,_Y_,_R_,
    _R_,_O_,_Y_,_W_,_W_,_Y_,_O_,_R_,
    _X_,_R_,_O_,_Y_,_Y_,_O_,_R_,_X_,
    _X_,_X_,_R_,_R_,_R_,_R_,_X_,_X_
};

// 16x16 1-Bit Monochrome Shield Icon Bitmap
static const USHORT g_BitmapShield16x16[16] = {
    0x0000,
    0x0FF0,
    0x1FF8,
    0x3FFC,
    0x3FFC,
    0x3FFC,
    0x3FFC,
    0x3FFC,
    0x3FFC,
    0x1FF8,
    0x1FF8,
    0x0FF0,
    0x07E0,
    0x03C0,
    0x0180,
    0x0000
};

#undef _X_
#undef _W_
#undef _C_
#undef _B_
#undef _Y_
#undef _O_
#undef _R_
#undef _K_
#undef _G_
#undef _D_
#undef _M_
#undef _P_

// ============================================================================
// Rasterizer Primitives
// ============================================================================

// Plot single pixel safely with 2D bounds check
// Math: Offset = (y * Stride) + x
// ---------------------------------------------------------------------------
// Physical primitives: coordinates are real screen pixels (no canvas origin).
// These are the ONLY functions that touch video memory; everything is clipped
// against the real Width/Height here.
// ---------------------------------------------------------------------------
static __inline VOID Rasterizer_PlotPixelPhys(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG color
)
{
    if (!Fb || !Fb->VirtualBase || x >= Fb->Width || y >= Fb->Height) {
        return;
    }

    SIZE_T pixelOffset = ((SIZE_T)y * Fb->PixelsPerScanLine) + x;
    ((volatile ULONG*)Fb->VirtualBase)[pixelOffset] = color;
}

static __inline VOID Rasterizer_FillRectPhys(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG width,
    _In_ ULONG height,
    _In_ ULONG color
)
{
    if (!Fb || !Fb->VirtualBase || width == 0 || height == 0 || x >= Fb->Width || y >= Fb->Height) {
        return;
    }

    ULONG xMax = (width  > Fb->Width  - x) ? Fb->Width  : x + width;
    ULONG yMax = (height > Fb->Height - y) ? Fb->Height : y + height;

    volatile ULONG *vram = (volatile ULONG*)Fb->VirtualBase;
    for (ULONG py = y; py < yMax; py++) {
        SIZE_T rowStart = (SIZE_T)py * Fb->PixelsPerScanLine;
        for (ULONG px = x; px < xMax; px++) {
            vram[rowStart + px] = color;
        }
    }
}

// ---------------------------------------------------------------------------
// Logical primitives: coordinates are on the 1024x768 canvas, which is placed
// at (OriginX, OriginY) on the real screen. All higher-level drawing uses these.
// ---------------------------------------------------------------------------
static __inline VOID Rasterizer_PlotPixel(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG color
)
{
    if (!Fb) return;
    Rasterizer_PlotPixelPhys(Fb, x + Fb->OriginX, y + Fb->OriginY, color);
}

// Fill solid rectangle (logical canvas coordinates)
static __inline VOID Rasterizer_FillRect(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG width,
    _In_ ULONG height,
    _In_ ULONG color
)
{
    if (!Fb) return;
    Rasterizer_FillRectPhys(Fb, x + Fb->OriginX, y + Fb->OriginY, width, height, color);
}

// Draw hollow/bordered rectangle with specified border thickness
static __inline VOID Rasterizer_DrawRect(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG width,
    _In_ ULONG height,
    _In_ ULONG thickness,
    _In_ ULONG color
)
{
    if (!Fb || !Fb->VirtualBase || width == 0 || height == 0 || thickness == 0) {
        return;
    }

    // Top border
    Rasterizer_FillRect(Fb, x, y, width, thickness, color);
    // Bottom border
    if (height > thickness) {
        Rasterizer_FillRect(Fb, x, y + height - thickness, width, thickness, color);
    }
    // Left border
    Rasterizer_FillRect(Fb, x, y, thickness, height, color);
    // Right border
    if (width > thickness) {
        Rasterizer_FillRect(Fb, x + width - thickness, y, thickness, height, color);
    }
}

// Bresenham's Integer-Only Line Algorithm (pure integer, zero floating point)
static __inline VOID Rasterizer_DrawLine(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ LONG x0,
    _In_ LONG y0,
    _In_ LONG x1,
    _In_ LONG y1,
    _In_ ULONG color
)
{
    if (!Fb || !Fb->VirtualBase) return;

    LONG dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    LONG sx = (x0 < x1) ? 1 : -1;
    LONG dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0); // dy <= 0
    LONG sy = (y0 < y1) ? 1 : -1;
    LONG err = dx + dy;

    while (TRUE) {
        if (x0 >= 0 && y0 >= 0) {
            Rasterizer_PlotPixel(Fb, (ULONG)x0, (ULONG)y0, color);
        }
        if (x0 == x1 && y0 == y1) break;

        LONG e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Visible extent of the logical canvas (canvas size, clipped to the real screen)
static __inline ULONG Rasterizer_CanvasWidth(_In_ PFRAMEBUFFER_CONFIG Fb)
{
    ULONG avail = (Fb->Width > Fb->OriginX) ? Fb->Width - Fb->OriginX : 0;
    return (avail < CANVAS_WIDTH) ? avail : CANVAS_WIDTH;
}

static __inline ULONG Rasterizer_CanvasHeight(_In_ PFRAMEBUFFER_CONFIG Fb)
{
    ULONG avail = (Fb->Height > Fb->OriginY) ? Fb->Height - Fb->OriginY : 0;
    return (avail < CANVAS_HEIGHT) ? avail : CANVAS_HEIGHT;
}

// Clear entire screen to solid color
static __inline VOID Rasterizer_Clear(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG color
)
{
    if (!Fb || !Fb->VirtualBase) return;
    // Whole physical screen, not just the canvas
    Rasterizer_FillRectPhys(Fb, 0, 0, Fb->Width, Fb->Height, color);
}

// Draw single 8x8 character
static __inline VOID Rasterizer_DrawChar(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ CHAR c,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    if (!Fb || !Fb->VirtualBase || x >= Rasterizer_CanvasWidth(Fb) || y >= Rasterizer_CanvasHeight(Fb)) return;

    UCHAR ascii = (UCHAR)c;
    if (ascii < 32 || ascii > 126) {
        ascii = 32;
    }

    const UCHAR *glyph = g_Font8x8[ascii - 32];

    for (ULONG row = 0; row < 8; row++) {
        ULONG py = y + row;
        if (py >= Rasterizer_CanvasHeight(Fb)) break;

        UCHAR rowBits = glyph[row];
        for (ULONG col = 0; col < 8; col++) {
            ULONG px = x + col;
            if (px >= Rasterizer_CanvasWidth(Fb)) break;

            BOOLEAN isPixelSet = (rowBits & (0x80 >> col)) != 0;
            if (isPixelSet) {
                Rasterizer_PlotPixel(Fb, px, py, fgColor);
            } else if (bgColor != COLOR_TRANSPARENT) {
                Rasterizer_PlotPixel(Fb, px, py, bgColor);
            }
        }
    }
}

// Draw scaled character (scale >= 1)
static __inline VOID Rasterizer_DrawCharScaled(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ CHAR c,
    _In_ ULONG scale,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    if (!Fb || !Fb->VirtualBase || scale == 0 || x >= Rasterizer_CanvasWidth(Fb) || y >= Rasterizer_CanvasHeight(Fb)) return;

    UCHAR ascii = (UCHAR)c;
    if (ascii < 32 || ascii > 126) {
        ascii = 32;
    }

    const UCHAR *glyph = g_Font8x8[ascii - 32];

    for (ULONG row = 0; row < 8; row++) {
        ULONG py = y + (row * scale);
        if (py >= Rasterizer_CanvasHeight(Fb)) break;

        UCHAR rowBits = glyph[row];
        for (ULONG col = 0; col < 8; col++) {
            ULONG px = x + (col * scale);
            if (px >= Rasterizer_CanvasWidth(Fb)) break;

            BOOLEAN isPixelSet = (rowBits & (0x80 >> col)) != 0;
            if (isPixelSet) {
                Rasterizer_FillRect(Fb, px, py, scale, scale, fgColor);
            } else if (bgColor != COLOR_TRANSPARENT) {
                Rasterizer_FillRect(Fb, px, py, scale, scale, bgColor);
            }
        }
    }
}

// Draw string of ASCII characters
static __inline VOID Rasterizer_DrawString(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ const char *text,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    if (!Fb || !Fb->VirtualBase || !text) return;

    ULONG cursorX = x;
    ULONG cursorY = y;

    for (ULONG i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            cursorY += 10;
            cursorX = x;
            continue;
        }

        Rasterizer_DrawChar(Fb, cursorX, cursorY, text[i], fgColor, bgColor);
        cursorX += 8;

        if (cursorX + 8 > Rasterizer_CanvasWidth(Fb)) {
            cursorX = x;
            cursorY += 10;
        }
    }
}

// Draw scaled string of ASCII characters
static __inline VOID Rasterizer_DrawStringScaled(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ const char *text,
    _In_ ULONG scale,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    if (!Fb || !Fb->VirtualBase || !text || scale == 0) return;

    ULONG cursorX = x;
    ULONG cursorY = y;
    ULONG charWidth = 8 * scale;
    ULONG charHeight = 10 * scale;

    for (ULONG i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            cursorY += charHeight;
            cursorX = x;
            continue;
        }

        Rasterizer_DrawCharScaled(Fb, cursorX, cursorY, text[i], scale, fgColor, bgColor);
        cursorX += charWidth;

        if (cursorX + charWidth > Rasterizer_CanvasWidth(Fb)) {
            cursorX = x;
            cursorY += charHeight;
        }
    }
}

// Draw integer value without CRT/printf dependencies
static __inline VOID Rasterizer_DrawInt(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ LONG value,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    CHAR buf[16];
    ULONG idx = 0;
    BOOLEAN negative = FALSE;

    if (value < 0) {
        negative = TRUE;
        value = -value;
    }

    if (value == 0) {
        buf[idx++] = '0';
    } else {
        CHAR temp[16];
        ULONG tIdx = 0;
        while (value > 0) {
            temp[tIdx++] = (CHAR)('0' + (value % 10));
            value /= 10;
        }
        if (negative) {
            buf[idx++] = '-';
        }
        while (tIdx > 0) {
            buf[idx++] = temp[--tIdx];
        }
    }
    buf[idx] = '\0';
    Rasterizer_DrawString(Fb, x, y, buf, fgColor, bgColor);
}

// Draw hexadecimal value without CRT/printf dependencies
static __inline VOID Rasterizer_DrawHex(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG64 value,
    _In_ ULONG digits,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    CHAR buf[24];
    const CHAR hexChars[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    if (digits > 16) digits = 16;
    if (digits == 0) digits = 8;

    for (LONG i = (LONG)digits - 1; i >= 0; i--) {
        buf[2 + (digits - 1 - i)] = hexChars[(value >> (i * 4)) & 0xF];
    }
    buf[2 + digits] = '\0';
    Rasterizer_DrawString(Fb, x, y, buf, fgColor, bgColor);
}

// Draw graphical progress / health bar with border
static __inline VOID Rasterizer_DrawProgressBar(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG width,
    _In_ ULONG height,
    _In_ ULONG currentVal,
    _In_ ULONG maxVal,
    _In_ ULONG barColor,
    _In_ ULONG bgColor,
    _In_ ULONG borderColor
)
{
    if (!Fb || !Fb->VirtualBase || width < 4 || height < 4 || maxVal == 0) return;

    // Draw 1px border
    Rasterizer_DrawRect(Fb, x, y, width, height, 1, borderColor);

    // Inner background
    Rasterizer_FillRect(Fb, x + 1, y + 1, width - 2, height - 2, bgColor);

    // Filled portion
    if (currentVal > maxVal) currentVal = maxVal;
    ULONG fillWidth = (ULONG)(((ULONGLONG)(width - 2) * currentVal) / maxVal);
    if (fillWidth > 0) {
        Rasterizer_FillRect(Fb, x + 1, y + 1, fillWidth, height - 2, barColor);
    }
}

// Draw 32-bit ARGB Sprite with scaling and alpha transparency
static __inline VOID Rasterizer_DrawSprite(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ ULONG width,
    _In_ ULONG height,
    _In_ const ULONG *spriteData,
    _In_ ULONG scale
)
{
    if (!Fb || !Fb->VirtualBase || !spriteData || scale == 0 || x >= Rasterizer_CanvasWidth(Fb) || y >= Rasterizer_CanvasHeight(Fb)) {
        return;
    }

    for (ULONG sy = 0; sy < height; sy++) {
        ULONG py = y + (sy * scale);
        if (py >= Rasterizer_CanvasHeight(Fb)) break;

        for (ULONG sx = 0; sx < width; sx++) {
            ULONG px = x + (sx * scale);
            if (px >= Rasterizer_CanvasWidth(Fb)) break;

            ULONG color = spriteData[sy * width + sx];
            // Alpha test: 0x00xxxxxx is treated as transparent
            if ((color & 0xFF000000) != 0) {
                if (scale == 1) {
                    Rasterizer_PlotPixel(Fb, px, py, color);
                } else {
                    Rasterizer_FillRect(Fb, px, py, scale, scale, color);
                }
            }
        }
    }
}

// Draw 16x16 1-Bit Monochrome Bitmap Sprite with scaling
static __inline VOID Rasterizer_DrawBitmap16x16(
    _In_ PFRAMEBUFFER_CONFIG Fb,
    _In_ ULONG x,
    _In_ ULONG y,
    _In_ const USHORT bitmap[16],
    _In_ ULONG scale,
    _In_ ULONG fgColor,
    _In_ ULONG bgColor
)
{
    if (!Fb || !Fb->VirtualBase || !bitmap || scale == 0 || x >= Rasterizer_CanvasWidth(Fb) || y >= Rasterizer_CanvasHeight(Fb)) {
        return;
    }

    for (ULONG row = 0; row < 16; row++) {
        ULONG py = y + (row * scale);
        if (py >= Rasterizer_CanvasHeight(Fb)) break;

        USHORT rowBits = bitmap[row];
        for (ULONG col = 0; col < 16; col++) {
            ULONG px = x + (col * scale);
            if (px >= Rasterizer_CanvasWidth(Fb)) break;

            BOOLEAN isPixelSet = (rowBits & (0x8000 >> col)) != 0;
            if (isPixelSet) {
                if (scale == 1) {
                    Rasterizer_PlotPixel(Fb, px, py, fgColor);
                } else {
                    Rasterizer_FillRect(Fb, px, py, scale, scale, fgColor);
                }
            } else if (bgColor != COLOR_TRANSPARENT) {
                if (scale == 1) {
                    Rasterizer_PlotPixel(Fb, px, py, bgColor);
                } else {
                    Rasterizer_FillRect(Fb, px, py, scale, scale, bgColor);
                }
            }
        }
    }
}

// ============================================================================
// Back-buffered rendering
//
// All canvas drawing goes into a 1024x768 buffer in normal (non-paged) RAM.
// Rasterizer_Present then copies ONLY pixels that differ from what is already
// on screen (tracked in a shadow copy) into video memory. A typical frame
// changes a few thousand pixels instead of ~1.4 million bytes, which matters
// a lot in VMs where every VRAM write is tracked for the host display, and it
// removes the clear-then-redraw flicker.
// ============================================================================
#define CANVAS_PIXELS        ((SIZE_T)CANVAS_WIDTH * CANVAS_HEIGHT)
#define SHADOW_INVALID_PIXEL 0x00000001u   // Never produced by drawing (alpha 0)

typedef struct _RENDER_CONTEXT {
    PFRAMEBUFFER_CONFIG Screen;   // Real framebuffer (VRAM)
    FRAMEBUFFER_CONFIG  Back;     // Canvas-sized RAM buffer descriptor
    PULONG              Shadow;   // Last presented canvas contents
    BOOLEAN             Buffered; // FALSE = draw straight to Screen (fallback)
} RENDER_CONTEXT, *PRENDER_CONTEXT;

// Where game/UI drawing should go
static __inline PFRAMEBUFFER_CONFIG Render_Target(_In_ PRENDER_CONTEXT Rc)
{
    return Rc->Buffered ? &Rc->Back : Rc->Screen;
}

static __inline VOID Render_Init(
    _Out_ PRENDER_CONTEXT Rc,
    _In_ PFRAMEBUFFER_CONFIG Screen,
    _In_opt_ PULONG BackBuffer,
    _In_opt_ PULONG ShadowBuffer
)
{
    Rc->Screen = Screen;
    Rc->Shadow = ShadowBuffer;
    Rc->Buffered = (BackBuffer != NULL && ShadowBuffer != NULL);

    // The back buffer is exactly one canvas: no origin offset, full size
    Rc->Back = *Screen;
    Rc->Back.VirtualBase = BackBuffer;
    Rc->Back.MappedBase = BackBuffer;
    Rc->Back.Width = CANVAS_WIDTH;
    Rc->Back.Height = CANVAS_HEIGHT;
    Rc->Back.PixelsPerScanLine = CANVAS_WIDTH;
    Rc->Back.BufferSize = CANVAS_PIXELS * 4;
    Rc->Back.MappedSize = CANVAS_PIXELS * 4;
    Rc->Back.OriginX = 0;
    Rc->Back.OriginY = 0;

    if (Rc->Buffered) {
        // Force the first Present to write every visible canvas pixel
        for (SIZE_T i = 0; i < CANVAS_PIXELS; i++) {
            ShadowBuffer[i] = SHADOW_INVALID_PIXEL;
        }
    }
}

// Copy changed canvas pixels from the back buffer to the screen
static __inline VOID Render_Present(_In_ PRENDER_CONTEXT Rc)
{
    if (!Rc->Buffered) return;

    PFRAMEBUFFER_CONFIG scr = Rc->Screen;
    ULONG w = Rasterizer_CanvasWidth(scr);
    ULONG h = Rasterizer_CanvasHeight(scr);
    const ULONG *back = (const ULONG *)Rc->Back.VirtualBase;
    PULONG shadow = Rc->Shadow;
    volatile ULONG *vram = (volatile ULONG *)scr->VirtualBase;

    for (ULONG y = 0; y < h; y++) {
        SIZE_T rowCanvas = (SIZE_T)y * CANVAS_WIDTH;
        SIZE_T rowScreen = ((SIZE_T)(y + scr->OriginY) * scr->PixelsPerScanLine) + scr->OriginX;
        for (ULONG x = 0; x < w; x++) {
            ULONG c = back[rowCanvas + x];
            if (c != shadow[rowCanvas + x]) {
                shadow[rowCanvas + x] = c;
                vram[rowScreen + x] = c;
            }
        }
    }
}

