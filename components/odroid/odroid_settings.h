#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    ODROID_START_ACTION_RESUME = 0,
    ODROID_START_ACTION_NEWGAME,
    ODROID_START_ACTION_NETPLAY
} ODROID_START_ACTION;

typedef enum
{
    ODROID_REGION_AUTO = 0,
    ODROID_REGION_NTSC,
    ODROID_REGION_PAL
} ODROID_REGION;

void odroid_settings_init(void);
void odroid_settings_reset(void);
void odroid_settings_commit(void);

int32_t odroid_settings_FontSize_get();
void odroid_settings_FontSize_set(int32_t);

int32_t odroid_settings_Volume_get();
void odroid_settings_Volume_set(int32_t value);

char* odroid_settings_RomFilePath_get();
void odroid_settings_RomFilePath_set(const char* value);

int32_t odroid_settings_Backlight_get();
void odroid_settings_Backlight_set(int32_t value);

int32_t odroid_settings_StartupApp_get();
void odroid_settings_StartupApp_set(int32_t value);

void* odroid_settings_StartupFile_get(void);
void odroid_settings_StartupFile_set(void *value);

uint16_t odroid_settings_MainMenuTimeoutS_get(void);
void odroid_settings_MainMenuTimeoutS_set(uint16_t value);

uint16_t odroid_settings_MainMenuSelectedTab_get(void);
void odroid_settings_MainMenuSelectedTab_set(uint16_t value);

uint16_t odroid_settings_MainMenuCursor_get(void);
void odroid_settings_MainMenuCursor_set(uint16_t value);

ODROID_START_ACTION odroid_settings_StartAction_get();
void odroid_settings_StartAction_set(ODROID_START_ACTION value);

int32_t odroid_settings_AudioSink_get();
void odroid_settings_AudioSink_set(int32_t value);

ODROID_REGION odroid_settings_Region_get();
void odroid_settings_Region_set(ODROID_REGION value);

int32_t odroid_settings_Palette_get();
void odroid_settings_Palette_set(int32_t value);

int32_t odroid_settings_SpriteLimit_get();
void odroid_settings_SpriteLimit_set(int32_t value);

int32_t odroid_settings_DisplayScaling_get();
void odroid_settings_DisplayScaling_set(int32_t value);

int32_t odroid_settings_DisplayFilter_get();
void odroid_settings_DisplayFilter_set(int32_t value);

int32_t odroid_settings_DisplayRotation_get();
void odroid_settings_DisplayRotation_set(int32_t value);

int32_t odroid_settings_DisplayOverscan_get();
void odroid_settings_DisplayOverscan_set(int32_t value);

#if CHEAT_CODES == 1
bool odroid_settings_ActiveGameGenieCodes_is_enabled(uint32_t rom_id, int code_index);
bool odroid_settings_ActiveGameGenieCodes_set(uint32_t rom_id, int code_index, bool enable);
#endif

bool odroid_settings_DebugMenuDebugClockAlwaysOn_get();
void odroid_settings_DebugMenuDebugClockAlwaysOn_set(bool value);

/*** Generic functions ***/

void odroid_settings_string_set(const char *key, const char *value);
char* odroid_settings_string_get(const char *key, const char *default_value);

int32_t odroid_settings_int32_get(const char *key, int32_t value_default);
void odroid_settings_int32_set(const char *key, int32_t value);

int32_t odroid_settings_app_int32_get(const char *key, int32_t value_default);
void odroid_settings_app_int32_set(const char *key, int32_t value);
