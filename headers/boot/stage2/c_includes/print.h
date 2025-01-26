#ifndef _PRINT_H
#define _PRINT_H


// Foreground colors
enum VGA_ForegroundColor {
    FG_BLACK        = 0x0,
    FG_BLUE         = 0x1,
    FG_GREEN        = 0x2,
    FG_CYAN         = 0x3,
    FG_RED          = 0x4,
    FG_MAGENTA      = 0x5,
    FG_BROWN        = 0x6,
    FG_LIGHT_GRAY   = 0x7,
    FG_DARK_GRAY    = 0x8,
    FG_LIGHT_BLUE   = 0x9,
    FG_LIGHT_GREEN  = 0xA,
    FG_LIGHT_CYAN   = 0xB,
    FG_LIGHT_RED    = 0xC,
    FG_LIGHT_MAGENTA= 0xD,
    FG_YELLOW       = 0xE,
    FG_WHITE        = 0xF
};

// Background colors
enum VGA_BackgroundColor {
    BG_BLACK        = 0x0 << 4,
    BG_BLUE         = 0x1 << 4,
    BG_GREEN        = 0x2 << 4,
    BG_CYAN         = 0x3 << 4,
    BG_RED          = 0x4 << 4,
    BG_MAGENTA      = 0x5 << 4,
    BG_BROWN        = 0x6 << 4,
    BG_LIGHT_GRAY   = 0x7 << 4
};

// Combine foreground and background colors
#define VGA_COLOR(fg, bg) ((fg) | (bg))


/* VGA driver */
extern void boot_clear_screen();
extern void boot_print_char_at(unsigned char, int, int);
extern void boot_print(unsigned char*);
extern void boot_print_hex(unsigned int);
extern void boot_ushort_print_hex(unsigned short);
extern void set_attribute(enum VGA_BackgroundColor bg, enum VGA_ForegroundColor fg);
extern void init_print_stage2();


#endif
