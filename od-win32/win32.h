/* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Win32-specific header file
  * 
  * (c) 1997-1999 Mathias Ortmann
  * (c) 1998-2001 Brian King
  */

#ifndef __WIN32_H__
#define __WIN32_H__

#define IHF_WINDOWHIDDEN 6
#define NORMAL_WINDOW_STYLE (WS_VISIBLE | WS_BORDER | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU )

extern HMODULE hUIDLL;
extern HWND hAmigaWnd, hMainWnd, hHiddenWnd;
extern RECT amigawin_rect;
extern int in_sizemove;
extern int manual_painting_needed;
extern int manual_palette_refresh_needed;
extern int mouseactive, focus;
extern int ignore_messages_all;
#define WINUAEBETA 0
#define WINUAEBETASTR ""

extern char start_path_exe[MAX_DPATH];
extern char start_path_data[MAX_DPATH];

extern void my_kbd_handler (int, int, int);
extern void clearallkeys(void);
extern int getcapslock (void);

void releasecapture (void);
int WIN32_RegisterClasses( void );
int WIN32_InitHtmlHelp( void );
int WIN32_InitLibraries( void );
int WIN32_CleanupLibraries( void );
void WIN32_MouseDefaults( int, int );
void WIN32_HandleRegistryStuff( void );
extern void setup_brkhandler (void);
extern void remove_brkhandler (void);
extern void disablecapture (void);
extern void fullscreentoggle (void);

extern void setmouseactive (int active);
extern void minimizewindow (void);
extern uae_u32 OSDEP_minimize_uae(void);

void finishjob (void);
void updatedisplayarea (void);
void init_colors (void);

extern int pause_emulation;
extern int sound_available;
extern int framecnt;
extern char prtname[];
extern char VersionStr[256];
extern int os_winnt, os_winnt_admin, os_64bit;
extern int paraport_mask;
extern int gui_active;
extern DWORD quickstart;

extern HKEY hWinUAEKey;
extern int screen_is_picasso;
extern HINSTANCE hInst;
extern int win_x_diff, win_y_diff;
extern int af_path_2005, af_path_old, winuae_path;
extern char start_path_af[MAX_DPATH];

extern void sleep_millis (int ms);
extern void sleep_millis_busy (int ms);
extern void wait_keyrelease (void);
extern void keyboard_settrans (void);

extern void handle_rawinput (LPARAM lParam);

#define DEFAULT_PRIORITY 2
struct threadpriorities {
    char *name;
    int value;
    int classvalue;
    int id;
};
extern struct threadpriorities priorities[];
extern void setpriority (struct threadpriorities *pri);

extern int dinput_wmkey (uae_u32 key);
extern int dinput_winmouse (void);
extern int dinput_winmousemode (void);

void systray (HWND hwnd, int remove);
void systraymenu (HWND hwnd);
void exit_gui (int);
void fetch_path (char *name, char *out, int size);
void set_path (char *name, char *path);
void read_rom_list (void);

#define WIN32_PLUGINDIR "plugins\\"
HMODULE WIN32_LoadLibrary (const char *);

extern int screenshot_prepare(void);
extern void screenshot_free(void);

struct winuae_lang
{
    WORD id;
    char *name;
};
extern struct winuae_lang langs[];
extern HMODULE language_load(WORD language);

extern void logging_open(int,int);
extern void logging_cleanup(void);

#endif