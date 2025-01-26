#include <port_io.h>

typedef struct {
	int resolution;
	int otherOption;
} BootOptions;

BootOptions bootOptions = {0, 0}; // Default values

typedef void (*MenuAction)(void);


typedef struct {
	char *heading;
	MenuAction action;
	int selected; // 0: unselected, 1: selected
	//MenuAction submenu;
	struct BootMenu *submenu;
} MenuItem;


typedef struct {
	MenuItem *items;
	int count;
	int selected;
} BootMenu;


void videoModes() {
	clear_();
	print_("video modes");
}


void loadConfiguration2() {
	clear_();
	print_("audio things");
}

void resolution1(){
	clear_();
	print_("800x600 selected");
}


void resolution2(){
	clear_();
	print_("1024x768 selected");
}


MenuItem resolutionMenuItems[] =  {
	{"800x600", resolution1, 1, (void*)0},
	{"1024x768", resolution2, 0, (void*)0}
};

BootMenu resolutionMenu = {
	.items = resolutionMenuItems,
	.count = sizeof(resolutionMenuItems) / sizeof(MenuItem),
	.selected = 0
	};

void resolutionSubMenu() {
	clear_();
	//print_("resolution submenu");
	handleMenuSelection(&resolutionMenu);
}


int read_scancode(void) {
	while (!(inb(0x64) & 1));
	int out;
	while (inb(0x64) & 1) {
		out = inb(0x60);
	}
	return out;
}


unsigned short * textmemptr = (unsigned short *)0xB8000;
void placech(unsigned char c, int x, int y, int attr) {
	unsigned short *where;
	unsigned att = attr << 8;
	where = textmemptr + (y * 80 + x);
	*where = c | att;
}


int x = 0;
int y = 0;
int attr = 0x07;
void print_(char * str) {
	while (*str) {
		if (*str == '\n') {
			for (; x < 80; ++x) {
				placech(' ', x, y, attr);
			}
			x = 0;
			y += 1;
			if (y == 24) {
				y = 0;
			}
		} else {
			placech(*str, x, y, attr);
			x++;
			if (x == 80) {
				x = 0;
				y += 1;
				if (y == 24) {
					y = 0;
				}
			}
		}
		str++;
	}
}


void clear_() {
	x = 0;
	y = 0;
	for (int y = 0; y < 24; ++y) {
		for (int x = 0; x < 80; ++x) {
			placech(' ', x, y, 0x00);
		}
	}
}


void print_banner(char * str) {
	int len = 0;
	char *c = str;
	while (*c) {
		len++;
		c++;
	}
	int off = (80 - len) / 2;

	for (int i = 0; i < 80; ++i) {
		placech(' ', i, y, attr);
	}
	for (int i = 0; i < len; ++i) {
		placech(str[i], i + off, y, attr);
	}

	y++;
}


void displayMenu(const BootMenu* menu) {

	for(int i = 0; i < menu->count; i++) {
	
		if (i == menu->selected) {
			print_(" > ");
			//print_(menu->items[i].heading);
		} else {
		
			print_("   ");
			//print_(menu->items[i].heading);
		}
		
		if(menu->items[i].selected) {
			print_("[X] ");
		} else {
			print_("[ ] ");
		}
		print_(menu->items[i].heading);
		print_("\n");
	
	}

}



int handleMenuSelection(BootMenu* menu) {
	int running = 1;
	char choice;
	
	while (running) {

		clear_(); // clear the screen

		attr = 0x1f;
		print_banner("TheTaaJ Bootloader v1.0");

		attr = 0x07;
		print_("\n");

		displayMenu(menu);

		// Wait for user input
		int scan_code = read_scancode();

		switch (scan_code) {
		
			case 0x48: // up key
				if (menu->selected  > 0) {
					menu->selected--;
				}
				break;
			case 0x50: // down key
				if(menu->selected < menu->count - 1) {
					menu->selected++;
				}
				break;
			case 0x1c: // Enter key
				if(menu->items[menu->selected].submenu != (void*)0) {
					//menu->items[menu->selected].submenu();
					handleMenuSelection( menu->items[menu->selected].submenu);
					//print_("sub");
				//} else if (menu->items[menu->selected].action != (void*)0) {
					//menu->items[menu->selected].action();
					
					//return 1;
				} else {
					menu->items[menu->selected].selected = !menu->items[menu->selected].selected;
					if (menu == &resolutionMenu) {
						bootOptions.resolution = menu->selected;
					}
					}
				//running = 1;
				//break;
				break;
			case 0x01: // Escape key
				//running = 0;
				//break;
				return 1;
			default:
				break;
		}
	}
	return 0;

}


void DrawBootMenu () {
	MenuItem menuItems[] = {
	 {"Select Resolution", videoModes, 0, &resolutionMenu},
	 {"Other Options", loadConfiguration2, 0, (void*)0}
	};

	// Define the boot menu
	BootMenu bootMenu = {
		.items = menuItems,
		.count = sizeof(menuItems) / sizeof(MenuItem),
		.selected = 0
	};

	// int continueRunning = 1;
	// while(continueRunning) {

	// continueRunning =
	handleMenuSelection(&bootMenu);
	//}

}

