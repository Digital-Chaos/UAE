#ifndef WIN32GUI_H
#define WIN32GUI_H

#define CFG_DESCRIPTION_LENGTH 128

#define CONFIG_SAVE   0
#define CONFIG_LOAD   1
#define CONFIG_SAVE_FULL 2
#define CONFIG_LOAD_FULL 3
#define CONFIG_DELETE 4

void WIN32GUI_LoadUIString( DWORD id, char *string, DWORD dwStringLen );
extern int GetSettings (int all_options, HWND);
extern int DiskSelection( HWND hDlg, WPARAM wParam, int flag, struct uae_prefs *prefs, char *);
void InitializeListView( HWND hDlg );
extern void pre_gui_message (const char*,...);
extern void gui_message_id (int id);
int dragdrop (HWND hDlg, HDROP hd, struct uae_prefs *prefs, int currentpage);
UAEREG *read_disk_history (void);
void write_disk_history (void);

struct newresource
{
    void *resource;
    HINSTANCE inst;
    int size;
    int tmpl;
    int width, height;
};

extern struct newresource *scaleresource(struct newresource *res, HWND);
extern void freescaleresource(struct newresource*);
extern void scaleresource_setmaxsize(int w, int h);
extern HWND CustomCreateDialog (int templ, HWND hDlg, DLGPROC proc);
extern INT_PTR CustomDialogBox(int templ, HWND hDlg, DLGPROC proc);
extern struct newresource *getresource(int tmpl);
extern struct newresource *resourcefont(struct newresource*, char *font, int size);

#endif
