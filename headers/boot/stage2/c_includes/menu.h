#ifndef __MENU_H__
#define __MENU_H__


typedef enum console_color {

	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	PURPLE,
	BROWN,
	GRAY,
	
	DARK_GRAY,
	BRIGHT_BLUE,
	BRIGHT_GREEN,
	BRIGHT_CYAN,
	BRIGHT_RED,
	MAGENTA,
	YELLOW,
	WHITE
} console_color;

const console_color c_BackgroundColor = BLACK;
const console_color c_TextColor = WHITE;

const console_color c_ItemColor = GRAY;
const console_color c_SelectedItemColor = WHITE;
const console_color c_ItemBackgroundColor = c_BackgroundColor;
const console_color c_SelectedItemBackgroundColor = GRAY;

const console_color c_ArrowColor = GRAY;

const console_color c_SliderColor = CYAN;
const console_color c_SliderBackgroundColor = DARK_GRAY;

const console_color c_TitleColor = MAGENTA;
const console_color c_TitleBackgroundColor = c_BackgroundColor;

typedef enum {
	MAIN_MENU = 1,
	CHOICE_MENU,
	STANDARD_MENU,
} menu_type;



typedef enum {
	MENU_ITEM_STANDARD = 1,
	MENU_ITEM_MARKABLE,
	MENU_ITEM_TITLE,
	MENU_ITEM_NO_CHOICE,
	MENU_ITEM_SEPARATOR,
} menu_item_type;

typedef struct MenuItem MenuItem;
typedef struct {
	char* title;
	int count;
	int isHidden;
	menu_type type;

//	int selected;

	MenuItem* head;
	MenuItem* tail;

	// Selected menu item
	MenuItem *selectedItem;

	int itemCount;
} Menu;

typedef void (*menu_item_hook)(Menu* menu, MenuItem* item);

struct MenuItem {
	menu_item_type type;

	Menu* fMenu;
	Menu* subMenu;

	char* label;
	int isSelected;
	int isMarked;
	char* helpText;
	struct MenuItem* prev;
	struct MenuItem* next;

	menu_item_hook target;

};


Menu* Menu_Create(menu_type type, const char* title);
void Menu_AddItem(Menu* menu, MenuItem* item);

MenuItem* MenuItem_Create(const char* label, Menu* subMenu);

MenuItem* FindSelected(Menu* menu, int * selected);

void MenuItem_SetTarget();

void create_menu();


void run_menu(Menu* menu);


// Console
void Console_WriteString(const char* str);
void console_set_cursor(int x, int y);
void console_set_color(int foreground, int background);

void print_centered(int line, const char* text);
void print_item_at(int line, MenuItem* item);
#endif
