
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "xwin.h"
#include "dxwrap.h"
#include "win32.h"
#include "win32gfx.h"
#include "gfxfilter.h"

struct uae_filter uaefilters[] =
{
    { UAE_FILTER_NULL, "Null filter", "null",
    { 0, UAE_FILTER_MODE_16_16 | UAE_FILTER_MODE_32_32, 0, 0, 0 } },

    { UAE_FILTER_DIRECT3D, "Direct3D", "direct3d", 1, 0, 0, 0, 0 },

    { UAE_FILTER_OPENGL, "OpenGL", "opengl", 1, 0, 0, 0, 0 },

    { UAE_FILTER_SCALE2X, "Scale2X", "scale2x", 0, 0, UAE_FILTER_MODE_16_16 | UAE_FILTER_MODE_32_32, 0, 0 },

//    { UAE_FILTER_HQ, "hq", "hq", 0, 0, UAE_FILTER_MODE_16_32, UAE_FILTER_MODE_16_32, UAE_FILTER_MODE_16_32 },

    { UAE_FILTER_SUPEREAGLE, "SuperEagle", "supereagle", 0, 0, UAE_FILTER_MODE_16_16, 0, 0 },

    { UAE_FILTER_SUPER2XSAI, "Super2xSaI", "super2xsai", 0, 0, UAE_FILTER_MODE_16_16, 0, 0 },

    { UAE_FILTER_2XSAI, "2xSaI", "2xsai", 0, 0, UAE_FILTER_MODE_16_16, 0, 0 },


    { 0 }
};


static int dst_width, dst_height, amiga_width, amiga_height, amiga_depth, dst_depth, scale;
uae_u8 *bufmem_ptr;
int bufmem_width, bufmem_height;

void S2X_configure (int rb, int gb, int bb, int rs, int gs, int bs)
{
    Init_2xSaI (rb, gb, bb, rs, gs, bs);
    hq_init (rb, gb, bb, rs, gs, bs);
    bufmem_ptr = 0;
}

void S2X_init (int dw, int dh, int aw, int ah, int mult, int ad, int dd)
{
    int flags;

    flags = usedfilter->x[mult];
    if (mult) {
	if ((ad == 16 && !(flags & UAE_FILTER_MODE_16)) || (ad == 32 && !(flags & UAE_FILTER_MODE_32))) {
	    usedfilter = &uaefilters[0];
	    mult = 1;
	    changed_prefs.gfx_filter = usedfilter->type;
	}
    }
    dst_width = dw;
    dst_height = dh;
    dst_depth = dd;
    amiga_width = aw;
    amiga_height = ah;
    amiga_depth = ad;
    scale = mult;
}

void S2X_render (void)
{
    int aw = amiga_width, ah = amiga_height, v, pitch;
    uae_u8 *dptr, *sptr;
    int ok = 0;

    sptr = gfxvidinfo.bufmem;

    v = currprefs.gfx_filter_horiz_offset;
    v += (dst_width / scale - amiga_width) / 8;
    sptr += -v * (amiga_depth / 8) * 4;
    aw -= -v * 4;

    v = currprefs.gfx_filter_vert_offset;
    v += (dst_height / scale - amiga_height) / 8;
    sptr += -v * gfxvidinfo.rowbytes * 4;
    ah -= -v * 4;

    if (aw * scale > dst_width)
        aw = (dst_width / scale) & ~3;
    if (ah * scale > dst_height)
        ah = (dst_height / scale) & ~3;

    if (ah < 16)
	return;
    if (aw < 16)
	return;

    bufmem_ptr = sptr;
    bufmem_width = aw;
    bufmem_height = ah;

    if (!DirectDraw_SurfaceLock (lockable_surface))
    	return;

    dptr = DirectDraw_GetSurfacePointer ();
    pitch = DirectDraw_GetSurfacePitch();

    if (usedfilter->type == UAE_FILTER_SCALE2X ) { /* 16+32/2X */

        if (amiga_depth == 16 && dst_depth == 16) {
	    AdMame2x (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    ok = 1;
	} else if (amiga_depth == 32 && dst_depth == 32) {
	    AdMame2x32 (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    ok = 1;
	}

    } else if (usedfilter->type == UAE_FILTER_HQ) { /* 32/2X+3X+4X */

        int hqsrcpitch = gfxvidinfo.rowbytes - aw * amiga_depth / 8;
	int hqdstpitch = pitch - aw * scale * dst_depth / 8;
	int hqdstpitch2 = pitch * scale - aw * scale * dst_depth / 8;
	if (scale == 2) {
	    if (amiga_depth == 16 && dst_depth == 32) {
		hq2x_32 (sptr, dptr, aw, ah, hqdstpitch, hqsrcpitch, hqdstpitch2);
		ok = 1;
	    }
	} else if (scale == 3) {
	    if (amiga_depth == 16 && dst_depth == 16) {
		hq3x_16 (sptr, dptr, aw, ah, hqdstpitch, hqsrcpitch, hqdstpitch2);
		ok = 1;
	    } else if (amiga_depth == 16 && dst_depth == 32) {
	    	hq3x_32 (sptr, dptr, aw, ah, hqdstpitch, hqsrcpitch, hqdstpitch2);
		ok = 1;
	    }
	} else if (scale == 4) {
	    if (amiga_depth == 16 && dst_depth == 32) {
		hq4x_32 (sptr, dptr, aw, ah, hqdstpitch, hqsrcpitch, hqdstpitch2);
		ok = 1;
	    }
	}

    } else if (usedfilter->type == UAE_FILTER_SUPEREAGLE) { /* 16/2X */

	if (scale == 2 && amiga_depth == 16 && dst_depth == 16) {
	    SuperEagle (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    ok = 1;
	}

    } else if (usedfilter->type == UAE_FILTER_SUPER2XSAI) { /* 16/2X */

	if (scale == 2 && amiga_depth == 16 && dst_depth == 16) {
    	    Super2xSaI (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    ok = 1;
	}

    } else if (usedfilter->type == UAE_FILTER_2XSAI) { /* 16/2X */

	if (scale == 2 && amiga_depth == 16 && dst_depth == 16) {
	    _2xSaI (sptr, gfxvidinfo.rowbytes, dptr, pitch, aw, ah);
	    ok = 1;
	}

    } else { /* null */

	if (amiga_depth == dst_depth) {
	    int y;
	    for (y = 0; y < dst_height; y++) {
		memcpy (dptr, sptr, dst_width * dst_depth / 8);
		sptr += gfxvidinfo.rowbytes;
		dptr += pitch;
	    }
	}
	ok = 1;

    }

    if (ok == 0) {
        usedfilter = &uaefilters[0];
        changed_prefs.gfx_filter = usedfilter->type;
    }

    DirectDraw_SurfaceUnlock ();

}

void S2X_refresh (void)
{
    int y, pitch;
    uae_u8 *dptr;

    if (!DirectDraw_SurfaceLock (lockable_surface))
    	return;
    dptr = DirectDraw_GetSurfacePointer ();
    pitch = DirectDraw_GetSurfacePitch();
    for (y = 0; y < dst_height; y++)
	memset (dptr + y * pitch, 0, dst_width * dst_depth / 8);
    DirectDraw_SurfaceUnlock ();
    S2X_render ();
}
