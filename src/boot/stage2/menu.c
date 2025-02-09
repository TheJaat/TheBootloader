#include <menu.h>
#include <port_io.h>

// memory allocation things
#define MEMORY_POOL_SIZE 4096*4 // 4KB

char memory_pool[MEMORY_POOL_SIZE];

static next_free_byte = 0;

typedef unsigned int size_t;

void* menu_malloc(size_t size) {
	if(next_free_byte + size > MEMORY_POOL_SIZE) {
		return (void*)0;
	}

	void* ptr = &memory_pool[next_free_byte];
	next_free_byte += size;
	return ptr;
}



Menu* Menu_Create(menu_type type, const char* title) {

	Menu* menu = (Menu*) menu_malloc(sizeof(Menu));

	// Initialization
	menu->type = type;
	menu->head = (void*)0;
	menu->tail = (void*)0;
	menu->selectedItem = (void*)0;
	menu->itemCount = 0;

	// Hide the menu by default
	menu->isHidden = 1;
	
	if(title != (void*)0) {
		menu->title = (char*)menu_malloc(strlen(title) + 1);
		for (int i = 0; i<= strlen(title); i++) {
			menu->title[i] = title[i];
		}
		
	} else {
		menu->title = (void*)0;
	}

	return menu;
}

MenuItem* FindSelected(Menu* menu, int* selected) {

	MenuItem *currentItem = menu->head;

	while (currentItem != (void*)0) {
		*selected++;
		if (currentItem->isSelected) {
			return currentItem;
		}
		currentItem = currentItem->next;
	}

	return (void*)0;

}


int strlen(const char* str) {

	int count = 0;
	
	while(*str){
		count++;
		str++;
	}
	return count;
}

MenuItem* MenuItem_Create(const char* label, Menu* subMenu) {
	MenuItem* item = (MenuItem*) menu_malloc(sizeof(MenuItem));

	// Initialization
	item->type = MENU_ITEM_STANDARD;
	item->subMenu = subMenu;//(void*)0;
	item->prev = (void*)0;
	item->next = (void*)0;
	item->isSelected = 0;

	item->target = (void*)0;

	int length = strlen(label) + 1;
	char *duplicate = (char*)menu_malloc(length);
	for(int i = 0; i < length; i++){
	
		duplicate[i] = label[i];
	}
	item->label = duplicate;

	return item;
}

void Menu_AddItem(Menu* menu, MenuItem* item) {

	if (menu->head == (void*)0){
		// list is empty, initialize head & tail
		menu->head = item;
		menu->tail = item;
	} else {
		menu->tail->next = item;
		item->prev = menu->tail;
		menu->tail = item;
	}

	menu->itemCount++;

	// If selectedItem is NULL, set it to the newly added item
	if (menu->selectedItem == (void*)0) {
		menu->selectedItem = item;
	}
}


void Menu_AddSeparatorItem(Menu* menu) {
	MenuItem* item = (MenuItem*) menu_malloc(sizeof(MenuItem));

	// TODO, check if it is not null

	item->type = MENU_ITEM_SEPARATOR;

	Menu_AddItem(menu, item);
}


void Hide_Menu(Menu* menu) {
	menu->isHidden = 1;
}


void Show_Menu(Menu* menu) {
	menu->isHidden = 0;
}

int Is_MenuHidden(Menu* menu) {
	return menu->isHidden;
}


Menu* subMenues() {

	Menu* sub = Menu_Create(CHOICE_MENU, "Sub Menues");
	
	MenuItem* subitem1 = MenuItem_Create("SubItem1", (void*)0);
	MenuItem* subitem2 = MenuItem_Create("SubItem2", (void*)0);
	MenuItem* subitem3 = MenuItem_Create("SubItem3", (void*)0);
	
	Menu_AddItem(sub, subitem1);
	Menu_AddItem(sub, subitem2);
	Menu_AddItem(sub, subitem3);
	return sub;
}

void system_reboot(Menu* menu, MenuItem* item) {
	console_set_color(c_TextColor, c_BackgroundColor);
	Console_Clear_Screen();

	Console_WriteString("Todo reboot the system");

	while(1){}
}


void create_menu() {
	Menu* menu = Menu_Create(MAIN_MENU, "Boot Main Menu");
	
	// Create Main Menu Items
	MenuItem* item1 = MenuItem_Create("Item1", subMenues());
	MenuItem* item2 = MenuItem_Create("Item2", (void*)0);
	
	MenuItem* item3 = MenuItem_Create("Item3", (void*)0);
	MenuItem* item4 = MenuItem_Create("Item4", (void*)0);
	
	MenuItem* item5 = MenuItem_Create("Item5", (void*)0);
	MenuItem* item6 = MenuItem_Create("Item6", (void*)0);
	MenuItem* item7 = MenuItem_Create("Item7", (void*)0);
	MenuItem* item8 = MenuItem_Create("Item8", (void*)0);
	MenuItem* item9 = MenuItem_Create("Item9", (void*)0);
	MenuItem* item10 = MenuItem_Create("Item10", (void*)0);
	MenuItem* item11 = MenuItem_Create("Item11", (void*)0);
	MenuItem* item12 = MenuItem_Create("Item12", (void*)0);
	MenuItem* item13 = MenuItem_Create("Item13", (void*)0);
	MenuItem* item14 = MenuItem_Create("Item14", (void*)0);
	MenuItem* item15 = MenuItem_Create("Item15", (void*)0);

	MenuItem* reboot = MenuItem_Create("Reboot", (void*)0);


	MenuItem* item16 = MenuItem_Create("Item16", (void*)0);
	MenuItem* item17 = MenuItem_Create("Item17", (void*)0);
	MenuItem* item18 = MenuItem_Create("Item18", (void*)0);
	
	MenuItem* item19 = MenuItem_Create("Item19", (void*)0);
	MenuItem* item20 = MenuItem_Create("Item20", (void*)0);
	MenuItem* item21 = MenuItem_Create("Item21", (void*)0);
	MenuItem* item22 = MenuItem_Create("Item22", (void*)0);
	MenuItem* item23 = MenuItem_Create("Item23", (void*)0);
	MenuItem* item24 = MenuItem_Create("Item24", (void*)0);
	MenuItem* item25 = MenuItem_Create("Item25", (void*)0);
	MenuItem* item26 = MenuItem_Create("Item26", (void*)0);
	
	
	// Add items to the main menu
	Menu_AddItem(menu, item1);
	Menu_AddItem(menu, item2);
	
	Menu_AddItem(menu, item3);
	Menu_AddItem(menu, item4);
	Menu_AddItem(menu, item5);
	Menu_AddItem(menu, item6);
	Menu_AddItem(menu, item7);
	Menu_AddItem(menu, item8);
	Menu_AddItem(menu, item9);
	Menu_AddItem(menu, item10);
	Menu_AddItem(menu, item11);
	Menu_AddItem(menu, item12);
	Menu_AddItem(menu, item13);
	Menu_AddItem(menu, item14);
	Menu_AddItem(menu, item15);

	MenuItem_SetTarget(reboot, system_reboot);
	Menu_AddItem(menu, reboot);

	Menu_AddItem(menu, item16);
	Menu_AddItem(menu, item17);
	Menu_AddItem(menu, item18);
	Menu_AddItem(menu, item19);
	Menu_AddItem(menu, item20);
	Menu_AddItem(menu, item21);
	Menu_AddItem(menu, item22);
	Menu_AddItem(menu, item23);
	Menu_AddItem(menu, item24);
	Menu_AddItem(menu, item25);
	Menu_AddItem(menu, item26);
	
	run_menu(menu);
}


// console things
unsigned short* s_ScreenBase = (unsigned short*) 0xb8000;
static unsigned int s_ScreenWidth = 80;
static unsigned int s_ScreenHeight = 25;
static unsigned int s_ScreenOffset = 0;
static unsigned short s_Color = 0x0f00;


void MenuItem_SetTarget(MenuItem *item, menu_item_hook target) {
	item->target = target;
}


void console_set_color(int foreground, int background) {

	s_Color = (background & 0xf) << 12 | (foreground & 0xf) << 8;
}

void console_set_cursor(int x, int y) {

	
	if (y >= (int) s_ScreenHeight)
		y = s_ScreenHeight - 1;
	else if (y < 0)
		y = 0;
	if (x >= (int) s_ScreenWidth)
		x = s_ScreenWidth - 1;
	else if (x < 0)
		x = 0;

	s_ScreenOffset = x + y * s_ScreenWidth;

}


void print_centered(int line, const char* text) {

	int x = s_ScreenWidth / 2 - strlen(text) / 2;
	int  y = line;
	
	if (y >= (int) s_ScreenHeight)
		y = s_ScreenHeight - 1;
	else if (y < 0)
		y = 0;
	if (x >= (int) s_ScreenWidth)
		x = s_ScreenWidth - 1;
	else if (x < 0)
		x = 0;

	s_ScreenOffset = x + y * s_ScreenWidth;

	//console_set_color(c_TextColor, c_BackgroundColor);
	//s_Color = 0x0f00;
	Console_WriteString(text);
}

const g_FirstLine = 8;
const int g_OffsetX = 10;
const int g_HelpLines = 3;
int g_MenuOffset = 0;

int menu_height() {
	return 25 - g_FirstLine - 1 - g_HelpLines;  // returns 13
}


void print_item_at(int line, MenuItem* item) {

	int selected = item->isSelected;

	line -= g_MenuOffset;
	if (line < 0 || line >= menu_height()) {
		return;
	}
	
	console_color background = selected ? c_SelectedItemBackgroundColor: c_ItemBackgroundColor;
	console_color foreground = selected ? c_SelectedItemColor : c_ItemColor;
	
	console_set_cursor(g_OffsetX, line + g_FirstLine);
	console_set_color(foreground, background);

	int length = strlen(item->label) + 1;

	Console_WriteString(" ");
	
	Console_WriteString(item->label);


	// If it has submenu
	if (item->subMenu && item->subMenu->type == CHOICE_MENU) {
		const char *text = " (Current: )";
		Console_WriteString(text);
		length += strlen(text);
	
	}
	
	// print spacing
	for (int i = 0; i < s_ScreenWidth - length - 2 * g_OffsetX; i++) {
		Console_WriteString(" ");
	}
	
	if(!selected) {
		return;
	}

}


void* memcpy(void *dest, const void *src, int n) {

	unsigned char *d = (unsigned char*) dest;
	const unsigned char *s = (const unsigned char*) src;

	while(n--) {
		*d++ = *s++;
	}

	return dest;
}


void scroll_up() {
	memcpy (s_ScreenBase, s_ScreenBase + s_ScreenWidth, s_ScreenWidth * s_ScreenHeight * 2 - s_ScreenWidth * 2 );

	s_ScreenOffset = (s_ScreenHeight - 1) * s_ScreenWidth;

	for (unsigned int i = 0; i < s_ScreenWidth; i++) {
		s_ScreenBase[s_ScreenOffset + i] = s_Color | ' ';
	}
}


void Console_Clear_Screen() {

	for (unsigned int i = 0; i < s_ScreenWidth * s_ScreenHeight; i++) {
		s_ScreenBase[i] = s_Color;
	}

	s_ScreenOffset = 0;

}


void Console_WriteString(const char* str) {

	while(*str) {
	
		if (*str == '\n') {
			s_ScreenOffset += s_ScreenWidth - (s_ScreenOffset % s_ScreenWidth);
		} else {
			s_ScreenBase[s_ScreenOffset++] = s_Color | *str;
		}
		
		if (s_ScreenOffset >= s_ScreenWidth * s_ScreenHeight) {
			scroll_up();
		}
		
		str++;
	}

}

void Console_Putc(int c) {
	char character = (char)c;

	Console_WriteString(character);

}

void Console_WriteAt(const void* buffer, size_t bufferSize) {
	const char* string = (const char*) buffer;
	
	for (unsigned int i = 0; i < bufferSize; i++){
	
		if(string[0] == '\n'){
			s_ScreenOffset += s_ScreenWidth - (s_ScreenOffset % s_ScreenWidth);
		} else {
			s_ScreenBase[s_ScreenOffset++] = s_Color | string[0];
		}
	
		if(s_ScreenOffset >= s_ScreenWidth * s_ScreenHeight) {
			//scroll_up();
		}
		
		string++;
	}
	
	//return bufferSize;
}


void select_next_item(Menu* menu) {
	menu->selectedItem->isSelected = 0;
	// If no item is currently selected.
	if (menu->selectedItem == (void*)0) {
		menu->selectedItem = menu->head;
	} else {
		// Move to the next item in the list
		if (menu->selectedItem->next != (void*)0) {
			menu->selectedItem = menu->selectedItem->next;
		} else {
			// If at the end of the list, optionally loop back (wrap around) to the first item.
			menu->selectedItem = menu->head;
		}
	}
	menu->selectedItem->isSelected = 1;
}


void select_previous_item(Menu* menu) {
	menu->selectedItem->isSelected = 0;
	// If no item is currently selected.
	if (menu->selectedItem == (void*)0) {
		menu->selectedItem = menu->head;
	} else {
		// Move to the next item in the list
		if (menu->selectedItem->prev != (void*)0) {
			menu->selectedItem = menu->selectedItem->prev;
		} else {
			// If at the end of the list, optionally loop back to the first item.
			menu->selectedItem = menu->tail;
		}
	}
	menu->selectedItem->isSelected = 1;
}


void draw_common_things() {

	console_set_color(c_TextColor, c_BackgroundColor);
	Console_Clear_Screen();
	
	console_set_color(c_TextColor, c_BackgroundColor);
	print_centered(1, "The Jaat Loader");
	print_centered(2, ".....");

}


void draw_menu(Menu* menu) {

	MenuItem *currentItem = menu->head;
	int line = 0;

	// Print title
	if (menu->title != (void*)0) {
		console_set_cursor(g_OffsetX, g_FirstLine - 2);
		console_set_color(c_TitleColor, c_TitleBackgroundColor);
		Console_WriteString(menu->title);
	}

	while (currentItem != (void*)0) {
		// Check if it is separator
		if (currentItem->type == MENU_ITEM_SEPARATOR) {
			Console_WriteString("\n");
			line++;
			continue;
		}
		print_item_at(line, currentItem);
		
		line++;
		currentItem = currentItem->next;
	}

/*	int height = menu_height();  // 13
	if (menu->itemCount >= height) {
		int x = s_ScreenWidth - g_OffsetX; // g_OffsetX = 10, x = 80 - 10 = 70
		console_set_cursor(x, g_FirstLine); // g_FirstLine = 8
					// It updates s_ScreenOffset to 70, 8 location
		console_set_color(c_ArrowColor, c_BackgroundColor);
		//Console_WriteString("^");
		Console_Putc(30); //30
		height--;

		int start = g_MenuOffset * height / menu->itemCount;
			// 0 * 13/26 = 0
		int end = (g_MenuOffset + height) * height / menu->itemCount;

		for (int i = 0; i < height; i++) {
			if (i >= start && i <= end) {
				console_set_color(WHITE, c_SliderColor);
			} else {
				console_set_color(WHITE, c_SliderBackgroundColor);
			}
			Console_Putc(' ');
		}
	
		console_set_color(c_ArrowColor, c_BackgroundColor);
		Console_Putc(31);
	
	}*/

}


int read_key_scancode () {
	while (!(inb(0x64) & 1));
	int out;
	while (inb(0x64) & 1) {
		out = inb(0x60);
	}
	return out;

}


void make_item_visible(Menu *menu, int selected) {

	if(g_MenuOffset > selected || g_MenuOffset + menu_height() <= selected) {
		if (g_MenuOffset > selected)
			g_MenuOffset = selected;
		else
			g_MenuOffset = selected + 1 - menu_height();
	
	}

}

void invoke_item(Menu* menu, MenuItem* item) {

	// enter the submenu
	if (item->subMenu != (void*)0) {
		int temp_Offset = g_MenuOffset;
		Hide_Menu(menu);
		run_menu(item->subMenu);
		Console_WriteString("exit");
		// Restore current menu
		//g_MenuOffset = temp_Offset;
		//Hide_Menu(item->subMenu);
		Show_Menu(menu);
		//draw_menu(menu);
	} else if (item->target != (void*)0) {
		item->target(menu, item);
	
	}

}

void run_menu(Menu* menu) {
	g_MenuOffset = 0;

	// Show the menu
	Show_Menu(menu);
	
	// Get selected menu item and selected index (1-based)
	int selected = 0;
	MenuItem* selectedItem = FindSelected(menu, &selected);
	if (selectedItem == (void*)0) { // If any item is not selected,
					// select the first item.
		MenuItem *item = menu->head;
		if (item != (void*)0) {
			item->isSelected = 1;
		}
	}
//	make_item_visible(menu, selected);

	selected = 1;

	while (1) {
		int isEscape = 0;
		draw_common_things();

		if (!Is_MenuHidden(menu)) {
			draw_menu(menu);
		} else {
			return;
		}

		int scancode = read_key_scancode();
		switch (scancode) {
			case 0x48: // up key
				select_previous_item(menu);
				selected--;
				if (selected <= 0) {
					selected = menu->itemCount;
				}
				if (menu->itemCount > menu_height()) {
					if (selected == menu->itemCount) {
						g_MenuOffset = menu->itemCount - menu_height();
					} else if (selected <= g_MenuOffset) {
						g_MenuOffset--;
					}
				}
				break;
			case 0x50: // down key
				selected++;
				select_next_item(menu);
				if (selected > menu->itemCount) {
					selected = 1;
					g_MenuOffset = 0;
				}

				if (selected > menu_height()) {
					g_MenuOffset++;
				}
				break;
			case 0x01: // escape key
				if (menu->type != MAIN_MENU) {
					isEscape = 1;
				}
				break;
			case 0x1C: // Return key
				invoke_item(menu, menu->selectedItem);
				break;
			default:
				break;
		}
		if (isEscape) {
			break;
		}
	}
	Hide_Menu(menu);
}

