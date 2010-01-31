#ifndef __DXWRAP_H__
#define __DXWRAP_H__

#include "rtgmodes.h"
#include "ddraw.h"

extern int ddforceram;
extern int useoverlay;

struct ddstuff
{
    int ddinit;
    int ddzeroguid;
    GUID ddguid;
    LPDIRECTDRAW7 maindd;
    LPDIRECTDRAWCLIPPER dclip;
    LPDIRECTDRAWSURFACE7 primary, secondary, flipping[2];
    LPDIRECTDRAWPALETTE palette;
    DDOVERLAYFX overlayfx;
    DWORD overlayflags;
    int fsmodeset, backbuffers;
    int width, height, depth, freq;
    int swidth, sheight;
    DDSURFACEDESC2 native;
    DDSURFACEDESC2 locksurface;
    int lockcnt;
    DWORD pitch;
    HWND hwnd;
    uae_u32 colorkey;
    int islost, isoverlay;

    LPDIRECTDRAWSURFACE7 cursorsurface1;
    LPDIRECTDRAWSURFACE7 cursorsurface2;
    LPDIRECTDRAWSURFACE7 statussurface;
    int statuswidth, statusheight;

};
struct ddcaps
{
    int cursorwidth, cursorheight;
    int maxwidth, maxheight;
    int cancolorkey;
    int cannonlocalvidmem;
};
extern struct ddstuff dxdata;
extern struct ddcaps dxcaps;

struct ScreenResolution
{
    uae_u32 width;  /* in pixels */
    uae_u32 height; /* in pixels */
};

#define MAX_PICASSO_MODES 300
#define MAX_REFRESH_RATES 100
struct PicassoResolution
{
    struct ScreenResolution res;
    int depth;   /* depth in bytes-per-pixel */
    int residx;
    int refresh[MAX_REFRESH_RATES]; /* refresh-rates in Hz */
    char name[25];
    /* Bit mask of RGBFF_xxx values.  */
    uae_u32 colormodes;
};
extern GUID *displayGUID;

#define MAX_DISPLAYS 10
struct MultiDisplay {
    int primary, disabled, gdi;
    GUID guid;
    char *name;
    char *name2;
    struct PicassoResolution *DisplayModes;
    RECT rect;
};
extern struct MultiDisplay Displays[MAX_DISPLAYS];

typedef enum
{
    red_mask,
    green_mask,
    blue_mask
} DirectDraw_Mask_e;

extern const char *DXError (HRESULT hr);
extern char *outGUID (const GUID *guid);

HRESULT DirectDraw_GetDisplayMode (void);
void DirectDraw_Release(void);
int DirectDraw_Start(GUID *guid);
void clearsurface(LPDIRECTDRAWSURFACE7 surf);
int locksurface (LPDIRECTDRAWSURFACE7 surf, LPDDSURFACEDESC2 desc);
void unlocksurface (LPDIRECTDRAWSURFACE7 surf);
HRESULT restoresurface (LPDIRECTDRAWSURFACE7 surf);
LPDIRECTDRAWSURFACE7 allocsurface (int width, int height);
LPDIRECTDRAWSURFACE7 allocsystemsurface (int width, int height);
LPDIRECTDRAWSURFACE7 createsurface (uae_u8 *ptr, int pitch, int width, int height);
void freesurface (LPDIRECTDRAWSURFACE7 surf);
void DirectDraw_FreeMainSurface (void);
HRESULT DirectDraw_CreateMainSurface (int width, int height);
HRESULT DirectDraw_SetDisplayMode(int width, int height, int bits, int freq);
HRESULT DirectDraw_SetCooperativeLevel (HWND window, int fullscreen, int doset);
HRESULT DirectDraw_CreateClipper (void);
HRESULT DirectDraw_SetClipper(HWND hWnd);
RGBFTYPE DirectDraw_GetSurfacePixelFormat(LPDDSURFACEDESC2 surface);
HRESULT DirectDraw_EnumDisplayModes(DWORD flags, LPDDENUMMODESCALLBACK2 callback, void *context);
HRESULT DirectDraw_EnumDisplays(LPDDENUMCALLBACKEX callback);
DWORD DirectDraw_CurrentWidth (void);
DWORD DirectDraw_CurrentHeight (void);
DWORD DirectDraw_GetCurrentDepth (void);
int DirectDraw_SurfaceLock (void);
void DirectDraw_SurfaceUnlock (void);
void *DirectDraw_GetSurfacePointer (void);
DWORD DirectDraw_GetSurfacePitch (void);
int DirectDraw_IsLocked (void);
DWORD DirectDraw_GetPixelFormatBitMask (DirectDraw_Mask_e mask);
DWORD DirectDraw_GetPixelFormat (void);
DWORD DirectDraw_GetBytesPerPixel (void);
HRESULT DirectDraw_GetDC(HDC *hdc);
HRESULT DirectDraw_ReleaseDC(HDC hdc);
int DirectDraw_GetVerticalBlankStatus (void);
DWORD DirectDraw_CurrentRefreshRate (void);
void DirectDraw_GetPrimaryPixelFormat (DDSURFACEDESC2 *desc);
HRESULT DirectDraw_FlipToGDISurface (void);
int DirectDraw_Flip (int doflip);
int DirectDraw_BlitToPrimary (RECT *rect);
int DirectDraw_BlitToPrimaryScale (RECT *dstrect, RECT *srcrect);
int DirectDraw_Blit (LPDIRECTDRAWSURFACE7 dst, LPDIRECTDRAWSURFACE7 src);
int DirectDraw_BlitRect (LPDIRECTDRAWSURFACE7 dst, RECT *dstrect, LPDIRECTDRAWSURFACE7 src, RECT *scrrect);
int DirectDraw_BlitRectCK (LPDIRECTDRAWSURFACE7 dst, RECT *dstrect, LPDIRECTDRAWSURFACE7 src, RECT *scrrect);
void DirectDraw_Fill (RECT *rect, uae_u32 color);
void DirectDraw_FillPrimary (void);

HRESULT DirectDraw_SetPaletteEntries (int start, int count, PALETTEENTRY *palette);
HRESULT DirectDraw_SetPalette (int remove);
HRESULT DirectDraw_CreatePalette (LPPALETTEENTRY pal);

void dx_check (void);
int dx_islost (void);

#define DDFORCED_NONLOCAL 0
#define DDFORCED_DEFAULT 1
#define DDFORCED_VIDMEM 2
#define DDFORCED_SYSMEM 3

#endif

