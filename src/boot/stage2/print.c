#include <print.h>
#include <port_io.h>
#include <revision.h>  // for build information


#define SCREEN_WIDTH 80		// Mode 3 Screen Width | Column
#define SCREEN_HEIGHT 25	// Mode 3 Screen Height | Row
#define VIDEO_MEMORY 0xB8000	// Video Memory in VGA mode

unsigned short *videoMem = (unsigned short*) VIDEO_MEMORY;

unsigned char attribute = VGA_COLOR(FG_BLACK, BG_LIGHT_GRAY);
int row = 0;
int column = 0;


/*
 * move_cursor
 * Update the hardware cursor
 */
void move_cursor() {
	unsigned temp;
	temp = column * SCREEN_WIDTH + row;
	
	/*
	 * Write stuff out.
	 */
	outb(0x3D4, 14);
	outb(0x3D5, temp >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, temp);
}


void set_attribute(enum VGA_BackgroundColor bg, enum VGA_ForegroundColor fg) {

	attribute = bg | fg;
}


void boot_scroll() {
    // Scroll up by copying each line up one row
    for (int i = 0; i < SCREEN_HEIGHT - 1; ++i) {
        for (int j = 0; j < SCREEN_WIDTH; ++j) {
            videoMem[i * SCREEN_WIDTH + j] = videoMem[(i + 1) * SCREEN_WIDTH + j];
        }
    }

    // Clear the last line
    for (int j = 0; j < SCREEN_WIDTH; ++j) {
        videoMem[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + j] = (attribute << 8) | ' ';
    }
}


void boot_clear_screen() {
    unsigned short blank = (attribute << 8) | ' ';
    for (int row = 0; row < SCREEN_HEIGHT; ++row) {
        for (int column = 0; column < SCREEN_WIDTH; ++column) {
            videoMem[row * SCREEN_WIDTH + column] = blank;
        }
    }

// Reset global row and column to 0.    
    row = 0;
    column = 0;
    move_cursor();
}

void boot_print_char_at(unsigned char c, int row, int col) {

	// Get Offset by row and column
	int offset = row * SCREEN_WIDTH + col;

	videoMem[offset] = (attribute << 8) | c;
}


void boot_print(unsigned char *str) {
    while (*str) {
        if (*str == '\n') {
            // Move to the next line and reset column
            row++;
            column = 0;
        } else {
            // Print character at current position
            boot_print_char_at(*str, row, column);
            column++;
            if (column >= SCREEN_WIDTH) {
                // Move to the next line if the end of the current line is reached
                column = 0;
                row++;
            }
        }

        // Scroll the screen if necessary
        if (row >= SCREEN_HEIGHT) {
        	// Either scroll or do wraparound
            boot_scroll();
            row = SCREEN_HEIGHT - 1;
        }

        str++;
    }
}


void boot_print_hex(unsigned int value) {
    char hex_chars[] = "0123456789ABCDEF";
    char hex_str[9]; // 8 hex digits + null terminator
    hex_str[8] = '\0'; // Null terminator

    for (int i = 7; i >= 0; --i) {
        hex_str[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    boot_print(hex_str);
}
void boot_ushort_print_hex(unsigned short value) {
    char hex_chars[] = "0123456789ABCDEF";
    char hex_str[5]; // 8 hex digits + null terminator
    hex_str[8] = '\0'; // Null terminator

    for (int i = 3; i >= 0; --i) {
        hex_str[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    boot_print(hex_str);
}


void boot_print_int(int value) {
    char buffer[12]; // Enough for an int (including sign and null terminator)
    int i = 10;
    buffer[11] = '\0';

    if (value == 0) {
        boot_print("0");
        return;
    }

    int isNegative = (value < 0);
    if (isNegative) {
        value = -value;
    }

    while (value > 0 && i > 0) {
        buffer[i--] = (value % 10) + '0';
        value /= 10;
    }

    if (isNegative) {
        buffer[i--] = '-';
    }

    boot_print(&buffer[i + 1]);
}


void init_print_stage2() {

    set_attribute(BG_LIGHT_GRAY, FG_MAGENTA);
    boot_clear_screen();

    boot_print("Welcome to the console.\n");
    boot_print("");

// Print Build Information:
    boot_print("\nBuild Information:\n");
    boot_print("Date: ");
    boot_print(BUILD_DATE);
    boot_print("\n");

    boot_print("Time: ");
    boot_print(BUILD_TIME);
    boot_print("\n");

    boot_print("System: ");
    boot_print(BUILD_SYSTEM);
    boot_print("\n");

    boot_print("Version: ");
    boot_print_int(REVISION_MAJOR);
    boot_print(".");
    boot_print_int(REVISION_MINOR);
    boot_print(".");
    boot_print_int(REVISION_BUILD);
    boot_print("\n");


/*	boot_print("\n");

	boot_print("second line.");

	for(int i=0; i<24; i++){
		boot_print("\n");
		boot_print_hex(i);
	}
*/
}
