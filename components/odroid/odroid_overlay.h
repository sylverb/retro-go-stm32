#pragma once

#include "stdbool.h"
#include "stdint.h"

typedef enum {
    ODROID_DIALOG_INIT,
    ODROID_DIALOG_PREV,
    ODROID_DIALOG_NEXT,
    ODROID_DIALOG_FOCUS_GAINED,
    ODROID_DIALOG_ENTER,
} odroid_dialog_event_t;

typedef enum {
    ODROID_DIALOG_IGNORE,
    ODROID_DIALOG_RETURN,
} odroid_dialog_cb_return_t;

typedef enum
{
    ODROID_MENU_CLOSED,
    ODROID_MENU_PENDING,
    ODROID_MENU_OPEN,
} odroid_menu_state_t;

typedef struct odroid_dialog_choice odroid_dialog_choice_t;

struct odroid_dialog_choice {
    int  id;
    const char *label;
    char *value;
    int  enabled;
    bool (*update_cb)(odroid_dialog_choice_t *, odroid_dialog_event_t, uint32_t repeat);
};

typedef void (*void_callback_t)();

#define ODROID_DIALOG_CHOICE_LAST {0x0F0F0F0F, "LAST", (char *)"LAST", 0xFFFF, NULL}

extern odroid_menu_state_t odroid_menu_state;

void odroid_overlay_init();
void odroid_overlay_set_font_size(int size);
int  odroid_overlay_get_font_size();
int  odroid_overlay_get_font_width();
int  odroid_overlay_draw_text(uint16_t x, uint16_t y, uint16_t width, const char *text, uint16_t color, uint16_t color_bg);
void odroid_overlay_draw_rect(int x, int y, int width, int height, int border, uint16_t color);
void odroid_overlay_draw_fill_rect(int x, int y, int width, int height, uint16_t color);
void odroid_overlay_draw_battery(int x, int y);
void odroid_overlay_draw_dialog(const char *header, odroid_dialog_choice_t *options, int sel);

int odroid_overlay_dialog(const char *header, odroid_dialog_choice_t *options, int selected, void_callback_t repaint);
int odroid_overlay_confirm(const char *text, bool yes_selected, void_callback_t repaint);
void odroid_overlay_alert(const char *text);

int odroid_overlay_settings_menu(odroid_dialog_choice_t *extra_options, void_callback_t repaint);
int odroid_overlay_game_settings_menu(odroid_dialog_choice_t *extra_options, void_callback_t repaint);
int odroid_overlay_game_menu(odroid_dialog_choice_t *extra_options, void_callback_t repaint);
int odroid_savestate_menu(const char *title, const char *rom_path, bool show_preview, void_callback_t repaint);
