/***************************************************************************

  M.A.M.E.UI  -  Multiple Arcade Machine Emulator with User Interface
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse,
  Copyright (C) 2003-2007 Chris Kirmse and the MAME32/MAMEUI team.

  This file is part of MAMEUI, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.

 ***************************************************************************/

 /***************************************************************************

  mui_opts.c

  Stores global options and per-game options;

***************************************************************************/

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <winreg.h>
#include <commctrl.h>

// standard C headers
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#include <direct.h>
#include <emu.h>
#include <emuopts.h>
#include <stddef.h>
#include <tchar.h>

// MAME/MAMEUI headers
#include "bitmask.h"
#include "winui.h"
#include "mui_util.h"
#include "treeview.h"
#include "splitters.h"
#include "mui_opts.h"
#include "winmain.h"
#include "winutf8.h"
#include "strconv.h"
#include "clifront.h"
#include "translate.h"

#ifdef MESS
#include "optionsms.h"
#endif // MESS

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

/***************************************************************************
    Internal function prototypes
 ***************************************************************************/

// static void LoadFolderFilter(int folder_index,int filters);

static file_error LoadSettingsFile(core_options *opts, const char *utf8filename);
static file_error SaveSettingsFile(core_options *opts, core_options *baseopts, const char *utf8_filename);

static void LoadOptionsAndSettings(void);

static void  CusColorEncodeString(const COLORREF *value, char* str);
static void  CusColorDecodeString(const char* str, COLORREF *value);

static void  SplitterEncodeString(const int *value, char* str);
static void  SplitterDecodeString(const char *str, int *value);

static void  FontEncodeString(const LOGFONTW *f, char *str);
static void  FontDecodeString(const char* str, LOGFONTW *f);

static void TabFlagsEncodeString(int data, char *str);
static void TabFlagsDecodeString(const char *str, int *data);

//static DWORD DecodeFolderFlags(const char *buf);
//static const char * EncodeFolderFlags(DWORD value);

static void ResetToDefaults(core_options *opts, int priority);

static void ui_parse_ini_file(core_options *opts, const char *name);
static void remove_all_source_options(void);


#ifdef _MSC_VER
#define snprintf _snprintf
#endif

/***************************************************************************
    Internal defines
 ***************************************************************************/

#define UI_INI_FILENAME MAMEUINAME ".ini"
#define DEFAULT_OPTIONS_INI_FILENAME CONFIGNAME ".ini"

#define MUIOPTION_LIST_MODE						"list_mode"
#define MUIOPTION_CHECK_GAME					"check_game"
#define MUIOPTION_JOYSTICK_IN_INTERFACE			"joystick_in_interface"
#define MUIOPTION_KEYBOARD_IN_INTERFACE			"keyboard_in_interface"
#define MUIOPTION_CYCLE_SCREENSHOT				"cycle_screenshot"
#define MUIOPTION_STRETCH_SCREENSHOT_LARGER		"stretch_screenshot_larger"
#define MUIOPTION_SCREENSHOT_BORDER_SIZE		"screenshot_bordersize"
#define MUIOPTION_SCREENSHOT_BORDER_COLOR		"screenshot_bordercolor"
#define MUIOPTION_INHERIT_FILTER				"inherit_filter"
#define MUIOPTION_OFFSET_CLONES					"offset_clones"
#ifdef USE_SHOW_SPLASH_SCREEN
#define MUIOPTION_DISPLAY_SPLASH_SCREEN			"display_splash_screen"
#endif /* USE_SHOW_SPLASH_SCREEN */
#ifdef TREE_SHEET
#define MUIOPTION_SHOW_TREE_SHEET			"show_tree_sheet"
#endif /* TREE_SHEET */
#define MUIOPTION_BROADCAST_GAME_NAME			"broadcast_game_name"
#define MUIOPTION_RANDOM_BACKGROUND				"random_background"
#define MUIOPTION_DEFAULT_FOLDER_ID				"default_folder_id"
#define MUIOPTION_SHOW_IMAGE_SECTION			"show_image_section"
#define MUIOPTION_SHOW_FOLDER_SECTION			"show_folder_section"
#define MUIOPTION_HIDE_FOLDERS					"hide_folders"
#define MUIOPTION_SHOW_STATUS_BAR				"show_status_bar"
#define MUIOPTION_SHOW_TABS						"show_tabs"
#define MUIOPTION_SHOW_TOOLBAR					"show_tool_bar"
#define MUIOPTION_CURRENT_TAB					"current_tab"
#define MUIOPTION_WINDOW_X						"window_x"
#define MUIOPTION_WINDOW_Y						"window_y"
#define MUIOPTION_WINDOW_WIDTH					"window_width"
#define MUIOPTION_WINDOW_HEIGHT					"window_height"
#define MUIOPTION_WINDOW_STATE					"window_state"
#define MUIOPTION_CUSTOM_COLOR					"custom_color"
#define MUIOPTION_FOLDER_FLAG					"folder_flag"
#define MUIOPTION_USE_BROKEN_ICON				"use_broken_icon"
#define MUIOPTION_LIST_FONT						"list_font"
#define MUIOPTION_LIST_FONTFACE						"list_fontface"
#define MUIOPTION_TEXT_COLOR					"text_color"
#define MUIOPTION_CLONE_COLOR					"clone_color"
#define MUIOPTION_BROKEN_COLOR					"broken_color"
#define MUIOPTION_HIDE_TABS						"hide_tabs"
#define MUIOPTION_HISTORY_TAB					"history_tab"
#define MUIOPTION_COLUMN_WIDTHS					"column_widths"
#define MUIOPTION_COLUMN_ORDER					"column_order"
#define MUIOPTION_COLUMN_SHOWN					"column_shown"
#define MUIOPTION_SPLITTERS						"splitters"
#define MUIOPTION_SORT_COLUMN					"sort_column"
#define MUIOPTION_SORT_REVERSED					"sort_reversed"
#if 0 //mamep
#define MUIOPTION_LANGUAGE						"language"
#endif
#define MUIOPTION_FLYER_DIRECTORY				"flyer_directory"
#define MUIOPTION_CABINET_DIRECTORY				"cabinet_directory"
#define MUIOPTION_MARQUEE_DIRECTORY				"marquee_directory"
#define MUIOPTION_TITLE_DIRECTORY				"title_directory"
#define MUIOPTION_CPANEL_DIRECTORY				"cpanel_directory"
#define MUIOPTION_PCB_DIRECTORY				    "pcb_directory"
#ifdef USE_VIEW_PCBINFO
#define MUIOPTION_PCBINFO_DIRECTORY				    "pcbinfo_directory"
#endif /* USE_VIEW_PCBINFO */
#define MUIOPTION_ICONS_DIRECTORY				"icons_directory"
#define MUIOPTION_BACKGROUND_DIRECTORY			"background_directory"
#define MUIOPTION_FOLDER_DIRECTORY				"folder_directory"
#define MUIOPTION_UI_KEY_UP						"ui_key_up"
#define MUIOPTION_UI_KEY_DOWN					"ui_key_down"
#define MUIOPTION_UI_KEY_LEFT					"ui_key_left"
#define MUIOPTION_UI_KEY_RIGHT					"ui_key_right"
#define MUIOPTION_UI_KEY_START					"ui_key_start"
#define MUIOPTION_UI_KEY_PGUP					"ui_key_pgup"
#define MUIOPTION_UI_KEY_PGDWN					"ui_key_pgdwn"
#define MUIOPTION_UI_KEY_HOME					"ui_key_home"
#define MUIOPTION_UI_KEY_END					"ui_key_end"
#define MUIOPTION_UI_KEY_SS_CHANGE				"ui_key_ss_change"
#define MUIOPTION_UI_KEY_HISTORY_UP				"ui_key_history_up"
#define MUIOPTION_UI_KEY_HISTORY_DOWN			"ui_key_history_down"
#define MUIOPTION_UI_KEY_CONTEXT_FILTERS		"ui_key_context_filters"
#define MUIOPTION_UI_KEY_SELECT_RANDOM			"ui_key_select_random"
#define MUIOPTION_UI_KEY_GAME_AUDIT				"ui_key_game_audit"
#define MUIOPTION_UI_KEY_GAME_PROPERTIES		"ui_key_game_properties"
#define MUIOPTION_UI_KEY_HELP_CONTENTS			"ui_key_help_contents"
#define MUIOPTION_UI_KEY_UPDATE_GAMELIST		"ui_key_update_gamelist"
#define MUIOPTION_UI_KEY_VIEW_FOLDERS			"ui_key_view_folders"
#define MUIOPTION_UI_KEY_VIEW_FULLSCREEN		"ui_key_view_fullscreen"
#define MUIOPTION_UI_KEY_VIEW_PAGETAB			"ui_key_view_pagetab"
#define MUIOPTION_UI_KEY_VIEW_PICTURE_AREA		"ui_key_view_picture_area"
#define MUIOPTION_UI_KEY_VIEW_STATUS			"ui_key_view_status"
#define MUIOPTION_UI_KEY_VIEW_TOOLBARS			"ui_key_view_toolbars"
#define MUIOPTION_UI_KEY_VIEW_TAB_CABINET		"ui_key_view_tab_cabinet"
#define MUIOPTION_UI_KEY_VIEW_TAB_CPANEL		"ui_key_view_tab_cpanel"
#define MUIOPTION_UI_KEY_VIEW_TAB_FLYER			"ui_key_view_tab_flyer"
#define MUIOPTION_UI_KEY_VIEW_TAB_HISTORY		"ui_key_view_tab_history"
#ifdef STORY_DATAFILE
#define MUIOPTION_UI_KEY_VIEW_TAB_STORY			"ui_key_view_tab_story"
#endif /* STORY_DATAFILE */
#define MUIOPTION_UI_KEY_VIEW_TAB_MARQUEE		"ui_key_view_tab_marquee"
#define MUIOPTION_UI_KEY_VIEW_TAB_SCREENSHOT	"ui_key_view_tab_screenshot"
#define MUIOPTION_UI_KEY_VIEW_TAB_TITLE			"ui_key_view_tab_title"
#define MUIOPTION_UI_KEY_VIEW_TAB_PCB   		"ui_key_view_tab_pcb"
#define MUIOPTION_UI_KEY_QUIT					"ui_key_quit"
#define MUIOPTION_UI_JOY_UP						"ui_joy_up"
#define MUIOPTION_UI_JOY_DOWN					"ui_joy_down"
#define MUIOPTION_UI_JOY_LEFT					"ui_joy_left"
#define MUIOPTION_UI_JOY_RIGHT					"ui_joy_right"
#define MUIOPTION_UI_JOY_START					"ui_joy_start"
#define MUIOPTION_UI_JOY_PGUP					"ui_joy_pgup"
#define MUIOPTION_UI_JOY_PGDWN					"ui_joy_pgdwn"
#define MUIOPTION_UI_JOY_HOME					"ui_joy_home"
#define MUIOPTION_UI_JOY_END					"ui_joy_end"
#define MUIOPTION_UI_JOY_SS_CHANGE				"ui_joy_ss_change"
#define MUIOPTION_UI_JOY_HISTORY_UP				"ui_joy_history_up"
#define MUIOPTION_UI_JOY_HISTORY_DOWN			"ui_joy_history_down"
#define MUIOPTION_UI_JOY_EXEC					"ui_joy_exec"
#define MUIOPTION_EXEC_COMMAND					"exec_command"
#define MUIOPTION_EXEC_WAIT						"exec_wait"
#define MUIOPTION_HIDE_MOUSE					"hide_mouse"
#define MUIOPTION_FULL_SCREEN					"full_screen"

#ifdef MESS
// Options names
#define MUIOPTION_DEFAULT_GAME					"default_system"
#define MUIOPTION_HISTORY_FILE					"sysinfo_file"
#define MUIOPTION_MAMEINFO_FILE					"messinfo_file"
// Option values
#define MUIDEFAULT_SELECTION					"nes"
#define MUIDEFAULT_SPLITTERS					"152,310,468"
#define MUIHISTORY_FILE							"sysinfo.dat"
#define MUIMAMEINFO_FILE						"messinfo.dat"
#else
// Options names
#define MUIOPTION_DEFAULT_GAME					"default_game"
//#define MUIOPTION_HISTORY_FILE					"history_file"
//#define MUIOPTION_MAMEINFO_FILE					"mameinfo_file"
// Options values
#define MUIDEFAULT_SELECTION					"puckman"
#define MUIDEFAULT_SPLITTERS					"152,362"
#define MUIHISTORY_FILE							"history.dat"
#define MUIMAMEINFO_FILE						"mameinfo.dat"
#ifdef STORY_DATAFILE
#define MUISTORY_FILE   						"story.dat"
#endif /* STORY_DATAFILE */
#endif



/***************************************************************************
    Internal structures
 ***************************************************************************/

#if 0
typedef struct
{
	int folder_index;
	int filters;
} folder_filter_type;
#endif

/***************************************************************************
    Internal variables
 ***************************************************************************/

static object_pool *options_memory_pool;

static core_options *settings;

static core_options *global = NULL;			// Global 'default' options

// UI options in mameui.ini
static const options_entry regSettings[] =
{
	// UI options
	{ NULL,									NULL,       OPTION_HEADER,     "DISPLAY STATE OPTIONS" },
	{ MUIOPTION_DEFAULT_GAME,				MUIDEFAULT_SELECTION, 0,       NULL },
//	{ MUIOPTION_DEFAULT_GAME,				"puckman",  0,                 NULL },
	{ MUIOPTION_DEFAULT_FOLDER_ID,			"0",        0,                 NULL },
	{ MUIOPTION_SHOW_IMAGE_SECTION,			"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_FULL_SCREEN,				"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_CURRENT_TAB,				"0",        0,                 NULL },
	{ MUIOPTION_SHOW_TOOLBAR,				"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_SHOW_STATUS_BAR,			"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_HIDE_FOLDERS,				"",         0,                 NULL },
#ifdef MESS
	{ MUIOPTION_SHOW_FOLDER_SECTION,		"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_SHOW_TABS,					"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_HIDE_TABS,					"flyer, cabinet, marquee, title, cpanel, pcb", 0, NULL },
	{ MUIOPTION_HISTORY_TAB,				"1",        0,                 NULL },
#else
	{ MUIOPTION_SHOW_FOLDER_SECTION,		"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_SHOW_TABS,					"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_HIDE_TABS,					"marquee, title, cpanel, pcb, history", 0, NULL },
	{ MUIOPTION_HISTORY_TAB,				"0",        0,                 NULL },
#endif

	{ MUIOPTION_SORT_COLUMN,				"0",        0,                 NULL },
	{ MUIOPTION_SORT_REVERSED,				"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_WINDOW_X,					"0",        0,                 NULL },
	{ MUIOPTION_WINDOW_Y,					"0",        0,                 NULL },
	{ MUIOPTION_WINDOW_WIDTH,				"640",      0,                 NULL },
	{ MUIOPTION_WINDOW_HEIGHT,				"400",      0,                 NULL },
	{ MUIOPTION_WINDOW_STATE,				"1",        0,                 NULL },

	{ MUIOPTION_TEXT_COLOR,					"-1",       0,                 NULL },
	{ MUIOPTION_CLONE_COLOR,				"-1",       0,                 NULL },
	{ MUIOPTION_CUSTOM_COLOR,				"0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0", 0, NULL },
	/* ListMode needs to be before ColumnWidths settings */
	{ MUIOPTION_LIST_MODE,					"5",        0,                 NULL },
	{ MUIOPTION_SPLITTERS,					MUIDEFAULT_SPLITTERS, 0,       NULL },
	{ MUIOPTION_LIST_FONT,					"-8,0,0,0,400,0,0,0,0,0,0,0,0", 0, NULL },
	{ MUIOPTION_LIST_FONTFACE,		         	"MS Sans Serif", 0, NULL },
	{ MUIOPTION_COLUMN_WIDTHS,		        	"185,78,84,84,64,88,74,108,60,144,84,60,60", 0, NULL },
	{ MUIOPTION_COLUMN_ORDER,				"0,1,3,4,5,6,7,8,9,10,11,12,2", 0, NULL },
	{ MUIOPTION_COLUMN_SHOWN,				"1,1,0,1,1,1,1,1,1,1,1,1,1", 0,  NULL },

	{ NULL,									NULL,       OPTION_HEADER,     "INTERFACE OPTIONS" },
#if 0 //mamep
	{ MUIOPTION_LANGUAGE,					"english",  0,                 NULL },
#endif
	{ MUIOPTION_CHECK_GAME,					"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_JOYSTICK_IN_INTERFACE,		"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_KEYBOARD_IN_INTERFACE,		"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_RANDOM_BACKGROUND,			"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_BROADCAST_GAME_NAME,		"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_HIDE_MOUSE,					"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_INHERIT_FILTER,				"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_OFFSET_CLONES,				"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_STRETCH_SCREENSHOT_LARGER,	"0",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_CYCLE_SCREENSHOT,			"0",        0,                 NULL },
 	{ MUIOPTION_SCREENSHOT_BORDER_SIZE,		"11",       0,                 NULL },
 	{ MUIOPTION_SCREENSHOT_BORDER_COLOR,	"-1",       0,                 NULL },
	{ MUIOPTION_EXEC_COMMAND,				"",         0,                 NULL },
	{ MUIOPTION_EXEC_WAIT,					"0",        0,                 NULL },
#ifdef USE_SHOW_SPLASH_SCREEN
	{ MUIOPTION_DISPLAY_SPLASH_SCREEN,		"0",        OPTION_BOOLEAN,    NULL },
#endif /* USE_SHOW_SPLASH_SCREEN */
#ifdef TREE_SHEET
	{ MUIOPTION_SHOW_TREE_SHEET,			"1",        OPTION_BOOLEAN,    NULL },
#endif /* TREE_SHEET */
	{ MUIOPTION_BROKEN_COLOR,			"202",      0,                 NULL },
	{ MUIOPTION_USE_BROKEN_ICON,			"1",        OPTION_BOOLEAN,    NULL },
	{ MUIOPTION_FOLDER_FLAG,			NULL,       0,                 NULL },

	{ NULL,									NULL,       OPTION_HEADER,     "SEARCH PATH OPTIONS" },
	{ MUIOPTION_FLYER_DIRECTORY,			"flyers",   0,                 NULL },
	{ MUIOPTION_CABINET_DIRECTORY,			"cabinets", 0,                 NULL },
	{ MUIOPTION_MARQUEE_DIRECTORY,			"marquees", 0,                 NULL },
	{ MUIOPTION_TITLE_DIRECTORY,			"titles",   0,                 NULL },
	{ MUIOPTION_CPANEL_DIRECTORY,			"cpanel",   0,                 NULL },
	{ MUIOPTION_PCB_DIRECTORY,		        "pcb",      0,                 NULL },
	{ MUIOPTION_BACKGROUND_DIRECTORY,		"bkground", 0,                 NULL },
	{ MUIOPTION_FOLDER_DIRECTORY,			"folders",  0,                 NULL },
	{ MUIOPTION_ICONS_DIRECTORY,			"icons",    0,                 NULL },
#ifdef USE_VIEW_PCBINFO
	{ MUIOPTION_PCBINFO_DIRECTORY,		        "pcbinfo",  0,                 NULL },
#endif /* USE_VIEW_PCBINFO */


	{ NULL,									NULL,       OPTION_HEADER,     "FILENAME OPTIONS" },
	{ MUIOPTION_HISTORY_FILE,				MUIHISTORY_FILE, 0,              NULL },
	{ MUIOPTION_MAMEINFO_FILE,				MUIMAMEINFO_FILE, 0,             NULL },
#ifdef STORY_DATAFILE
	{ MUIOPTION_STORY_FILE, 				MUISTORY_FILE, 0,                NULL },
#endif /* STORY_DATAFILE */

	{ NULL,									NULL,       OPTION_HEADER,     "NAVIGATION KEY CODES" },
	{ MUIOPTION_UI_KEY_UP,					"KEYCODE_UP", 0,               NULL },
	{ MUIOPTION_UI_KEY_DOWN,				"KEYCODE_DOWN", 0,             NULL },
	{ MUIOPTION_UI_KEY_LEFT,				"KEYCODE_LEFT", 0,             NULL },
	{ MUIOPTION_UI_KEY_RIGHT,				"KEYCODE_RIGHT", 0,            NULL },
	{ MUIOPTION_UI_KEY_START,				"KEYCODE_ENTER NOT KEYCODE_LALT", 0, NULL },
	{ MUIOPTION_UI_KEY_PGUP,				"KEYCODE_PGUP", 0,             NULL },
	{ MUIOPTION_UI_KEY_PGDWN,				"KEYCODE_PGDN", 0,             NULL },
	{ MUIOPTION_UI_KEY_HOME,				"KEYCODE_HOME", 0,             NULL },
	{ MUIOPTION_UI_KEY_END,					"KEYCODE_END", 0,              NULL },
	{ MUIOPTION_UI_KEY_SS_CHANGE,			"KEYCODE_INSERT", 0,           NULL },
	{ MUIOPTION_UI_KEY_HISTORY_UP,			"KEYCODE_DEL", 0,              NULL },
	{ MUIOPTION_UI_KEY_HISTORY_DOWN,		"KEYCODE_LALT KEYCODE_0", 0,   NULL },

	{ MUIOPTION_UI_KEY_CONTEXT_FILTERS,		"KEYCODE_LCONTROL KEYCODE_F", 0, NULL },
	{ MUIOPTION_UI_KEY_SELECT_RANDOM,		"KEYCODE_LCONTROL KEYCODE_R", 0, NULL },
	{ MUIOPTION_UI_KEY_GAME_AUDIT,			"KEYCODE_LALT KEYCODE_A",     0, NULL },
	{ MUIOPTION_UI_KEY_GAME_PROPERTIES,		"KEYCODE_LALT KEYCODE_ENTER", 0, NULL },
	{ MUIOPTION_UI_KEY_HELP_CONTENTS,		"KEYCODE_F1",                 0, NULL },
	{ MUIOPTION_UI_KEY_UPDATE_GAMELIST,		"KEYCODE_F5",                 0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_FOLDERS,		"KEYCODE_LALT KEYCODE_D",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_FULLSCREEN,		"KEYCODE_F11",                0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_PAGETAB,		"KEYCODE_LALT KEYCODE_B",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_PICTURE_AREA,	"KEYCODE_LALT KEYCODE_P",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_STATUS,			"KEYCODE_LALT KEYCODE_S",     0, NULL },
        { MUIOPTION_UI_KEY_VIEW_TOOLBARS,		"KEYCODE_LALT KEYCODE_T",     0, NULL },

	{ MUIOPTION_UI_KEY_VIEW_TAB_CABINET,	"KEYCODE_LALT KEYCODE_3",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_CPANEL,		"KEYCODE_LALT KEYCODE_6",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_FLYER,		"KEYCODE_LALT KEYCODE_2",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_HISTORY,	"KEYCODE_LALT KEYCODE_8",     0, NULL },
#ifdef STORY_DATAFILE
	{ MUIOPTION_UI_KEY_VIEW_TAB_STORY,	"KEYCODE_LALT KEYCODE_9",     0, NULL },
#endif /* STORY_DATAFILE */
	{ MUIOPTION_UI_KEY_VIEW_TAB_MARQUEE,	"KEYCODE_LALT KEYCODE_4",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_SCREENSHOT,	"KEYCODE_LALT KEYCODE_1",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_TITLE,		"KEYCODE_LALT KEYCODE_5",     0, NULL },
	{ MUIOPTION_UI_KEY_VIEW_TAB_PCB,		"KEYCODE_LALT KEYCODE_7",     0, NULL },
	{ MUIOPTION_UI_KEY_QUIT,				"KEYCODE_LALT KEYCODE_Q",     0, NULL },

	{ NULL,									NULL,       OPTION_HEADER,     "NAVIGATION JOYSTICK CODES" },
	{ MUIOPTION_UI_JOY_UP,					"1,1,1,1",         0,                 NULL },
	{ MUIOPTION_UI_JOY_DOWN,				"1,1,1,2",         0,                 NULL },
	{ MUIOPTION_UI_JOY_LEFT,				"1,1,2,1",  0,                 NULL },
	{ MUIOPTION_UI_JOY_RIGHT,				"1,1,2,2",  0,                 NULL },
	{ MUIOPTION_UI_JOY_START,				"1,0,1,0",         0,                 NULL },
	{ MUIOPTION_UI_JOY_PGUP,				"2,1,2,1",  0,                 NULL },
	{ MUIOPTION_UI_JOY_PGDWN,				"2,1,2,2",  0,                 NULL },
	{ MUIOPTION_UI_JOY_HOME,				"0,0,0,0",         0,                 NULL },
	{ MUIOPTION_UI_JOY_END,					"0,0,0,0",         0,                 NULL },
	{ MUIOPTION_UI_JOY_SS_CHANGE,			"2,0,3,0",  0,                 NULL },
	{ MUIOPTION_UI_JOY_HISTORY_UP,			"2,0,4,0",  0,                 NULL },
	{ MUIOPTION_UI_JOY_HISTORY_DOWN,		"2,0,1,0",  0,                 NULL },
	{ MUIOPTION_UI_JOY_EXEC,				"0,0,0,0",         0,                 NULL },

#ifndef MESS
	{ NULL,									NULL,       OPTION_HEADER,     "GAME STATISTICS" },
#endif
	{ NULL }
};

#define MESS_MARK_CONSOLE_ONLY	"Console"

static const options_entry perGameOptions[] =
{
	// per game options
	{ "_play_count",             "0",        0,                 NULL },
	{ "_play_time",              "0",        0,                 NULL },
	{ "_rom_audit",              "-1",       0,                 NULL },
	{ "_samples_audit",          "-1",       0,                 NULL },
#if 1
	{ "_extra_software",         "",         0,                 MESS_MARK_CONSOLE_ONLY },
#endif
	{ NULL }
};

static const options_entry filterOptions[] =
{
	// filters
	{ "_filters",                "0",        0,                 NULL },
	{ NULL }
};



// Screen shot Page tab control text
// these must match the order of the options flags in options.h
// (TAB_...)
static const WCHAR* image_tabs_long_name[MAX_TAB_TYPES] =
{
	L"Snapshot",
	L"Flyer",
	L"Cabinet",
	L"Marquee",
	L"Title",
	L"Control Panel",
	L"PCB",
	L"History",
#ifdef STORY_DATAFILE
	L"Story",
#endif /* STORY_DATAFILE */
};

static const char * image_tabs_short_name[MAX_TAB_TYPES] =
{
	"snapshot",
	"flyer",
	"cabinet",
	"marquee",
	"title",
	"cpanel",
	"pcb",
	"history"
#ifdef STORY_DATAFILE
	,"story"
#endif /* STORY_DATAFILE */
};


static BOOL save_gui_settings = TRUE;
static BOOL save_default_options = TRUE;

#if 0
folder_filter_type *folder_filters;
int size_folder_filters;
int num_folder_filters;
#endif

/***************************************************************************
    External functions
 ***************************************************************************/

static void memory_error(const char *message)
{
	win_message_box_utf8(NULL, message, NULL, MB_OK);
	exit(-1);
}



void AddOptions(core_options *opts, const options_entry *entrylist, BOOL is_global)
{
	static const char *blacklist[] =
	{
		WINOPTION_SCREEN,
		WINOPTION_ASPECT,
		WINOPTION_RESOLUTION,
		WINOPTION_VIEW
	};
	options_entry entries[2];
	BOOL good_option;
	int i;

	for ( ; entrylist->name != NULL || (entrylist->flags & OPTION_HEADER); entrylist++)
	{
		good_option = TRUE;

		// check blacklist
		//mamep: blacklist is disalbed
		if (0 && entrylist->name)
		{
			for (i = 0; i < ARRAY_LENGTH(blacklist); i++)
			{
				int len = strlen(blacklist[i]);

				if (strncmp(entrylist->name, blacklist[i], len) == 0
				 && (entrylist->name[len] == '\0' || entrylist->name[len] == ';'))
				{
					good_option = FALSE;
					break;
				}
			}
		}
#ifndef MESS
		// if is_global is FALSE, blacklist global options
		if (entrylist->name && !is_global && IsGlobalOption(entrylist->name))
			good_option = FALSE;
#endif
		if (good_option)
		{
			memcpy(entries, entrylist, sizeof(options_entry));
			memset(&entries[1], 0, sizeof(entries[1]));
			options_add_entries(opts, entries);
		}
	}
}



#ifdef MAMEMESS //mamep: moved from ../mess/osd/winui/optionsms.c
static void MessSetupGameOptions(core_options *opts, int driver_index)
{
	BOOL is_global = (driver_index == OPTIONS_TYPE_GLOBAL);
	AddOptions(opts, mess_core_options, is_global);

	if (driver_index >= 0)
	{
		image_add_device_options(opts, drivers[driver_index]);
	}
}
#endif // MAMEMESS

core_options *CreateGameOptions(int driver_index)
{
	core_options *opts;
	BOOL is_global = (driver_index == OPTIONS_TYPE_GLOBAL);
	extern const options_entry mame_win_options[];

	// create the options
	opts = options_create(memory_error);

	// add the options
	AddOptions(opts, mame_core_options, is_global);
	AddOptions(opts, mame_win_options, is_global);

	// customize certain options
	if (is_global)
		options_set_option_default_value(opts, OPTION_INIPATH, "ini");

#ifdef MAMEMESS
	MessSetupGameOptions(opts, driver_index);
#endif // MAMEMESS

	return opts;
}



BOOL OptionsInit()
{
	// create a memory pool for our data
	options_memory_pool = pool_alloc_lib(memory_error);
	if (!options_memory_pool)
		return FALSE;

	// set up the MAME32 settings (these get placed in MAME32ui.ini
	settings = options_create(memory_error);
	options_add_entries(settings, regSettings);
#ifdef MESS
	MessSetupSettings(settings);
#endif

	// set up per game options
	{
		char buffer[128];
		int i, j;
		int n = 0;
		int game_option_count = 0;
		options_entry *driver_per_game_options;
		options_entry *ent;

		while(perGameOptions[game_option_count].name)
			game_option_count++;

#if 1
		driver_per_game_options = (options_entry *) pool_malloc_lib(options_memory_pool,
			(game_option_count * driver_list_get_count(drivers) + 1) * (sizeof(*driver_per_game_options) + 1));
#else
		driver_per_game_options = (options_entry *) pool_malloc_lib(options_memory_pool,
			(game_option_count * driver_list_get_count(drivers) + 1) * sizeof(*driver_per_game_options));
#endif

		for (i = 0; i < driver_list_get_count(drivers); i++)
		{
			for (j = 0; j < game_option_count; j++)
			{
#if 1
				if (j == 0)
				{
					ent = &driver_per_game_options[n++];
					memset(ent, 0, sizeof(*ent));
					ent->flags = OPTION_HEADER;
					ent->description = drivers[i]->description;
				}

				if (perGameOptions[j].description)
				{
					if (strcmp(perGameOptions[j].description, MESS_MARK_CONSOLE_ONLY) == 0)
						continue;
				}

				ent = &driver_per_game_options[n++];
#else
				ent = &driver_per_game_options[i * game_option_count + j];
#endif

				snprintf(buffer, ARRAY_LENGTH(buffer), "%s%s", drivers[i]->name, perGameOptions[j].name);

				memset(ent, 0, sizeof(*ent));
				ent->name = pool_strdup_lib(options_memory_pool, buffer);
				ent->defvalue = perGameOptions[j].defvalue;
				ent->flags = perGameOptions[j].flags;
				ent->description = perGameOptions[j].description;
			}
		}
#if 1
		ent = &driver_per_game_options[n++];
#else
		ent = &driver_per_game_options[driver_list_get_count(drivers) * game_option_count];
#endif
		memset(ent, 0, sizeof(*ent));
		options_add_entries(settings, driver_per_game_options);
	}

	// set up global options
	global = CreateGameOptions(OPTIONS_TYPE_GLOBAL);
	lang_set_langcode(MameUIGlobal(), UI_LANG_EN_US);
#if 0
	// set up folders
	size_folder_filters = 1;
	num_folder_filters = 0;
	folder_filters = (folder_filter_type *) pool_malloc_lib(options_memory_pool, size_folder_filters * sizeof(*folder_filters));
#endif
	// now load the options and settings
	LoadOptionsAndSettings();

	return TRUE;

}

void OptionsExit(void)
{
	// free global options
	options_free(global);
	global = NULL;

	// free settings
	options_free(settings);
	settings = NULL;

	// free the memory pool
	pool_free_lib(options_memory_pool);
	options_memory_pool = NULL;
}

core_options * MameUISettings(void)
{
	return settings;
}

core_options * MameUIGlobal(void)
{
	return global;
}

#if 0
static void LoadFolderFilter(int folder_index,int filters)
{
	//dprintf("loaded folder filter %i %i\n",folder_index,filters);

	if (num_folder_filters == size_folder_filters)
	{
		size_folder_filters *= 2;
		folder_filters = (folder_filter_type *)realloc(
			folder_filters,size_folder_filters * sizeof(folder_filter_type));
	}
	folder_filters[num_folder_filters].folder_index = folder_index;
	folder_filters[num_folder_filters].filters = filters;

	num_folder_filters++;
}
#endif

// Restore ui settings to factory
void ResetGUI(void)
{
	ResetToDefaults(settings, OPTION_PRIORITY_MAXIMUM);
	// Save the new MAME32ui.ini
	SaveOptions();
	save_gui_settings = FALSE;
	SetLangcode(GetLangcode());
	SetUseLangList(UseLangList());
}

const WCHAR * GetImageTabLongName(int tab_index)
{
	assert(tab_index >= 0);
	assert(tab_index < ARRAY_LENGTH(image_tabs_long_name));
	return image_tabs_long_name[tab_index];
}

const char * GetImageTabShortName(int tab_index)
{
	assert(tab_index >= 0);
	assert(tab_index < ARRAY_LENGTH(image_tabs_short_name));
	return image_tabs_short_name[tab_index];
}

//============================================================
//  OPTIONS WRAPPERS
//============================================================

static COLORREF options_get_color(core_options *opts, const char *name)
{
	const char *value_str;
	unsigned int r, g, b;
	COLORREF value;

	value_str = options_get_string(opts, name);

	if (sscanf(value_str, "%u,%u,%u", &r, &g, &b) == 3)
		value = RGB(r,g,b);
	else
		value = (COLORREF) - 1;
	return value;
}

static void options_set_color(core_options *opts, const char *name, COLORREF value)
{
	char value_str[32];

	if (value == (COLORREF) -1)
	{
		snprintf(value_str, ARRAY_LENGTH(value_str), "%d", (int) value);
	}
	else
	{
		snprintf(value_str, ARRAY_LENGTH(value_str), "%d,%d,%d",
			(((int) value) >>  0) & 0xFF,
			(((int) value) >>  8) & 0xFF,
			(((int) value) >> 16) & 0xFF);
	}
	options_set_string(opts, name, value_str, OPTION_PRIORITY_CMDLINE);
}

static COLORREF options_get_color_default(core_options *opts, const char *name, int default_color)
{
	COLORREF value = options_get_color(opts, name);
	if (value == (COLORREF) -1)
		value = GetSysColor(default_color);
	return value;
}

static void options_set_color_default(core_options *opts, const char *name, COLORREF value, int default_color)
{
	if (value == GetSysColor(default_color))
		options_set_color(settings, name, (COLORREF) -1);
	else
		options_set_color(settings, name, value);
}

static input_seq *options_get_input_seq(core_options *opts, const char *name)
{
	static input_seq seq;
	const char *seq_string;

	seq_string = options_get_string(opts, name);
	input_seq_from_tokens(NULL, seq_string, &seq);   // HACK
	return &seq;
}



//============================================================
//  OPTIONS CALLS
//============================================================

void SetViewMode(int val)
{
	options_set_int(settings, MUIOPTION_LIST_MODE, val, OPTION_PRIORITY_CMDLINE);
}

int GetViewMode(void)
{
	return options_get_int(settings, MUIOPTION_LIST_MODE);
}

void SetGameCheck(BOOL game_check)
{
	options_set_bool(settings, MUIOPTION_CHECK_GAME, game_check, OPTION_PRIORITY_CMDLINE);
}

BOOL GetGameCheck(void)
{
	return options_get_bool(settings,MUIOPTION_CHECK_GAME);
}

void SetJoyGUI(BOOL use_joygui)
{
	options_set_bool(settings, MUIOPTION_JOYSTICK_IN_INTERFACE, use_joygui, OPTION_PRIORITY_CMDLINE);
}

BOOL GetJoyGUI(void)
{
	return options_get_bool(settings, MUIOPTION_JOYSTICK_IN_INTERFACE);
}

void SetKeyGUI(BOOL use_keygui)
{
	options_set_bool(settings, MUIOPTION_KEYBOARD_IN_INTERFACE, use_keygui, OPTION_PRIORITY_CMDLINE);
}

BOOL GetKeyGUI(void)
{
	return options_get_bool(settings, MUIOPTION_KEYBOARD_IN_INTERFACE);
}

void SetCycleScreenshot(int cycle_screenshot)
{
	options_set_int(settings, MUIOPTION_CYCLE_SCREENSHOT, cycle_screenshot, OPTION_PRIORITY_CMDLINE);
}

int GetCycleScreenshot(void)
{
	return options_get_int(settings, MUIOPTION_CYCLE_SCREENSHOT);
}

void SetStretchScreenShotLarger(BOOL stretch)
{
	options_set_bool(settings, MUIOPTION_STRETCH_SCREENSHOT_LARGER, stretch, OPTION_PRIORITY_CMDLINE);
}

BOOL GetStretchScreenShotLarger(void)
{
	return options_get_bool(settings, MUIOPTION_STRETCH_SCREENSHOT_LARGER);
}

void SetScreenshotBorderSize(int size)
{
	options_set_int(settings, MUIOPTION_SCREENSHOT_BORDER_SIZE, size, OPTION_PRIORITY_CMDLINE);
}

int GetScreenshotBorderSize(void)
{
	return options_get_int(settings, MUIOPTION_SCREENSHOT_BORDER_SIZE);
}

void SetScreenshotBorderColor(COLORREF uColor)
{
	options_set_color_default(settings, MUIOPTION_SCREENSHOT_BORDER_COLOR, uColor, COLOR_3DFACE);
}

COLORREF GetScreenshotBorderColor(void)
{
	return options_get_color_default(settings, MUIOPTION_SCREENSHOT_BORDER_COLOR, COLOR_3DFACE);
}

void SetFilterInherit(BOOL inherit)
{
	options_set_bool(settings, MUIOPTION_INHERIT_FILTER, inherit, OPTION_PRIORITY_CMDLINE);
}

BOOL GetFilterInherit(void)
{
	return options_get_bool(settings, MUIOPTION_INHERIT_FILTER);
}

void SetOffsetClones(BOOL offset)
{
	options_set_bool(settings, MUIOPTION_OFFSET_CLONES, offset, OPTION_PRIORITY_CMDLINE);
}

BOOL GetOffsetClones(void)
{
	return options_get_bool(settings, MUIOPTION_OFFSET_CLONES);
}

void SetBroadcast(BOOL broadcast)
{
	options_set_bool(settings, MUIOPTION_BROADCAST_GAME_NAME, broadcast, OPTION_PRIORITY_CMDLINE);
}

BOOL GetBroadcast(void)
{
	return options_get_bool(settings, MUIOPTION_BROADCAST_GAME_NAME);
}

void SetRandomBackground(BOOL random_bg)
{
	options_set_bool(settings, MUIOPTION_RANDOM_BACKGROUND, random_bg, OPTION_PRIORITY_CMDLINE);
}

BOOL GetRandomBackground(void)
{
	return options_get_bool(settings, MUIOPTION_RANDOM_BACKGROUND);
}

void SetSavedFolderID(UINT val)
{
	options_set_int(settings, MUIOPTION_DEFAULT_FOLDER_ID, (int) val, OPTION_PRIORITY_CMDLINE);
}

UINT GetSavedFolderID(void)
{
	return (UINT) options_get_int(settings, MUIOPTION_DEFAULT_FOLDER_ID);
}

void SetShowScreenShot(BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_IMAGE_SECTION, val, OPTION_PRIORITY_CMDLINE);
}

BOOL GetShowScreenShot(void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_IMAGE_SECTION);
}

void SetShowFolderList(BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_FOLDER_SECTION, val, OPTION_PRIORITY_CMDLINE);
}

BOOL GetShowFolderList(void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_FOLDER_SECTION);
}

static void GetsShowFolderFlags(LPBITS bits)
{
	char s[2000];
	extern const FOLDERDATA g_folderData[];
	char *token;

	snprintf(s, ARRAY_LENGTH(s), "%s", options_get_string(settings, MUIOPTION_HIDE_FOLDERS));

	SetAllBits(bits, TRUE);

	token = strtok(s,", \t");
	while (token != NULL)
	{
		int j;

		for (j=0;g_folderData[j].m_lpTitle != NULL;j++)
		{
			if (strcmp(g_folderData[j].short_name,token) == 0)
			{
				ClearBit(bits, g_folderData[j].m_nFolderId);
				break;
			}
		}
		token = strtok(NULL,", \t");
	}
}

BOOL GetShowFolder(int folder)
{
	BOOL result;
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	GetsShowFolderFlags(show_folder_flags);
	result = TestBit(show_folder_flags, folder);
	DeleteBits(show_folder_flags);
	return result;
}

void SetShowFolder(int folder,BOOL show)
{
	LPBITS show_folder_flags = NewBits(MAX_FOLDERS);
	int i;
	int num_saved = 0;
	char str[10000];
	extern const FOLDERDATA g_folderData[];

	GetsShowFolderFlags(show_folder_flags);

	if (show)
		SetBit(show_folder_flags, folder);
	else
		ClearBit(show_folder_flags, folder);

	strcpy(str, "");

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for (i=0;i<MAX_FOLDERS;i++)
	{
		if (TestBit(show_folder_flags, i) == FALSE)
		{
			int j;

			if (num_saved != 0)
				strcat(str,", ");

			for (j=0;g_folderData[j].m_lpTitle != NULL;j++)
			{
				if (g_folderData[j].m_nFolderId == i)
				{
					strcat(str,g_folderData[j].short_name);
					num_saved++;
					break;
				}
			}
		}
	}
	options_set_string(settings, MUIOPTION_HIDE_FOLDERS, str, OPTION_PRIORITY_CMDLINE);
	DeleteBits(show_folder_flags);
}

void SetShowStatusBar(BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_STATUS_BAR, val, OPTION_PRIORITY_CMDLINE);
}

BOOL GetShowStatusBar(void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_STATUS_BAR);
}

void SetShowTabCtrl (BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_TABS, val, OPTION_PRIORITY_CMDLINE);
}

BOOL GetShowTabCtrl (void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_TABS);
}

void SetShowToolBar(BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_TOOLBAR, val, OPTION_PRIORITY_CMDLINE);
}

BOOL GetShowToolBar(void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_TOOLBAR);
}

void SetCurrentTab(const char *shortname)
{
	options_set_string(settings, MUIOPTION_CURRENT_TAB, shortname, OPTION_PRIORITY_CMDLINE);
}

const char *GetCurrentTab(void)
{
	return options_get_string(settings, MUIOPTION_CURRENT_TAB);
}

void SetDefaultGame(const char *name)
{
	options_set_string(settings, MUIOPTION_DEFAULT_GAME, name, OPTION_PRIORITY_CMDLINE);
}

const char *GetDefaultGame(void)
{
	return options_get_string(settings, MUIOPTION_DEFAULT_GAME);
}

void SetWindowArea(const AREA *area)
{
	options_set_int(settings, MUIOPTION_WINDOW_X,		area->x, OPTION_PRIORITY_CMDLINE);
	options_set_int(settings, MUIOPTION_WINDOW_Y,		area->y, OPTION_PRIORITY_CMDLINE);
	options_set_int(settings, MUIOPTION_WINDOW_WIDTH,	area->width, OPTION_PRIORITY_CMDLINE);
	options_set_int(settings, MUIOPTION_WINDOW_HEIGHT,	area->height, OPTION_PRIORITY_CMDLINE);
}

void GetWindowArea(AREA *area)
{
	area->x      = options_get_int(settings, MUIOPTION_WINDOW_X);
	area->y      = options_get_int(settings, MUIOPTION_WINDOW_Y);
	area->width  = options_get_int(settings, MUIOPTION_WINDOW_WIDTH);
	area->height = options_get_int(settings, MUIOPTION_WINDOW_HEIGHT);
}

void SetWindowState(UINT state)
{
	options_set_int(settings, MUIOPTION_WINDOW_STATE, state, OPTION_PRIORITY_CMDLINE);
}

UINT GetWindowState(void)
{
	return options_get_int(settings, MUIOPTION_WINDOW_STATE);
}

void SetCustomColor(int iIndex, COLORREF uColor)
{
	const char *custom_color_string;
	COLORREF custom_color[256];
	char buffer[10000];

	custom_color_string = options_get_string(settings, MUIOPTION_CUSTOM_COLOR);
	CusColorDecodeString(custom_color_string, custom_color);

	custom_color[iIndex] = uColor;

	CusColorEncodeString(custom_color, buffer);
	options_set_string(settings, MUIOPTION_CUSTOM_COLOR, buffer, OPTION_PRIORITY_CMDLINE);
}

COLORREF GetCustomColor(int iIndex)
{
	const char *custom_color_string;
	COLORREF custom_color[256];

	custom_color_string = options_get_string(settings, MUIOPTION_CUSTOM_COLOR);
	CusColorDecodeString(custom_color_string, custom_color);

	if (custom_color[iIndex] == (COLORREF)-1)
		return (COLORREF)RGB(0,0,0);

	return custom_color[iIndex];
}

void SetListFont(const LOGFONTW *font)
{
	char font_string[10000];
	FontEncodeString(font, font_string);
	options_set_string(settings, MUIOPTION_LIST_FONT, font_string, OPTION_PRIORITY_CMDLINE);
	options_set_wstring(settings, MUIOPTION_LIST_FONTFACE, font->lfFaceName, OPTION_PRIORITY_CMDLINE);
}

void GetListFont(LOGFONTW *font)
{
	const char *font_string = options_get_string(settings, MUIOPTION_LIST_FONT);
	const WCHAR *stemp = options_get_wstring(settings, MUIOPTION_LIST_FONTFACE);

	FontDecodeString(font_string, font);

	if (stemp)
		snwprintf(font->lfFaceName, ARRAY_LENGTH(font->lfFaceName), L"%s", stemp);
}

void SetListFontColor(COLORREF uColor)
{
	options_set_color_default(settings, MUIOPTION_TEXT_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetListFontColor(void)
{
	return options_get_color_default(settings, MUIOPTION_TEXT_COLOR, COLOR_WINDOWTEXT);
}

void SetListCloneColor(COLORREF uColor)
{
	options_set_color_default(settings, MUIOPTION_CLONE_COLOR, uColor, COLOR_WINDOWTEXT);
}

COLORREF GetListCloneColor(void)
{
	return options_get_color_default(settings, MUIOPTION_CLONE_COLOR, COLOR_WINDOWTEXT);
}

int GetShowTab(int tab)
{
	const char *show_tabs_string;
	int show_tab_flags;

	show_tabs_string = options_get_string(settings, MUIOPTION_HIDE_TABS);
	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);
	return (show_tab_flags & (1 << tab)) != 0;
}

void SetShowTab(int tab,BOOL show)
{
	const char *show_tabs_string;
	int show_tab_flags;
	char buffer[10000];

	show_tabs_string = options_get_string(settings, MUIOPTION_HIDE_TABS);
	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);

	if (show)
		show_tab_flags |= 1 << tab;
	else
		show_tab_flags &= ~(1 << tab);

	TabFlagsEncodeString(show_tab_flags, buffer);
	options_set_string(settings, MUIOPTION_HIDE_TABS, buffer, OPTION_PRIORITY_CMDLINE);
}

// don't delete the last one
BOOL AllowedToSetShowTab(int tab,BOOL show)
{
	const char *show_tabs_string;
	int show_tab_flags;

	if (show == TRUE)
		return TRUE;

	show_tabs_string = options_get_string(settings, MUIOPTION_HIDE_TABS);
	TabFlagsDecodeString(show_tabs_string, &show_tab_flags);

	show_tab_flags &= ~(1 << tab);
	return show_tab_flags != 0;
}

int GetHistoryTab(void)
{
	return options_get_int(settings, MUIOPTION_HISTORY_TAB);
}

void SetHistoryTab(int tab, BOOL show)
{
	if (show)
		options_set_int(settings, MUIOPTION_HISTORY_TAB, tab, OPTION_PRIORITY_CMDLINE);
	else
		options_set_int(settings, MUIOPTION_HISTORY_TAB, TAB_NONE, OPTION_PRIORITY_CMDLINE);
}

void SetColumnWidths(int width[])
{
	char column_width_string[10000];
	ColumnEncodeStringWithCount(width, column_width_string, COLUMN_MAX);
	options_set_string(settings, MUIOPTION_COLUMN_WIDTHS, column_width_string, OPTION_PRIORITY_CMDLINE);
}

void GetColumnWidths(int width[])
{
	const char *column_width_string;
	column_width_string = options_get_string(settings, MUIOPTION_COLUMN_WIDTHS);
	ColumnDecodeStringWithCount(column_width_string, width, COLUMN_MAX);
}

void SetSplitterPos(int splitterId, int pos)
{
	const char *splitter_string;
	int *splitter;
	char buffer[10000];

	if (splitterId < GetSplitterCount())
	{
		splitter_string = options_get_string(settings, MUIOPTION_SPLITTERS);
		splitter = (int *) alloca(GetSplitterCount() * sizeof(*splitter));
		SplitterDecodeString(splitter_string, splitter);

		splitter[splitterId] = pos;

		SplitterEncodeString(splitter, buffer);
		options_set_string(settings, MUIOPTION_SPLITTERS, buffer, OPTION_PRIORITY_CMDLINE);
	}
}

int  GetSplitterPos(int splitterId)
{
	const char *splitter_string;
	int *splitter;

	splitter_string = options_get_string(settings, MUIOPTION_SPLITTERS);
	splitter = (int *) alloca(GetSplitterCount() * sizeof(*splitter));
	SplitterDecodeString(splitter_string, splitter);

	if (splitterId < GetSplitterCount())
		return splitter[splitterId];

	return -1; /* Error */
}

void SetColumnOrder(int order[])
{
	char column_order_string[10000];
	ColumnEncodeStringWithCount(order, column_order_string, COLUMN_MAX);
	options_set_string(settings, MUIOPTION_COLUMN_ORDER, column_order_string, OPTION_PRIORITY_CMDLINE);
}

void GetColumnOrder(int order[])
{
	const char *column_order_string;
	column_order_string = options_get_string(settings, MUIOPTION_COLUMN_ORDER);
	ColumnDecodeStringWithCount(column_order_string, order, COLUMN_MAX);
}

void SetColumnShown(int shown[])
{
	char column_shown_string[10000];
	ColumnEncodeStringWithCount(shown, column_shown_string, COLUMN_MAX);
	options_set_string(settings, MUIOPTION_COLUMN_SHOWN, column_shown_string, OPTION_PRIORITY_CMDLINE);
}

void GetColumnShown(int shown[])
{
	const char *column_shown_string;
	column_shown_string = options_get_string(settings, MUIOPTION_COLUMN_SHOWN);
	ColumnDecodeStringWithCount(column_shown_string, shown, COLUMN_MAX);
}

void SetSortColumn(int column)
{
	options_set_int(settings, MUIOPTION_SORT_COLUMN, column, OPTION_PRIORITY_CMDLINE);
}

int GetSortColumn(void)
{
	return options_get_int(settings, MUIOPTION_SORT_COLUMN);
}

void SetSortReverse(BOOL reverse)
{
	options_set_bool(settings, MUIOPTION_SORT_REVERSED, reverse, OPTION_PRIORITY_CMDLINE);
}

BOOL GetSortReverse(void)
{
	return options_get_bool(settings, MUIOPTION_SORT_REVERSED);
}

#if 0 //mamep
const char* GetLanguage(void)
{
	return options_get_string(settings, MUIOPTION_LANGUAGE);
}

void SetLanguage(const char* lang)
{
	options_set_string(settings, MUIOPTION_LANGUAGE, lang, OPTION_PRIORITY_CMDLINE);
}
#endif

const WCHAR* GetRomDirs(void)
{
	return options_get_wstring(global, OPTION_ROMPATH);
}

void SetRomDirs(const WCHAR* paths)
{
	options_set_wstring(global, OPTION_ROMPATH, paths, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetSampleDirs(void)
{
	return options_get_wstring(global, OPTION_SAMPLEPATH);
}

void SetSampleDirs(const WCHAR* paths)
{
	options_set_wstring(global, OPTION_SAMPLEPATH, paths, OPTION_PRIORITY_CMDLINE);
}

const WCHAR * GetIniDir(void)
{
	const WCHAR *ini_dir;
	const WCHAR *s;

	ini_dir = options_get_wstring(global, OPTION_INIPATH);
	while((s = wcschr(ini_dir, ';')) != NULL)
	{
		ini_dir = s + 1;
	}
	return ini_dir;

}

void SetIniDir(const WCHAR *path)
{
	options_set_wstring(global, OPTION_INIPATH, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCtrlrDir(void)
{
	return options_get_wstring(global, OPTION_CTRLRPATH);
}

void SetCtrlrDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_CTRLRPATH, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCommentDir(void)
{
	return options_get_wstring(global, OPTION_COMMENT_DIRECTORY);
}

void SetCommentDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_COMMENT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCfgDir(void)
{
	return options_get_wstring(global, OPTION_CFG_DIRECTORY);
}

void SetCfgDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_CFG_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetNvramDir(void)
{
	return options_get_wstring(global, OPTION_NVRAM_DIRECTORY);
}

void SetNvramDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_NVRAM_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetInpDir(void)
{
	return options_get_wstring(global, OPTION_INPUT_DIRECTORY);
}

void SetInpDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_INPUT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetImgDir(void)
{
	return options_get_wstring(global, OPTION_SNAPSHOT_DIRECTORY);
}

void SetImgDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_SNAPSHOT_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetStateDir(void)
{
	return options_get_wstring(global, OPTION_STATE_DIRECTORY);
}

void SetStateDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_STATE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetArtDir(void)
{
	return options_get_wstring(global, OPTION_ARTPATH);
}

void SetArtDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_ARTPATH, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetMemcardDir(void)
{
	return options_get_wstring(global, OPTION_MEMCARD_DIRECTORY);
}

void SetMemcardDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_MEMCARD_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetFontDir(void)
{
	return options_get_wstring(global, OPTION_FONTPATH);
}

void SetFontDir(const WCHAR* paths)
{
	options_set_wstring(global, OPTION_FONTPATH, paths, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCrosshairDir(void)
{
	return options_get_wstring(global, OPTION_CROSSHAIRPATH);
}

void SetCrosshairDir(const WCHAR* paths)
{
	options_set_wstring(global, OPTION_CROSSHAIRPATH, paths, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetFlyerDir(void)
{
	return options_get_wstring(settings, MUIOPTION_FLYER_DIRECTORY);
}

void SetFlyerDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_FLYER_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCabinetDir(void)
{
	return options_get_wstring(settings, MUIOPTION_CABINET_DIRECTORY);
}

void SetCabinetDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_CABINET_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetMarqueeDir(void)
{
	return options_get_wstring(settings, MUIOPTION_MARQUEE_DIRECTORY);
}

void SetMarqueeDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_MARQUEE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetTitlesDir(void)
{
	return options_get_wstring(settings, MUIOPTION_TITLE_DIRECTORY);
}

void SetTitlesDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_TITLE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR * GetControlPanelDir(void)
{
	return options_get_wstring(settings, MUIOPTION_CPANEL_DIRECTORY);
}

void SetControlPanelDir(const WCHAR *path)
{
	options_set_wstring(settings, MUIOPTION_CPANEL_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR * GetPcbDir(void)
{
	return options_get_wstring(settings, MUIOPTION_PCB_DIRECTORY);
}

void SetPcbDir(const WCHAR *path)
{
	options_set_wstring(settings, MUIOPTION_PCB_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR * GetDiffDir(void)
{
	return options_get_wstring(global, OPTION_DIFF_DIRECTORY);
}

void SetDiffDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_DIFF_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetIconsDir(void)
{
	return options_get_wstring(settings, MUIOPTION_ICONS_DIRECTORY);
}

void SetIconsDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_ICONS_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetBgDir (void)
{
	return options_get_wstring(settings, MUIOPTION_BACKGROUND_DIRECTORY);
}

void SetBgDir (const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_BACKGROUND_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetFolderDir(void)
{
	return options_get_wstring(settings, MUIOPTION_FOLDER_DIRECTORY);
}

void SetFolderDir(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_FOLDER_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetCheatDir(void)
{
	return options_get_wstring(global, OPTION_CHEATPATH);
}

void SetCheatDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_CHEATPATH, path, OPTION_PRIORITY_CMDLINE);
}

const WCHAR* GetHistoryFileName(void)
{
	return options_get_wstring(settings, MUIOPTION_HISTORY_FILE);
}

void SetHistoryFileName(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_HISTORY_FILE, path, OPTION_PRIORITY_CMDLINE);
}


const WCHAR* GetMAMEInfoFileName(void)
{
	return options_get_wstring(settings, MUIOPTION_MAMEINFO_FILE);
}

void SetMAMEInfoFileName(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_MAMEINFO_FILE, path, OPTION_PRIORITY_CMDLINE);
}

#ifdef STORY_DATAFILE
const WCHAR* GetStoryFileName(void)
{
	return options_get_wstring(settings, MUIOPTION_STORY_FILE);
}
void SetStoryFileName(const WCHAR* path)
{
	options_set_wstring(settings, MUIOPTION_STORY_FILE, path, OPTION_PRIORITY_CMDLINE);
}
#endif /* STORY_DATAFILE */
#ifdef USE_VIEW_PCBINFO
const WCHAR * GetPcbInfoDir(void)
{
	return options_get_wstring(settings, MUIOPTION_PCBINFO_DIRECTORY);
}
void SetPcbInfoDir(const WCHAR *path)
{
	options_set_wstring(settings, MUIOPTION_PCBINFO_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}
#endif /* USE_VIEW_PCBINFO */

const char* GetSnapName(void)
{
	return options_get_string(global, OPTION_SNAPNAME);
}

void SetSnapName(const char* pattern)
{
	options_set_string(global, OPTION_SNAPNAME, pattern, OPTION_PRIORITY_CMDLINE);
}

void ResetGameOptions(int driver_index)
{
	assert(0 <= driver_index && driver_index < driver_list_get_count(drivers));

	save_options(OPTIONS_GAME, NULL, driver_index);
}

void ResetGameDefaults(void)
{
	// Walk the global settings and reset everything to defaults;
	ResetToDefaults(global, OPTION_PRIORITY_MAXIMUM);
	save_options(OPTIONS_GLOBAL, global, 1);
	save_default_options = FALSE;
}

/*
 * Reset all game, vector and source options to defaults.
 * No reason to reboot if this is done.
 */
void ResetAllGameOptions(void)
{
	int i;

	for (i = 0; i < driver_list_get_count(drivers); i++)
	{
		ResetGameOptions(i);
	}

	/* Walk the ini/source folder deleting all ini's. */
	remove_all_source_options();

	/* finally vector.ini */
	save_options(OPTIONS_VECTOR, NULL, 0);
	/* finally horizont.ini */
	save_options(OPTIONS_HORIZONTAL, NULL, 0);
	/* finally vertical.ini */
	save_options(OPTIONS_VERTICAL, NULL, 0);
}

static void GetDriverOptionName(int driver_index, const char *option_name, char *buffer, size_t buffer_len)
{
	assert(0 <= driver_index && driver_index < driver_list_get_count(drivers));
	snprintf(buffer, buffer_len, "%s_%s", drivers[driver_index]->name, option_name);
}

int GetRomAuditResults(int driver_index)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "rom_audit", buffer, ARRAY_LENGTH(buffer));
	return options_get_int(settings, buffer);
}

void SetRomAuditResults(int driver_index, int audit_results)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "rom_audit", buffer, ARRAY_LENGTH(buffer));
	options_set_int(settings, buffer, audit_results, OPTION_PRIORITY_CMDLINE);
}

int  GetSampleAuditResults(int driver_index)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "samples_audit", buffer, ARRAY_LENGTH(buffer));
	return options_get_int(settings, buffer);
}

void SetSampleAuditResults(int driver_index, int audit_results)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "samples_audit", buffer, ARRAY_LENGTH(buffer));
	options_set_int(settings, buffer, audit_results, OPTION_PRIORITY_CMDLINE);
}

static void IncrementPlayVariable(int driver_index, const char *play_variable, int increment)
{
	char buffer[128];
	int count;

	GetDriverOptionName(driver_index, play_variable, buffer, ARRAY_LENGTH(buffer));
	count = options_get_int(settings, buffer);
	options_set_int(settings, buffer, count + increment, OPTION_PRIORITY_CMDLINE);
}

void IncrementPlayCount(int driver_index)
{
	IncrementPlayVariable(driver_index, "play_count", 1);
}

int GetPlayCount(int driver_index)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "play_count", buffer, ARRAY_LENGTH(buffer));
	return options_get_int(settings, buffer);
}

static void ResetPlayVariable(int driver_index, const char *play_variable)
{
	int i;
	if (driver_index < 0)
	{
		/* all games */
		for (i = 0; i< driver_list_get_count(drivers); i++)
		{
			/* 20070808 MSH - Was passing in driver_index instead of i. Doh! */
			ResetPlayVariable(i, play_variable);
		}
	}
	else
	{
		char buffer[128];
		GetDriverOptionName(driver_index, play_variable, buffer, ARRAY_LENGTH(buffer));
		options_set_int(settings, buffer, 0, OPTION_PRIORITY_CMDLINE);
	}
}

void ResetPlayCount(int driver_index)
{
	ResetPlayVariable(driver_index, "play_count");
}

void ResetPlayTime(int driver_index)
{
	ResetPlayVariable(driver_index, "play_time");
}

int GetPlayTime(int driver_index)
{
	char buffer[128];
	GetDriverOptionName(driver_index, "play_time", buffer, ARRAY_LENGTH(buffer));
	return options_get_int(settings, buffer);
}

void IncrementPlayTime(int driver_index,int playtime)
{
	IncrementPlayVariable(driver_index, "play_time", playtime);
}

void GetTextPlayTime(int driver_index, WCHAR *buf)
{
	int hour, minute, second;
	int temp = GetPlayTime(driver_index);

	assert(0 <= driver_index && driver_index < driver_list_get_count(drivers));

	hour = temp / 3600;
	temp = temp - 3600*hour;
	minute = temp / 60; //Calc Minutes
	second = temp - 60*minute;

	if (hour == 0)
		swprintf(buf, L"%d:%02d", minute, second );
	else
		swprintf(buf, L"%d:%02d:%02d", hour, minute, second );
}

input_seq* Get_ui_key_up(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_UP);
}
input_seq* Get_ui_key_down(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_DOWN);
}
input_seq* Get_ui_key_left(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_LEFT);
}
input_seq* Get_ui_key_right(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_RIGHT);
}
input_seq* Get_ui_key_start(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_START);
}
input_seq* Get_ui_key_pgup(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_PGUP);
}
input_seq* Get_ui_key_pgdwn(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_PGDWN);
}
input_seq* Get_ui_key_home(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_HOME);
}
input_seq* Get_ui_key_end(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_END);
}
input_seq* Get_ui_key_ss_change(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_SS_CHANGE);
}
input_seq* Get_ui_key_history_up(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_HISTORY_UP);
}
input_seq* Get_ui_key_history_down(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_HISTORY_DOWN);
}


input_seq* Get_ui_key_context_filters(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_CONTEXT_FILTERS);
}
input_seq* Get_ui_key_select_random(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_SELECT_RANDOM);
}
input_seq* Get_ui_key_game_audit(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_GAME_AUDIT);
}
input_seq* Get_ui_key_game_properties(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_GAME_PROPERTIES);
}
input_seq* Get_ui_key_help_contents(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_HELP_CONTENTS);
}
input_seq* Get_ui_key_update_gamelist(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_UPDATE_GAMELIST);
}
input_seq* Get_ui_key_view_folders(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_FOLDERS);
}
input_seq* Get_ui_key_view_fullscreen(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_FULLSCREEN);
}
input_seq* Get_ui_key_view_pagetab(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_PAGETAB);
}
input_seq* Get_ui_key_view_picture_area(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_PICTURE_AREA);
}
input_seq* Get_ui_key_view_status(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_STATUS);
}
input_seq* Get_ui_key_view_toolbars(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TOOLBARS);
}

input_seq* Get_ui_key_view_tab_cabinet(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_CABINET);
}
input_seq* Get_ui_key_view_tab_cpanel(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_CPANEL);
}
input_seq* Get_ui_key_view_tab_flyer(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_FLYER);
}
input_seq* Get_ui_key_view_tab_history(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_HISTORY);
}
#ifdef STORY_DATAFILE
input_seq *Get_ui_key_view_tab_story(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_STORY);
}
#endif /* STORY_DATAFILE */
input_seq* Get_ui_key_view_tab_marquee(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_MARQUEE);
}
input_seq* Get_ui_key_view_tab_screenshot(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_SCREENSHOT);
}
input_seq* Get_ui_key_view_tab_title(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_TITLE);
}
input_seq* Get_ui_key_view_tab_pcb(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_VIEW_TAB_PCB);
}
input_seq* Get_ui_key_quit(void)
{
	return options_get_input_seq(settings, MUIOPTION_UI_KEY_QUIT);
}



static int GetUIJoy(const char *option_name, int joycodeIndex)
{
	const char *joycodes_string;
	int joycodes[4];

	assert(0 <= joycodeIndex && joycodeIndex < 4);
	joycodes_string = options_get_string(settings, option_name);
	ColumnDecodeStringWithCount(joycodes_string, joycodes, ARRAY_LENGTH(joycodes));
	return joycodes[joycodeIndex];
}

static void SetUIJoy(const char *option_name, int joycodeIndex, int val)
{
	const char *joycodes_string;
	int joycodes[4];
	char buffer[1024];

	assert(0 <= joycodeIndex && joycodeIndex < 4);
	joycodes_string = options_get_string(settings, option_name);
	ColumnDecodeStringWithCount(joycodes_string, joycodes, ARRAY_LENGTH(joycodes));

	joycodes[joycodeIndex] = val;
	ColumnEncodeStringWithCount(joycodes, buffer, ARRAY_LENGTH(joycodes));
	options_set_string(settings, option_name, buffer, OPTION_PRIORITY_CMDLINE);


}

int GetUIJoyUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_UP, joycodeIndex);
}

void SetUIJoyUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_UP, joycodeIndex, val);
}

int GetUIJoyDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_DOWN, joycodeIndex);
}

void SetUIJoyDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_DOWN, joycodeIndex, val);
}

int GetUIJoyLeft(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_LEFT, joycodeIndex);
}

void SetUIJoyLeft(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_LEFT, joycodeIndex, val);
}

int GetUIJoyRight(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_RIGHT, joycodeIndex);
}

void SetUIJoyRight(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_RIGHT, joycodeIndex, val);
}

int GetUIJoyStart(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_START, joycodeIndex);
}

void SetUIJoyStart(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_START, joycodeIndex, val);
}

int GetUIJoyPageUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_PGUP, joycodeIndex);
}

void SetUIJoyPageUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_PGUP, joycodeIndex, val);
}

int GetUIJoyPageDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_PGDWN, joycodeIndex);
}

void SetUIJoyPageDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_PGDWN, joycodeIndex, val);
}

int GetUIJoyHome(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HOME, joycodeIndex);
}

void SetUIJoyHome(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HOME, joycodeIndex, val);
}

int GetUIJoyEnd(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_END, joycodeIndex);
}

void SetUIJoyEnd(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_END, joycodeIndex, val);
}

int GetUIJoySSChange(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_SS_CHANGE, joycodeIndex);
}

void SetUIJoySSChange(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_SS_CHANGE, joycodeIndex, val);
}

int GetUIJoyHistoryUp(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HISTORY_UP, joycodeIndex);
}

void SetUIJoyHistoryUp(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HISTORY_UP, joycodeIndex, val);
}

int GetUIJoyHistoryDown(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_HISTORY_DOWN, joycodeIndex);
}

void SetUIJoyHistoryDown(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_HISTORY_DOWN, joycodeIndex, val);
}

void SetUIJoyExec(int joycodeIndex, int val)
{
	SetUIJoy(MUIOPTION_UI_JOY_EXEC, joycodeIndex, val);
}

int GetUIJoyExec(int joycodeIndex)
{
	return GetUIJoy(MUIOPTION_UI_JOY_EXEC, joycodeIndex);
}

WCHAR * GetExecCommand(void)
{
	return options_get_wstring(settings, MUIOPTION_EXEC_COMMAND);
}

void SetExecCommand(WCHAR *cmd)
{
	options_set_wstring(settings, MUIOPTION_EXEC_COMMAND, cmd, OPTION_PRIORITY_CMDLINE);
}

int GetExecWait(void)
{
	return options_get_int(settings, MUIOPTION_EXEC_WAIT);
}

void SetExecWait(int wait)
{
	options_set_int(settings, MUIOPTION_EXEC_WAIT, wait, OPTION_PRIORITY_CMDLINE);
}

BOOL GetHideMouseOnStartup(void)
{
	return options_get_bool(settings, MUIOPTION_HIDE_MOUSE);
}

void SetHideMouseOnStartup(BOOL hide)
{
	options_set_bool(settings, MUIOPTION_HIDE_MOUSE, hide, OPTION_PRIORITY_CMDLINE);
}

BOOL GetRunFullScreen(void)
{
	return options_get_bool(settings, MUIOPTION_FULL_SCREEN);
}

void SetRunFullScreen(BOOL fullScreen)
{
	options_set_bool(settings, MUIOPTION_FULL_SCREEN, fullScreen, OPTION_PRIORITY_CMDLINE);
}

int GetLangcode(void)
{
	return lang_get_langcode();
}

void SetLangcode(int langcode)
{
	/* apply to emulator core for datafile.c */
	lang_set_langcode(MameUIGlobal(), langcode);
	langcode = GetLangcode();

	options_set_string(global, OPTION_LANGUAGE, ui_lang_info[langcode].name, OPTION_PRIORITY_CMDLINE);

	/* apply for osd core functions */
	set_osdcore_acp(ui_lang_info[langcode].codepage);

	InitTranslator(langcode);
}

BOOL UseLangList(void)
{
	return options_get_bool(global, OPTION_USE_LANG_LIST);
}

void SetUseLangList(BOOL is_use)
{
	options_set_bool(global, OPTION_USE_LANG_LIST, is_use, OPTION_PRIORITY_CMDLINE);

	/* apply to emulator core for datafile.c */
	lang_message_enable(UI_MSG_LIST, is_use);
	lang_message_enable(UI_MSG_MANUFACTURE, is_use);
}

const WCHAR* GetLanguageDir(void)
{
	return options_get_wstring(global, OPTION_LANGPATH);
}

void SetLanguageDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_LANGPATH, path, OPTION_PRIORITY_CMDLINE);
}

#ifdef USE_HISCORE
const WCHAR* GetHiDir(void)
{
	return options_get_wstring(global, OPTION_HISCORE_DIRECTORY);
}

void SetHiDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_HISCORE_DIRECTORY, path, OPTION_PRIORITY_CMDLINE);
}
#endif /* USE_HISCORE */

#ifdef USE_IPS
const WCHAR* GetIPSDir(void)
{
	return options_get_wstring(global, OPTION_IPSPATH);
}

void SetIPSDir(const WCHAR* path)
{
	options_set_wstring(global, OPTION_IPSPATH, path, OPTION_PRIORITY_CMDLINE);
}
#endif /* USE_IPS */

#ifdef UI_COLOR_DISPLAY
struct ui_palette_assign
{
	const char *name;
	int code;
};

static struct ui_palette_assign ui_palette_tbl[] =
{
	{ OPTION_SYSTEM_BACKGROUND,     SYSTEM_COLOR_BACKGROUND },
	{ OPTION_CURSOR_SELECTED_TEXT,  CURSOR_SELECTED_TEXT },
	{ OPTION_CURSOR_SELECTED_BG,    CURSOR_SELECTED_BG },
	{ OPTION_CURSOR_HOVER_TEXT,     CURSOR_HOVER_TEXT },
	{ OPTION_CURSOR_HOVER_BG,       CURSOR_HOVER_BG },
	{ OPTION_BUTTON_RED,            BUTTON_COLOR_RED },
	{ OPTION_BUTTON_YELLOW,         BUTTON_COLOR_YELLOW },
	{ OPTION_BUTTON_GREEN,          BUTTON_COLOR_GREEN },
	{ OPTION_BUTTON_BLUE,           BUTTON_COLOR_BLUE },
	{ OPTION_BUTTON_PURPLE,         BUTTON_COLOR_PURPLE },
	{ OPTION_BUTTON_PINK,           BUTTON_COLOR_PINK },
	{ OPTION_BUTTON_AQUA,           BUTTON_COLOR_AQUA },
	{ OPTION_BUTTON_SILVER,         BUTTON_COLOR_SILVER },
	{ OPTION_BUTTON_NAVY,           BUTTON_COLOR_NAVY },
	{ OPTION_BUTTON_LIME,           BUTTON_COLOR_LIME }
};

const char *GetUIPaletteString(int n)
{
	if (n < 0 || n >= ARRAY_LENGTH(ui_palette_tbl))
		return NULL;

	return options_get_string(global, ui_palette_tbl[n].name);
}

void SetUIPaletteString(int n, const char *s)
{
	if (n < 0 || n >= ARRAY_LENGTH(ui_palette_tbl))
		return;

	options_set_string(global, ui_palette_tbl[n].name, s, OPTION_PRIORITY_CMDLINE);
}
#endif /* UI_COLOR_DISPLAY */

BOOL FolderHasVector(const WCHAR *name)
{
	int i;

	for (i = 0; drivers[i]; i++)
		if (DriverIsVector(i) && wcscmp(name, GetDriverFilename(i)) == 0)
			return TRUE;

	return FALSE;
}

COLORREF GetListBrokenColor(void)
{
	COLORREF broken_color = (COLORREF)options_get_int(settings, MUIOPTION_BROKEN_COLOR);

	if (broken_color == (COLORREF)-1)
		return (GetSysColor(COLOR_WINDOWTEXT));

	return broken_color;
}

void SetListBrokenColor(COLORREF uColor)
{
	COLORREF broken_color = GetListBrokenColor();

	if (broken_color == GetSysColor(COLOR_WINDOWTEXT))
		broken_color = (COLORREF)-1;
	else
		broken_color = uColor;

	options_set_int(settings, MUIOPTION_BROKEN_COLOR, (int)broken_color, OPTION_PRIORITY_CMDLINE);
}

void SetUseBrokenIcon(BOOL use_broken_icon)
{
	options_set_bool(settings, MUIOPTION_USE_BROKEN_ICON, use_broken_icon, OPTION_PRIORITY_CMDLINE);
}

BOOL GetUseBrokenIcon(void)
{
	return options_get_bool(settings, MUIOPTION_USE_BROKEN_ICON);
}

#ifdef USE_SHOW_SPLASH_SCREEN
BOOL GetDisplaySplashScreen (void)
{
	return options_get_bool(settings, MUIOPTION_DISPLAY_SPLASH_SCREEN);
}

void SetDisplaySplashScreen (BOOL val)
{
	options_set_bool(settings, MUIOPTION_DISPLAY_SPLASH_SCREEN, val, OPTION_PRIORITY_CMDLINE);
}
#endif /* USE_SHOW_SPLASH_SCREEN */

#ifdef TREE_SHEET
BOOL GetShowTreeSheet(void)
{
	return options_get_bool(settings, MUIOPTION_SHOW_TREE_SHEET);
}

void SetShowTreeSheet(BOOL val)
{
	options_set_bool(settings, MUIOPTION_SHOW_TREE_SHEET, val, OPTION_PRIORITY_CMDLINE);
}
#endif /* TREE_SHEET */



#include <ctype.h>
#undef realloc
#undef malloc

typedef struct
{
	char *name;
	DWORD flags;
} f_flag_entry;

typedef struct
{
	f_flag_entry *entry;
	int           num;
} f_flag;

#define ALLOC_FOLDERFLAG	8
#define ALLOC_FOLDERS		100

static void set_folder_flag(f_flag *flag, const char *path, DWORD dwFlags)
{
	int i;

	if (flag->entry == NULL)
	{
		flag->entry = (f_flag_entry *)malloc(ALLOC_FOLDERFLAG * sizeof (*flag->entry));
		if (!flag->entry)
		{
			dprintf("error: malloc failed in set_folder_flag\n");
			return;
		}

		flag->num = ALLOC_FOLDERFLAG;
		memset(flag->entry, 0, flag->num * sizeof (*flag->entry));
	}

	for (i = 0; i < flag->num; i++)
		if (flag->entry[i].name && strcmp(flag->entry[i].name, path) == 0)
		{
			if (dwFlags == 0)
			{
				global_free(flag->entry[i].name);
				flag->entry[i].name = NULL;
			}
			else
				flag->entry[i].flags = dwFlags;

			return;
		}

	if (dwFlags == 0)
		return;

	for (i = 0; i < flag->num; i++)
		if (flag->entry[i].name == NULL)
			break;

	if (i == flag->num)
	{
		f_flag_entry *tmp;

		tmp = (f_flag_entry *)realloc(flag->entry, (flag->num + ALLOC_FOLDERFLAG) * sizeof (*tmp));
		if (!tmp)
		{
			dprintf("error: realloc failed in set_folder_flag\n");
			return;
		}

		flag->entry = tmp;
		memset(tmp + flag->num, 0, ALLOC_FOLDERFLAG * sizeof (*tmp));
		flag->num += ALLOC_FOLDERFLAG;
	}

	flag->entry[i].name = mame_strdup(path);
	flag->entry[i].flags = dwFlags;
}

static void free_folder_flag(f_flag *flag)
{
	int i;

	for (i = 0; i < flag->num; i++)
		FreeIfAllocated(&flag->entry[i].name);

	if (flag->entry)
		global_free(flag->entry);
	flag->entry = NULL;
	flag->num = 0;
}

static void options_get_folder_flag(core_options *opts, f_flag *flags, const char *name)
{
	const char *stemp = options_get_string(opts, name);

	free_folder_flag(flags);

	if (stemp == NULL)
		return;

	while (*stemp)
	{
		char buf[256];
		char *p1;
		char *p2;

		if (*stemp == ',')
			break;

		p1 = buf;
		while (*stemp != '\0' && *stemp != ',')
			*p1++ = *stemp++;
		*p1++ = '\0';

		if (*stemp++ != ',')
			return;

		while (*stemp == ' ')
			stemp++;

		if (!isdigit(*stemp))
			return;

		p2 = p1;
		while (isdigit(*stemp))
			*p2++ = *stemp++;
		*p2 = '\0';

		set_folder_flag(flags, buf, atoi(p1));

		if (*stemp++ != ',')
			return;

		while (*stemp == ' ')
			stemp++;
	}
}

static void options_set_folder_flag(core_options *opts, const char *name, const f_flag *flags, int priority)
{
	char *buf;
	int size;
	int len;
	int i;

	size = 1024;
	buf = (char *)malloc(size * sizeof (*buf));
	*buf = '\0';
	len = 0;

	for (i = 0; i < flags->num; i++)
		if (flags->entry[i].name != NULL)
		{
			DWORD dwFlags = flags->entry[i].flags;
			if (dwFlags == 0)
				continue;

			if (len + strlen(flags->entry[i].name) + 16 > size)
			{
				size += 1024;
				buf = (char *)realloc(buf, size * sizeof (*buf));
			}

			if (len)
				buf[len++] = ',';

			len += sprintf(buf + len, "%s,%ld", flags->entry[i].name, dwFlags);
		}

	options_set_string(opts, name, buf, priority);
	global_free(buf);
}

static f_flag settings_folder_flag;

/***************************************************************************
    Internal functions
 ***************************************************************************/

static void  CusColorEncodeString(const COLORREF *value, char* str)
{
	int  i;
	char tmpStr[256];

	sprintf(tmpStr, "%u", (int) value[0]);

	strcpy(str, tmpStr);

	for (i = 1; i < 16; i++)
	{
		sprintf(tmpStr, ",%u", (unsigned) value[i]);
		strcat(str, tmpStr);
	}
}

static void CusColorDecodeString(const char* str, COLORREF *value)
{
	int  i;
	char *s, *p;
	char tmpStr[256];

	strcpy(tmpStr, str);
	p = tmpStr;

	for (i = 0; p && i < 16; i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
	}
}


void ColumnEncodeStringWithCount(const int *value, char *str, int count)
{
	int  i;
	char buffer[100];

	snprintf(buffer,sizeof(buffer),"%d",value[0]);

	strcpy(str,buffer);

    for (i = 1; i < count; i++)
	{
		snprintf(buffer,sizeof(buffer),",%d",value[i]);
		strcat(str,buffer);
	}
}

void ColumnDecodeStringWithCount(const char* str, int *value, int count)
{
	int  i;
	char *s, *p;
	char tmpStr[100];

	if (str == NULL)
		return;

	strcpy(tmpStr, str);
	p = tmpStr;

    for (i = 0; p && i < count; i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
    }
	}

static void SplitterEncodeString(const int *value, char* str)
{
	int  i;
	char tmpStr[100];

	sprintf(tmpStr, "%d", value[0]);

	strcpy(str, tmpStr);

	for (i = 1; i < GetSplitterCount(); i++)
	{
		sprintf(tmpStr, ",%d", value[i]);
		strcat(str, tmpStr);
	}
}

static void SplitterDecodeString(const char *str, int *value)
{
	int  i;
	char *s, *p;
	char tmpStr[100];

	strcpy(tmpStr, str);
	p = tmpStr;

	for (i = 0; p && i < GetSplitterCount(); i++)
	{
		s = p;

		if ((p = strchr(s,',')) != NULL && *p == ',')
		{
			*p = '\0';
			p++;
		}
		value[i] = atoi(s);
	}
}

/* Parse the given comma-delimited string into a LOGFONT structure */
static void FontDecodeString(const char* str, LOGFONTW *f)
{
	const char*	ptr;
	WCHAR*		w_ptr;

	sscanf(str, "%li,%li,%li,%li,%li,%i,%i,%i,%i,%i,%i,%i,%i",
		   &f->lfHeight,
		   &f->lfWidth,
		   &f->lfEscapement,
		   &f->lfOrientation,
		   &f->lfWeight,
		   (int*)&f->lfItalic,
		   (int*)&f->lfUnderline,
		   (int*)&f->lfStrikeOut,
		   (int*)&f->lfCharSet,
		   (int*)&f->lfOutPrecision,
		   (int*)&f->lfClipPrecision,
		   (int*)&f->lfQuality,
		   (int*)&f->lfPitchAndFamily);
	ptr = strrchr(str, ',');
	if (ptr != NULL) {
		w_ptr = wstring_from_utf8(ptr + 1);
		if( !w_ptr )
			return;
		wcscpy(f->lfFaceName, w_ptr);
		osd_free(w_ptr);
	}
}

/* Encode the given LOGFONT structure into a comma-delimited string */
static void FontEncodeString(const LOGFONTW *f, char *str)
{
	//char* utf8_FaceName = utf8_from_tstring(f->lfFaceName);
	//if( !utf8_FaceName )
	//	return;

	sprintf(str, "%li,%li,%li,%li,%li,%i,%i,%i,%i,%i,%i,%i,%i",
			f->lfHeight,
			f->lfWidth,
			f->lfEscapement,
			f->lfOrientation,
			f->lfWeight,
			f->lfItalic,
			f->lfUnderline,
			f->lfStrikeOut,
			f->lfCharSet,
			f->lfOutPrecision,
			f->lfClipPrecision,
			f->lfQuality,
			f->lfPitchAndFamily);
	//		utf8_FaceName);
        //
	//osd_free(utf8_FaceName);
}

static void TabFlagsEncodeString(int data, char *str)
{
	int i;
	int num_saved = 0;

	strcpy(str,"");

	// we save the ones that are NOT displayed, so we can add new ones
	// and upgraders will see them
	for (i=0;i<MAX_TAB_TYPES;i++)
	{
		if (((data & (1 << i)) == 0) && GetImageTabShortName(i))
		{
			if (num_saved != 0)
				strcat(str, ", ");

			strcat(str,GetImageTabShortName(i));
			num_saved++;
		}
	}
}

static void TabFlagsDecodeString(const char *str, int *data)
{
	char s[2000];
	char *token;

	snprintf(s, ARRAY_LENGTH(s), "%s", str);

	// simple way to set all tab bits "on"
	*data = (1 << MAX_TAB_TYPES) - 1;

	token = strtok(s,", \t");
	while (token != NULL)
	{
		int j;

		for (j=0;j<MAX_TAB_TYPES;j++)
		{
			if (!GetImageTabShortName(j) || (strcmp(GetImageTabShortName(j), token) == 0))
			{
				// turn off this bit
				*data &= ~(1 << j);
				break;
			}
		}
		token = strtok(NULL,", \t");
	}

	if (*data == 0)
	{
		// not allowed to hide all tabs, because then why even show the area?
		*data = (1 << TAB_SCREENSHOT);
	}
}

static file_error LoadSettingsFile(core_options *opts, const char *filename)
{
	core_file *file;
	file_error filerr;

	filerr = core_fopen(filename, OPEN_FLAG_READ, &file);
	if (filerr == FILERR_NONE)
	{
		options_parse_ini_file(opts, file, OPTION_PRIORITY_CMDLINE);
		core_fclose(file);
	}
	return filerr;
}


static file_error SaveSettingsFile(core_options *opts, core_options *baseopts, const char *filename)
{
	core_file *file;
	file_error filerr;

	if ((opts != NULL) && ((baseopts == NULL) || !options_equal(opts, baseopts)))
	{
		filerr = core_fopen(filename, OPEN_FLAG_WRITE | OPEN_FLAG_CREATE | OPEN_FLAG_CREATE_PATHS, &file);
		if (filerr == FILERR_NONE)
		{
#ifdef MESS
			options_output_ini_file(opts, file);	/* required for MESS */
#else
			options_output_diff_ini_file(opts, baseopts, file);
#endif
			core_fclose(file);
		}
	}
	else
	{
		filerr = osd_rmfile(filename);
	}

	return filerr;
}



static void GetGlobalOptionsFileName(char *filename, size_t filename_size)
{
	// always in the current directory.
	snprintf(filename, filename_size, "%s", DEFAULT_OPTIONS_INI_FILENAME);
}

static void GetSettingsFileName(char *filename, size_t filename_size)
{
	// copy INI directory
	char *inidir = utf8_from_wstring(GetIniDir());
	char *s = _strdup(UI_INI_FILENAME);
	_strlwr(s);
	snprintf(filename, filename_size, "%s\\%s", inidir, s);
	global_free(inidir);
	global_free(s);
}

/* Register access functions below */
static void LoadOptionsAndSettings(void)
{
	char buffer[MAX_PATH];

	// parse global options mame.ini
	GetGlobalOptionsFileName(buffer, ARRAY_LENGTH(buffer));
	LoadSettingsFile(global, buffer);

	// parse ui/mameui.ini 
	GetSettingsFileName(buffer, ARRAY_LENGTH(buffer));
	LoadSettingsFile(settings, buffer);

	options_set_string(MameUIGlobal(), OPTION_LANGUAGE, options_get_string(global, OPTION_LANGUAGE), OPTION_PRIORITY_CMDLINE);
	GetLanguageDir();

	setup_language(MameUIGlobal());
	SetLangcode(GetLangcode());
	SetUseLangList(UseLangList());
}


DWORD LoadFolderFlags(const char *path)
{
	int i;

	options_get_folder_flag(settings, &settings_folder_flag, MUIOPTION_FOLDER_FLAG);

	if (settings_folder_flag.entry == NULL)
		return 0;

	for (i = 0; i < settings_folder_flag.num; i++)
		if (settings_folder_flag.entry[i].name
		 && strcmp(settings_folder_flag.entry[i].name, path) == 0)
			return settings_folder_flag.entry[i].flags;

	return 0;
}

void SaveFolderFlags(const char *path, DWORD flags)
{
	DWORD current = LoadFolderFlags(path);

	if (current == flags)
		return;

	set_folder_flag(&settings_folder_flag, path, flags);

	options_set_folder_flag(settings, MUIOPTION_FOLDER_FLAG, &settings_folder_flag, OPTION_PRIORITY_CMDLINE);
}



#if 0 //mamep
// Copy options, if entry doesn't exist in the dest, create it.
static void copy_options_ex(core_options *pDestOpts, core_options *pSourceOpts)
{
	options_enumerator *enumerator;
	const char *option_name;
	const char *option_value;
	options_entry entries[2];

	memcpy(entries, filterOptions, sizeof(filterOptions));

	enumerator = options_enumerator_begin(pSourceOpts);

	if (enumerator != NULL)
	{
		while((option_name = options_enumerator_next(enumerator)) != NULL)
		{
			option_value = options_get_string(pSourceOpts, option_name);
			{
				const char *existing = options_get_string(pDestOpts, option_name);
				if (NULL == existing || *existing == '\0')
				{
					entries[0].name = option_name;
					// create entry
					options_add_entries(pDestOpts, entries);
				}
				if (strcmp(option_value, existing) != 0)
					options_set_string(pDestOpts, option_name, option_value, OPTION_PRIORITY_CMDLINE);
			}
		}
		options_enumerator_free(enumerator);
	}
}
#endif

// Adds our folder flags to a temporarty core_options, for saving.
static core_options * AddFolderFlags(core_options *opts_param)
{
#if 1 //mamep: folder flags are already registered into variable settings by function SaveFolderFlags()
	return NULL;
#else
	core_options *opts;
	int numFolders;
	int i;
	LPTREEFOLDER lpFolder;
	int num_entries = 0;
	options_entry entries[2];

	opts = options_create(memory_error);

	if (NULL == opts)
	{
		return NULL;
	}

	options_add_entries(opts, regSettings);
	copy_options_ex(opts, opts_param);

	memcpy(entries, filterOptions, sizeof(filterOptions));
	entries[0].name = NULL;
	entries[0].defvalue = NULL;
	entries[0].flags = OPTION_HEADER;
	entries[0].description = "FOLDER FILTERS";
	options_add_entries(opts, entries);

	memcpy(entries, filterOptions, sizeof(filterOptions));

	numFolders = GetNumFolders();

	for (i = 0; i < numFolders; i++)
	{
		lpFolder = GetFolder(i);
		if (lpFolder && (lpFolder->m_dwFlags & F_MASK) != 0)
		{
			char folder_name[80];
			char *ptr;
			astring *option_name;

			// Convert spaces to underscores
			strcpy(folder_name, lpFolder->m_lpTitle);
			ptr = folder_name;
			while (*ptr && *ptr != '\0')
			{
				if (*ptr == ' ')
				{
					*ptr = '_';
				}
				ptr++;
			}

			option_name = astring_assemble_2(astring_alloc(), folder_name, "_filters" );

			// create entry
			entries[0].name = astring_c(option_name);
			options_add_entries(opts, entries);

			// store entry
            options_set_string(opts, astring_c(option_name), EncodeFolderFlags(lpFolder->m_dwFlags), OPTION_PRIORITY_CMDLINE);
			astring_free(option_name);

			// increment counter
			num_entries++;
		}
	}
	if (num_entries == 0) {
		options_free(opts);
		opts = NULL;
	}
	return opts;
#endif
}

// Save the UI ini
void SaveOptions(void)
{
	if (save_gui_settings)
	{
		// Add the folder flag to settings.
		char buffer[MAX_PATH];
		core_options *opts = AddFolderFlags(settings);
		// Save opts if it is non-null, else save settings.
		// It will be null if there are no filters set.
		GetSettingsFileName(buffer, ARRAY_LENGTH(buffer));
		SaveSettingsFile((opts == NULL) ? settings : opts, NULL, buffer);
		// Free up the opts allocated by AddFolderFlags.
		if (opts)
		{
			options_free(opts);
		}
	}
}


void SaveDefaultOptions(void)
{
	char buffer[MAX_PATH];
	GetGlobalOptionsFileName(buffer, ARRAY_LENGTH(buffer));
	SaveSettingsFile(global, NULL, buffer);
}

const char * GetVersionString(void)
{
	return build_version;
}


BOOL IsGlobalOption(const char *option_name)
{
	static const char *global_options[] =
	{
		OPTION_ROMPATH,
#ifdef MESS
		OPTION_HASHPATH,
#endif // MESS
		OPTION_SAMPLEPATH,
		OPTION_ARTPATH,
		OPTION_CTRLRPATH,
		OPTION_INIPATH,
		OPTION_FONTPATH,
		OPTION_CHEATPATH,
		OPTION_LANGPATH,
#ifdef USE_IPS
		OPTION_IPSPATH,
#endif /* USE_IPS */
		OPTION_CFG_DIRECTORY,
		OPTION_NVRAM_DIRECTORY,
		OPTION_MEMCARD_DIRECTORY,
		OPTION_INPUT_DIRECTORY,
		OPTION_STATE_DIRECTORY,
		OPTION_SNAPSHOT_DIRECTORY,
		OPTION_DIFF_DIRECTORY,
		OPTION_COMMENT_DIRECTORY,
#ifdef USE_HISCORE
		OPTION_HISCORE_DIRECTORY,
#endif /* USE_HISCORE */
#ifdef CMD_LIST
		OPTION_COMMAND_FILE,
#endif /* CMD_LIST */
#ifdef USE_HISCORE
		OPTION_HISCORE_FILE,
#endif /* USE_HISCORE */
#ifdef UI_COLOR_DISPLAY
		OPTION_SYSTEM_BACKGROUND,
		OPTION_CURSOR_SELECTED_TEXT,
		OPTION_CURSOR_SELECTED_BG,
		OPTION_CURSOR_HOVER_TEXT,
		OPTION_CURSOR_HOVER_BG,
		OPTION_BUTTON_RED,
		OPTION_BUTTON_YELLOW,
		OPTION_BUTTON_GREEN,
		OPTION_BUTTON_BLUE,
		OPTION_BUTTON_PURPLE,
		OPTION_BUTTON_PINK,
		OPTION_BUTTON_AQUA,
		OPTION_BUTTON_SILVER,
		OPTION_BUTTON_NAVY,
		OPTION_BUTTON_LIME,
#endif /* UI_COLOR_DISPLAY */
		OPTION_LANGUAGE,
		OPTION_USE_LANG_LIST
	};
	int i;
	char command_name[128];
	char *s;

	// determine command name
	snprintf(command_name, ARRAY_LENGTH(command_name), "%s", option_name);
	s = strchr(command_name, ';');
	if (s)
		*s = '\0';

	for (i = 0; i < ARRAY_LENGTH(global_options); i++)
	{
		if (!strcmp(command_name, global_options[i]))
			return TRUE;
	}
	return FALSE;
}


/* ui_parse_ini_file - parse a single INI file */
static void ui_parse_ini_file(core_options *opts, const char *name)
{
	astring *fname;

	/* open the file; if we fail, that's ok */
	char *inidir = utf8_from_wstring(GetIniDir());
	fname = astring_assemble_4(astring_alloc(), inidir, PATH_SEPARATOR, name, ".ini");
	global_free(inidir);
	LoadSettingsFile(opts, astring_c(fname));
	astring_free(fname);
}


/*  get options, based on passed in option level. */
core_options * load_options(OPTIONS_TYPE opt_type, int game_num)
{
	core_options *opts;
	const game_driver *driver = NULL;

	opts = CreateGameOptions(game_num);

	if (opts == NULL)
	{
		return NULL;
	}

	/* Copy over the defaults */
	options_copy(opts, global);

	if (opt_type == OPTIONS_GLOBAL)
	{
		return opts;
	}

	/* debug builds: parse "debug.ini" as well */
#ifdef MAME_DEBUG
	ui_parse_ini_file(opts, "debug");
#endif
	if (game_num >= 0)
	{
		driver = drivers[game_num];
	}

	/* if we have a valid game driver, parse game-specific INI files */
	if (driver != NULL)
	{
		const game_driver *parent = driver_get_clone(driver);
		const game_driver *gparent = (parent != NULL) ? driver_get_clone(parent) : NULL;

		astring *basename;
		astring *srcname;
		machine_config *config = global_alloc(machine_config(driver->machine_config));

		/* parse "vector.ini" for vector games */
		if (isDriverVector(config))
		{
			ui_parse_ini_file(opts, "vector");
		}
		if (opt_type == OPTIONS_VECTOR)
		{
			return opts;
		}
		/* parse "horizont.ini" for horizontal games */
		if (!DriverIsVertical(game_num))
		{
			ui_parse_ini_file(opts, "horizont");
		}
		if (opt_type == OPTIONS_HORIZONTAL)
		{
			return opts;
		}
		/* parse "vertical.ini" for vertical games */
		if (DriverIsVertical(game_num))
		{
			ui_parse_ini_file(opts, "vertical");
		}
		if (opt_type == OPTIONS_VERTICAL)
		{
			return opts;
		}
		global_free(config);


		/* then parse "<sourcefile>.ini" */
		basename = core_filename_extract_base(astring_alloc(), driver->source_file, TRUE);
		srcname = astring_assemble_3(astring_alloc(), "source", PATH_SEPARATOR, astring_c(basename));
		ui_parse_ini_file(opts, astring_c(srcname));
		astring_free(srcname);
		astring_free(basename);

		if (opt_type == OPTIONS_SOURCE)
		{
			return opts;
		}

		/* then parent the grandparent, parent, and game-specific INIs */
		if (gparent != NULL)
			ui_parse_ini_file(opts, gparent->name);
		if (parent != NULL)
			ui_parse_ini_file(opts, parent->name);

		if (opt_type == OPTIONS_PARENT)
		{
			return opts;
		}

#ifdef USE_IPS
		//mamep: DO NOT INHERIT IPS CONFIGURATION
		options_set_string(opts, OPTION_IPS, NULL, OPTION_PRIORITY_CMDLINE);
#endif /* USE_IPS */

		ui_parse_ini_file(opts, driver->name);

		if (opt_type == OPTIONS_GAME)
		{
			return opts;
		}
	}
	options_free(opts);
	return NULL;
}



/*
 * Save ini file based on game_num and passed in opt_type.  If opts are
 * NULL, the ini will be removed.
 *
 * game_num must be valid or the driver cannot be expanded and anything
 * with a higher priority than OPTIONS_VECTOR will not be saved.
 */
void save_options(OPTIONS_TYPE opt_type, core_options *opts, int game_num)
{
	core_options *baseopts = NULL;
	const game_driver *driver = NULL;
	astring *filename = NULL;

	//mamep: to remove ini file, load baseopts even if it is equals global
	if (OPTIONS_GLOBAL != opt_type && NULL != opts)
	{
		if (OPTIONS_VERTICAL == opt_type) {
			//since VERTICAL and HORIZONTAL are equally ranked
			//we need to subtract 2 from vertical to also get to global
			baseopts = load_options((OPTIONS_TYPE)(opt_type - 2), game_num);
		}
		else {
			baseopts = load_options((OPTIONS_TYPE)(opt_type - 1), game_num);
		}
	}

	if (game_num >= 0)
	{
		driver = drivers[game_num];
	}

	if (opt_type == OPTIONS_GLOBAL)
	{
		/* Don't try to save a null global options file,  or it will be erased. */
		if (NULL == opts)
			return;
		options_copy(global, opts);
		filename = astring_cpyc(astring_alloc(), CONFIGNAME);
	} else if (opt_type == OPTIONS_VECTOR)
	{
		filename = astring_cpyc(astring_alloc(), "vector");
	} else if (opt_type == OPTIONS_VERTICAL)
	{
		filename = astring_cpyc(astring_alloc(), "vertical");
	} else if (opt_type == OPTIONS_HORIZONTAL)
	{
		filename = astring_cpyc(astring_alloc(), "horizont");
	} else if (driver != NULL)
	{
		astring *basename, *srcname;

		switch (opt_type)
		{
		case OPTIONS_SOURCE:
			/* determine the <sourcefile> */
			basename = core_filename_extract_base(astring_alloc(), driver->source_file, TRUE);
			srcname = astring_assemble_3(astring_alloc(), "source", PATH_SEPARATOR, astring_c(basename));
			filename = astring_cpyc(astring_alloc(),astring_c(srcname));
			astring_free(srcname);
			astring_free(basename);
			break;
		case OPTIONS_GAME:
			filename = astring_cpyc(astring_alloc(), driver->name);
			break;
		default:
			break;
		}
	}
	if (filename != NULL)
	{
		astring *filepath = NULL;
		if (opt_type == OPTIONS_GLOBAL)
		{
			char buffer[MAX_PATH];

			GetGlobalOptionsFileName(buffer, ARRAY_LENGTH(buffer));
			filepath = astring_cpyc(astring_alloc(), buffer);
		}
		else
		{
			char *inidir = utf8_from_wstring(GetIniDir());
			filepath = astring_assemble_4(astring_alloc(), inidir, PATH_SEPARATOR, astring_c(filename), ".ini");
			global_free(inidir);
		}
		astring_free(filename);

		SaveSettingsFile(opts, baseopts, astring_c(filepath));
		astring_free(filepath);
	}
	if (baseopts != NULL)
	{
		options_free(baseopts);
	}
}

/* Remove all the srcname.ini files from inipath/source directory. */
static void remove_all_source_options(void) {
	WIN32_FIND_DATA findFileData;
	HANDLE hFindFile;
	astring *pathname, *match;
	char* utf8_filename;

	/*
	 * Easiest to just open the ini/source folder if it exists,
	 * then remove all the files in it that end in ini.
	 */
	char *inidir = utf8_from_wstring(GetIniDir());
	pathname = astring_assemble_3(astring_alloc(), inidir, PATH_SEPARATOR, "source");
	global_free(inidir);
	match = astring_assemble_3(astring_alloc(), astring_c(pathname), PATH_SEPARATOR, "*.ini");
	if ((hFindFile = win_find_first_file_utf8(astring_c(match), &findFileData)) != INVALID_HANDLE_VALUE)
	{
		astring_free(match);
		utf8_filename = utf8_from_tstring(findFileData.cFileName);
		match = astring_assemble_3(astring_alloc(), astring_c(pathname), PATH_SEPARATOR, utf8_filename );
		osd_free(utf8_filename);
		osd_rmfile(astring_c(match));
		astring_free(match);

		while (0 != FindNextFile(hFindFile, &findFileData))
		{
			utf8_filename = utf8_from_tstring(findFileData.cFileName);
			match = astring_assemble_3(astring_alloc(), astring_c(pathname), PATH_SEPARATOR, utf8_filename );
			osd_free(utf8_filename);
			osd_rmfile(astring_c(match));
			astring_free(match);
		}

		FindClose(hFindFile);

	}
	else
	{
		astring_free(match);
	}
	astring_free(pathname);

}

// Reset the given core_options to their default settings.
static void ResetToDefaults(core_options *opts, int priority)
{
	// iterate through the options setting each one back to the default value.
	options_revert(opts, priority);
}


#include "strconv.h"

WCHAR *options_get_wstring(core_options *opts, const char *name)
{
	const char *stemp = options_get_string(opts, name);

	if (stemp == NULL)
		return NULL;

	return wstring_from_utf8(stemp);
}

void options_set_wstring(core_options *opts, const char *name, const WCHAR *value, int priority)
{
	char *utf8_value = NULL;

	if (value)
		utf8_value = utf8_from_wstring(value);

	options_set_string(opts, name, utf8_value, priority);

	global_free(utf8_value);
}



